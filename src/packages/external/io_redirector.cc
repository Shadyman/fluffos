#include "io_redirector.h"
#include "external.h"
#include "event_notifier.h"
#include "base/package_api.h"
#include "base/internal/log.h"
#include "packages/sockets/socket_option_manager.h"
#include "packages/sockets/socket_efuns.h"

#include <cerrno>
#include <cstring>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/select.h>
#include <algorithm>
#include <sstream>

#ifndef _WIN32
#include <poll.h>
#include <sys/wait.h>
#endif

// Constants for I/O redirection
static const size_t DEFAULT_IO_BUFFER_SIZE = 4096;
static const size_t MAX_IO_BUFFER_SIZE = 64 * 1024;  // 64KB max
static const int DEFAULT_IO_TIMEOUT_MS = 100;
static const int MAX_CONCURRENT_IOS = 100;

// Static member initialization
std::unique_ptr<IORedirector> IORedirector::global_instance_ = nullptr;
std::map<int, bool> ProcessIOMonitor::monitored_sockets_;

/*
 * IORedirector Implementation
 */

IORedirector::IORedirector() {
    // Initialize with empty socket configurations
}

IORedirector::~IORedirector() {
    // Cleanup all active redirections
    for (auto& pair : socket_configs_) {
        cleanup_redirection(pair.first);
    }
    socket_configs_.clear();
}

bool IORedirector::configure_stdio(int socket_fd, IOStreamType stream_type, 
                                  IORedirectMode mode, const std::string& file_path) {
    if (socket_fd < 0) {
        debug_message("Invalid socket fd for I/O redirection: %d", socket_fd);
        return false;
    }
    
    // Get or create config for this socket
    auto it = socket_configs_.find(socket_fd);
    if (it == socket_configs_.end()) {
        socket_configs_[socket_fd] = std::make_unique<ProcessIOConfig>();
        socket_configs_[socket_fd]->socket_fd = socket_fd;
    }
    
    ProcessIOConfig* config = socket_configs_[socket_fd].get();
    IOStreamConfig* stream_config = nullptr;
    
    // Select the appropriate stream configuration
    switch (stream_type) {
        case IOStreamType::STDIN:
            stream_config = &config->stdin_config;
            break;
        case IOStreamType::STDOUT:
            stream_config = &config->stdout_config;
            break;
        case IOStreamType::STDERR:
            stream_config = &config->stderr_config;
            break;
        default:
            debug_message("Invalid stream type for socket %d", socket_fd);
            return false;
    }
    
    // Configure the stream
    stream_config->mode = mode;
    stream_config->file_path = file_path;
    stream_config->non_blocking = true;
    stream_config->buffer_size = DEFAULT_IO_BUFFER_SIZE;
    
    // Setup redirection based on mode
    bool success = false;
    switch (mode) {
        case IORedirectMode::PIPE:
            success = setup_pipe_redirection(*stream_config, stream_type);
            break;
        case IORedirectMode::FILE:
            success = setup_file_redirection(*stream_config, stream_type, file_path);
            break;
        case IORedirectMode::NULL_DEV:
            success = setup_null_redirection(*stream_config, stream_type);
            break;
        case IORedirectMode::CONSOLE:
        case IORedirectMode::INHERIT:
            // These modes don't require special setup
            success = true;
            break;
        case IORedirectMode::MERGE:
            // Only valid for stderr - merge into stdout
            if (stream_type == IOStreamType::STDERR) {
                success = true;
            } else {
                debug_message("MERGE mode only valid for stderr");
                success = false;
            }
            break;
        default:
            debug_message("Unsupported redirection mode: %d", static_cast<int>(mode));
            success = false;
    }
    
    if (success) {
        debug_message("Configured %s redirection to mode %d for socket %d", 
                     (stream_type == IOStreamType::STDIN ? "stdin" :
                      stream_type == IOStreamType::STDOUT ? "stdout" : "stderr"),
                     static_cast<int>(mode), socket_fd);
    }
    
    return success;
}

bool IORedirector::apply_redirection(int socket_fd, pid_t pid) {
    auto it = socket_configs_.find(socket_fd);
    if (it == socket_configs_.end()) {
        debug_message("No I/O configuration found for socket %d", socket_fd);
        return false;
    }
    
    ProcessIOConfig* config = it->second.get();
    config->pid = pid;
    
    // Apply redirection for each configured stream
    bool success = true;
    
    if (config->stdin_config.mode != IORedirectMode::INHERIT) {
        success &= apply_stream_redirection(config->stdin_config, IOStreamType::STDIN);
    }
    
    if (config->stdout_config.mode != IORedirectMode::INHERIT) {
        success &= apply_stream_redirection(config->stdout_config, IOStreamType::STDOUT);
    }
    
    if (config->stderr_config.mode != IORedirectMode::INHERIT) {
        success &= apply_stream_redirection(config->stderr_config, IOStreamType::STDERR);
    }
    
    config->is_redirected = success;
    
    if (success) {
        debug_message("Applied I/O redirection for socket %d, pid %d", socket_fd, pid);
    } else {
        debug_message("Failed to apply I/O redirection for socket %d", socket_fd);
    }
    
    return success;
}

void IORedirector::cleanup_redirection(int socket_fd) {
    auto it = socket_configs_.find(socket_fd);
    if (it == socket_configs_.end()) {
        return;
    }
    
    ProcessIOConfig* config = it->second.get();
    
    // Close all pipe file descriptors
    if (config->stdin_config.pipe_read_fd != -1) {
        close(config->stdin_config.pipe_read_fd);
    }
    if (config->stdin_config.pipe_write_fd != -1) {
        close(config->stdin_config.pipe_write_fd);
    }
    if (config->stdout_config.pipe_read_fd != -1) {
        close(config->stdout_config.pipe_read_fd);
    }
    if (config->stdout_config.pipe_write_fd != -1) {
        close(config->stdout_config.pipe_write_fd);
    }
    if (config->stderr_config.pipe_read_fd != -1) {
        close(config->stderr_config.pipe_read_fd);
    }
    if (config->stderr_config.pipe_write_fd != -1) {
        close(config->stderr_config.pipe_write_fd);
    }
    
    // Close other file descriptors
    cleanup_file_descriptor(config->stdin_config.fd);
    cleanup_file_descriptor(config->stdout_config.fd);
    cleanup_file_descriptor(config->stderr_config.fd);
    
    // Remove configuration
    socket_configs_.erase(it);
    
    debug_message("Cleaned up I/O redirection for socket %d", socket_fd);
}

IOResult IORedirector::write_to_stdin(int socket_fd, const char* data, size_t length) {
    IOResult result;
    
    auto it = socket_configs_.find(socket_fd);
    if (it == socket_configs_.end()) {
        result.error_message = "No I/O configuration for socket";
        return result;
    }
    
    ProcessIOConfig* config = it->second.get();
    IOStreamConfig& stdin_config = config->stdin_config;
    
    if (stdin_config.mode != IORedirectMode::PIPE) {
        result.error_message = "Stdin not configured for pipe mode";
        return result;
    }
    
    if (stdin_config.pipe_write_fd == -1) {
        result.error_message = "Stdin pipe not available";
        return result;
    }
    
#ifndef _WIN32
    // Check if pipe is ready for writing
    if (!is_fd_ready_for_write(stdin_config.pipe_write_fd)) {
        result.would_block = true;
        return result;
    }
    
    ssize_t bytes_written = write(stdin_config.pipe_write_fd, data, length);
    
    if (bytes_written == -1) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            result.would_block = true;
        } else {
            result.error_message = "Write to stdin failed: ";
            result.error_message += strerror(errno);
        }
        return result;
    }
    
    result.success = true;
    result.bytes_processed = bytes_written;
    
    // Signal async event if configured
    ProcessIOMonitor::signal_stdin_write_complete(socket_fd, bytes_written);
    
    debug_message("Wrote %zu bytes to stdin for socket %d", bytes_written, socket_fd);
#endif
    
    return result;
}

IOResult IORedirector::read_from_stdout(int socket_fd, char* buffer, size_t max_length) {
    IOResult result;
    
    auto it = socket_configs_.find(socket_fd);
    if (it == socket_configs_.end()) {
        result.error_message = "No I/O configuration for socket";
        return result;
    }
    
    ProcessIOConfig* config = it->second.get();
    IOStreamConfig& stdout_config = config->stdout_config;
    
    if (stdout_config.mode != IORedirectMode::PIPE) {
        result.error_message = "Stdout not configured for pipe mode";
        return result;
    }
    
    if (stdout_config.pipe_read_fd == -1) {
        result.error_message = "Stdout pipe not available";
        return result;
    }
    
#ifndef _WIN32
    // Check if data is available
    if (!is_fd_ready_for_read(stdout_config.pipe_read_fd)) {
        result.would_block = true;
        return result;
    }
    
    ssize_t bytes_read = read(stdout_config.pipe_read_fd, buffer, max_length);
    
    if (bytes_read == -1) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            result.would_block = true;
        } else {
            result.error_message = "Read from stdout failed: ";
            result.error_message += strerror(errno);
        }
        return result;
    }
    
    if (bytes_read == 0) {
        result.error_message = "Stdout pipe closed";
        return result;
    }
    
    result.success = true;
    result.bytes_processed = bytes_read;
    
    debug_message("Read %zu bytes from stdout for socket %d", bytes_read, socket_fd);
#endif
    
    return result;
}

IOResult IORedirector::read_from_stderr(int socket_fd, char* buffer, size_t max_length) {
    IOResult result;
    
    auto it = socket_configs_.find(socket_fd);
    if (it == socket_configs_.end()) {
        result.error_message = "No I/O configuration for socket";
        return result;
    }
    
    ProcessIOConfig* config = it->second.get();
    IOStreamConfig& stderr_config = config->stderr_config;
    
    if (stderr_config.mode == IORedirectMode::MERGE) {
        // Stderr is merged into stdout, read from stdout instead
        return read_from_stdout(socket_fd, buffer, max_length);
    }
    
    if (stderr_config.mode != IORedirectMode::PIPE) {
        result.error_message = "Stderr not configured for pipe mode";
        return result;
    }
    
    if (stderr_config.pipe_read_fd == -1) {
        result.error_message = "Stderr pipe not available";
        return result;
    }
    
#ifndef _WIN32
    // Check if data is available
    if (!is_fd_ready_for_read(stderr_config.pipe_read_fd)) {
        result.would_block = true;
        return result;
    }
    
    ssize_t bytes_read = read(stderr_config.pipe_read_fd, buffer, max_length);
    
    if (bytes_read == -1) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            result.would_block = true;
        } else {
            result.error_message = "Read from stderr failed: ";
            result.error_message += strerror(errno);
        }
        return result;
    }
    
    if (bytes_read == 0) {
        result.error_message = "Stderr pipe closed";
        return result;
    }
    
    result.success = true;
    result.bytes_processed = bytes_read;
    
    debug_message("Read %zu bytes from stderr for socket %d", bytes_read, socket_fd);
#endif
    
    return result;
}

bool IORedirector::setup_pipe_redirection(IOStreamConfig& config, IOStreamType stream_type) {
#ifndef _WIN32
    int pipe_fds[2];
    
    if (pipe(pipe_fds) == -1) {
        debug_message("Failed to create pipe: %s", strerror(errno));
        return false;
    }
    
    // For stdin: parent writes to pipe_fds[1], child reads from pipe_fds[0]
    // For stdout/stderr: child writes to pipe_fds[1], parent reads from pipe_fds[0]
    
    if (stream_type == IOStreamType::STDIN) {
        config.pipe_read_fd = pipe_fds[0];   // Child reads from this
        config.pipe_write_fd = pipe_fds[1];  // Parent writes to this
        config.fd = pipe_fds[0];             // Child's stdin
    } else {
        config.pipe_read_fd = pipe_fds[0];   // Parent reads from this
        config.pipe_write_fd = pipe_fds[1];  // Child writes to this
        config.fd = pipe_fds[1];             // Child's stdout/stderr
    }
    
    // Set non-blocking mode on parent's end
    int parent_fd = (stream_type == IOStreamType::STDIN) ? config.pipe_write_fd : config.pipe_read_fd;
    if (!set_non_blocking(parent_fd)) {
        close(pipe_fds[0]);
        close(pipe_fds[1]);
        return false;
    }
    
    return true;
#else
    return false;
#endif
}

bool IORedirector::setup_file_redirection(IOStreamConfig& config, IOStreamType stream_type, 
                                          const std::string& file_path) {
#ifndef _WIN32
    int flags;
    int mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH; // 644 permissions
    
    switch (stream_type) {
        case IOStreamType::STDIN:
            flags = O_RDONLY;
            break;
        case IOStreamType::STDOUT:
        case IOStreamType::STDERR:
            flags = O_WRONLY | O_CREAT | O_TRUNC;
            break;
        default:
            return false;
    }
    
    int fd = open(file_path.c_str(), flags, mode);
    if (fd == -1) {
        debug_message("Failed to open file '%s': %s", file_path.c_str(), strerror(errno));
        return false;
    }
    
    config.fd = fd;
    return true;
#else
    return false;
#endif
}

bool IORedirector::setup_null_redirection(IOStreamConfig& config, IOStreamType stream_type) {
#ifndef _WIN32
    int flags = (stream_type == IOStreamType::STDIN) ? O_RDONLY : O_WRONLY;
    
    int fd = open("/dev/null", flags);
    if (fd == -1) {
        debug_message("Failed to open /dev/null: %s", strerror(errno));
        return false;
    }
    
    config.fd = fd;
    return true;
#else
    return false;
#endif
}

bool IORedirector::apply_stream_redirection(const IOStreamConfig& config, IOStreamType stream_type) {
#ifndef _WIN32
    int target_fd;
    
    switch (stream_type) {
        case IOStreamType::STDIN:
            target_fd = STDIN_FILENO;
            break;
        case IOStreamType::STDOUT:
            target_fd = STDOUT_FILENO;
            break;
        case IOStreamType::STDERR:
            if (config.mode == IORedirectMode::MERGE) {
                // Duplicate stdout to stderr
                if (dup2(STDOUT_FILENO, STDERR_FILENO) == -1) {
                    debug_message("Failed to merge stderr to stdout: %s", strerror(errno));
                    return false;
                }
                return true;
            } else {
                target_fd = STDERR_FILENO;
            }
            break;
        default:
            return false;
    }
    
    if (config.fd != -1) {
        if (dup2(config.fd, target_fd) == -1) {
            debug_message("Failed to redirect stream: %s", strerror(errno));
            return false;
        }
    }
    
    return true;
#else
    return false;
#endif
}

bool IORedirector::is_fd_ready_for_read(int fd, int timeout_ms) const {
#ifndef _WIN32
    struct pollfd pfd;
    pfd.fd = fd;
    pfd.events = POLLIN;
    
    int result = poll(&pfd, 1, timeout_ms);
    return result > 0 && (pfd.revents & POLLIN);
#else
    return false;
#endif
}

bool IORedirector::is_fd_ready_for_write(int fd, int timeout_ms) const {
#ifndef _WIN32
    struct pollfd pfd;
    pfd.fd = fd;
    pfd.events = POLLOUT;
    
    int result = poll(&pfd, 1, timeout_ms);
    return result > 0 && (pfd.revents & POLLOUT);
#else
    return false;
#endif
}

bool IORedirector::set_non_blocking(int fd) {
#ifndef _WIN32
    int flags = fcntl(fd, F_GETFL);
    if (flags == -1) {
        return false;
    }
    
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK) != -1;
#else
    return false;
#endif
}

void IORedirector::cleanup_file_descriptor(int fd) {
    if (fd != -1 && fd != STDIN_FILENO && fd != STDOUT_FILENO && fd != STDERR_FILENO) {
        close(fd);
    }
}

// Static instance management
IORedirector& IORedirector::instance() {
    if (!global_instance_) {
        global_instance_ = std::make_unique<IORedirector>();
    }
    return *global_instance_;
}

bool IORedirector::initialize_global_redirector() {
    // Instance is created on first access
    return true;
}

void IORedirector::shutdown_global_redirector() {
    if (global_instance_) {
        global_instance_.reset();
    }
}

/*
 * IORedirectionManager Implementation
 */

IORedirectMode IORedirectionManager::string_to_redirect_mode(const std::string& mode_str) {
    if (mode_str == "pipe") return IORedirectMode::PIPE;
    if (mode_str == "file") return IORedirectMode::FILE;
    if (mode_str == "console") return IORedirectMode::CONSOLE;
    if (mode_str == "null") return IORedirectMode::NULL_DEV;
    if (mode_str == "merge") return IORedirectMode::MERGE;
    if (mode_str == "inherit") return IORedirectMode::INHERIT;
    
    return IORedirectMode::INHERIT; // Default
}

std::string IORedirectionManager::redirect_mode_to_string(IORedirectMode mode) {
    switch (mode) {
        case IORedirectMode::PIPE: return "pipe";
        case IORedirectMode::FILE: return "file";
        case IORedirectMode::CONSOLE: return "console";
        case IORedirectMode::NULL_DEV: return "null";
        case IORedirectMode::MERGE: return "merge";
        case IORedirectMode::INHERIT: return "inherit";
        default: return "inherit";
    }
}

bool IORedirectionManager::validate_redirect_mode(const std::string& mode_str) {
    return mode_str == "pipe" || mode_str == "file" || mode_str == "console" ||
           mode_str == "null" || mode_str == "merge" || mode_str == "inherit";
}

bool IORedirectionManager::handle_stdin_mode_option(int socket_fd, const std::string& mode) {
    IORedirectMode redirect_mode = string_to_redirect_mode(mode);
    
    // stdin cannot use merge mode
    if (redirect_mode == IORedirectMode::MERGE) {
        debug_message("MERGE mode not valid for stdin");
        return false;
    }
    
    IORedirector& redirector = IORedirector::instance();
    return redirector.configure_stdio(socket_fd, IOStreamType::STDIN, redirect_mode);
}

bool IORedirectionManager::handle_stdout_mode_option(int socket_fd, const std::string& mode) {
    IORedirectMode redirect_mode = string_to_redirect_mode(mode);
    
    IORedirector& redirector = IORedirector::instance();
    return redirector.configure_stdio(socket_fd, IOStreamType::STDOUT, redirect_mode);
}

bool IORedirectionManager::handle_stderr_mode_option(int socket_fd, const std::string& mode) {
    IORedirectMode redirect_mode = string_to_redirect_mode(mode);
    
    IORedirector& redirector = IORedirector::instance();
    return redirector.configure_stdio(socket_fd, IOStreamType::STDERR, redirect_mode);
}

/*
 * ProcessIOMonitor Implementation
 */

bool ProcessIOMonitor::signal_stdout_data_available(int socket_fd, size_t bytes) {
    return AsyncEventManager::signal_process_output(socket_fd, bytes);
}

bool ProcessIOMonitor::signal_stderr_data_available(int socket_fd, size_t bytes) {
    return AsyncEventManager::signal_process_error(socket_fd, "stderr_data_available");
}

bool ProcessIOMonitor::signal_stdin_write_complete(int socket_fd, size_t bytes_written) {
    return AsyncEventManager::signal_process_ready(socket_fd);
}

/*
 * Global initialization functions
 */

bool init_io_redirection_system() {
    return IORedirector::initialize_global_redirector();
}

void cleanup_io_redirection_system() {
    IORedirector::shutdown_global_redirector();
}

/*
 * Socket option validation functions
 */

bool validate_external_stdin_mode(const svalue_t* value) {
    if (value == nullptr || value->type != T_STRING) {
        return false;
    }
    
    std::string mode(value->u.string);
    return IORedirectionManager::validate_redirect_mode(mode);
}

bool validate_external_stdout_mode(const svalue_t* value) {
    if (value == nullptr || value->type != T_STRING) {
        return false;
    }
    
    std::string mode(value->u.string);
    return IORedirectionManager::validate_redirect_mode(mode);
}

bool validate_external_stderr_mode(const svalue_t* value) {
    if (value == nullptr || value->type != T_STRING) {
        return false;
    }
    
    std::string mode(value->u.string);
    return IORedirectionManager::validate_redirect_mode(mode);
}