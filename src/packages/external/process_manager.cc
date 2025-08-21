#include "process_manager.h"
#include "base/internal/log.h"
#include <signal.h>
#include <sys/wait.h>
#include <sys/resource.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <algorithm>
#include <cstring>

#ifndef _WIN32
#include <sys/prctl.h>
#include <pwd.h>
#include <grp.h>
#endif

/*
 * ProcessManager implementation
 */

ProcessManager& ProcessManager::instance() {
    static ProcessManager instance;
    return instance;
}

ProcessManager::ProcessManager() :
    max_processes_(100),
    default_timeout_(std::chrono::seconds(30)),
    default_buffer_size_(4096),
    monitor_running_(false) {
    
    debug(external, "ProcessManager initialized");
    
    // Start monitoring thread
    monitor_running_ = true;
    monitor_thread_ = std::thread(&ProcessManager::monitor_thread_function, this);
}

ProcessManager::~ProcessManager() {
    debug(external, "ProcessManager shutting down");
    
    // Stop monitoring thread
    monitor_running_ = false;
    monitor_cv_.notify_all();
    if (monitor_thread_.joinable()) {
        monitor_thread_.join();
    }
    
    // Cleanup all processes
    cleanup_all_processes();
}

bool ProcessManager::spawn_process(int socket_fd, ExternalProcessInfo* process_info, const SecurityContext& security) {
    auto lock = lock_processes();
    
    debug(external, "Spawning process for socket %d: %s", socket_fd, process_info->command.c_str());
    
    // Validate process limits
    if (processes_.size() >= max_processes_) {
        debug(external, "Process limit exceeded: %zu >= %zu", processes_.size(), max_processes_);
        process_info->error_message = "Process limit exceeded";
        return false;
    }
    
    // Validate security context
    if (!validate_security_context(security)) {
        debug(external, "Security validation failed for socket %d", socket_fd);
        process_info->error_message = "Security validation failed";
        return false;
    }
    
    // Setup I/O pipes
    if (!setup_process_io_pipes(process_info)) {
        debug(external, "Failed to setup I/O pipes for socket %d", socket_fd);
        process_info->error_message = "Failed to setup I/O pipes";
        return false;
    }
    
    // Spawn the process
    pid_t pid;
    
#ifndef _WIN32
    pid = spawn_unix_process(process_info, security);
#else
    // Windows implementation would go here
    pid = -1;
    process_info->error_message = "Windows not implemented";
#endif
    
    if (pid <= 0) {
        debug(external, "Failed to spawn process for socket %d: %s", socket_fd, process_info->error_message.c_str());
        close_process_pipes(process_info);
        return false;
    }
    
    // Update process info
    process_info->pid = pid;
    process_info->socket_fd = socket_fd;
    process_info->start_time = time(nullptr);
    process_info->is_running = true;
    
    // Store process info
    processes_[socket_fd] = std::unique_ptr<ExternalProcessInfo>(process_info);
    
    debug(external, "Process spawned successfully: socket=%d, pid=%d", socket_fd, pid);
    notify_process_change();
    
    return true;
}

pid_t ProcessManager::spawn_unix_process(ExternalProcessInfo* info, const SecurityContext& security) {
#ifndef _WIN32
    // Build argument array
    std::vector<char*> argv;
    argv.push_back(const_cast<char*>(info->command.c_str()));
    for (const auto& arg : info->args) {
        argv.push_back(const_cast<char*>(arg.c_str()));
    }
    argv.push_back(nullptr);
    
    // Build environment array
    std::vector<char*> envp;
    for (const auto& [key, value] : info->environment) {
        std::string env_var = key + "=" + value;
        char* env_str = new char[env_var.length() + 1];
        strcpy(env_str, env_var.c_str());
        envp.push_back(env_str);
    }
    envp.push_back(nullptr);
    
    // Fork the process
    pid_t pid = fork();
    
    if (pid == 0) {
        // Child process
        
        // Setup I/O redirection
        if (info->stdin_fd >= 0) {
            dup2(info->stdin_fd, STDIN_FILENO);
            close(info->stdin_fd);
        }
        if (info->stdout_fd >= 0) {
            dup2(info->stdout_fd, STDOUT_FILENO);
            close(info->stdout_fd);
        }
        if (info->stderr_fd >= 0) {
            dup2(info->stderr_fd, STDERR_FILENO);
            close(info->stderr_fd);
        }
        
        // Change working directory
        if (!info->working_dir.empty() && chdir(info->working_dir.c_str()) != 0) {
            debug(external, "Failed to change working directory to %s: %s", 
                  info->working_dir.c_str(), strerror(errno));
            _exit(1);
        }
        
        // Apply security context
        if (!ProcessSecurityManager::validate_and_apply_security(info, security)) {
            debug(external, "Failed to apply security context");
            _exit(1);
        }
        
        // Execute the command
        execve(info->command.c_str(), argv.data(), envp.data());
        
        // If we get here, execve failed
        debug(external, "execve failed for command %s: %s", info->command.c_str(), strerror(errno));
        _exit(1);
    } else if (pid > 0) {
        // Parent process - cleanup environment array
        for (char* env_str : envp) {
            if (env_str) delete[] env_str;
        }
        
        // Close child ends of pipes
        if (info->stdin_fd >= 0) close(info->stdin_fd);
        if (info->stdout_fd >= 0) close(info->stdout_fd);
        if (info->stderr_fd >= 0) close(info->stderr_fd);
        
        return pid;
    } else {
        // Fork failed
        info->error_message = std::string("fork() failed: ") + strerror(errno);
        
        // Cleanup environment array
        for (char* env_str : envp) {
            if (env_str) delete[] env_str;
        }
        
        return -1;
    }
#else
    info->error_message = "Unix process spawning not available on Windows";
    return -1;
#endif
}

bool ProcessManager::setup_process_io_pipes(ExternalProcessInfo* info) {
    int stdin_pipe[2], stdout_pipe[2], stderr_pipe[2];
    
    // Create pipes
    if (pipe(stdin_pipe) == -1 || pipe(stdout_pipe) == -1 || pipe(stderr_pipe) == -1) {
        info->error_message = std::string("Failed to create pipes: ") + strerror(errno);
        return false;
    }
    
    // Set up file descriptors
    info->stdin_fd = stdin_pipe[0];   // Child reads from this
    info->stdout_fd = stdout_pipe[1]; // Child writes to this
    info->stderr_fd = stderr_pipe[1]; // Child writes to this
    
    // Store parent ends for I/O operations
    // These will be stored in the socket structure or process info
    // for later I/O operations
    
    // Make parent ends non-blocking
    setup_nonblocking_io(stdin_pipe[1]);
    setup_nonblocking_io(stdout_pipe[0]);
    setup_nonblocking_io(stderr_pipe[0]);
    
    debug(external, "I/O pipes setup successfully for socket %d", info->socket_fd);
    return true;
}

void ProcessManager::setup_nonblocking_io(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags >= 0) {
        fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    }
}

bool ProcessManager::terminate_process(int socket_fd, int signal) {
    auto lock = lock_processes();
    
    auto it = processes_.find(socket_fd);
    if (it == processes_.end()) {
        debug(external, "Process not found for socket %d", socket_fd);
        return false;
    }
    
    ExternalProcessInfo* info = it->second.get();
    if (!info->is_running || info->pid <= 0) {
        debug(external, "Process already terminated for socket %d", socket_fd);
        return false;
    }
    
    debug(external, "Terminating process %d (socket %d) with signal %d", info->pid, socket_fd, signal);
    
#ifndef _WIN32
    if (kill(info->pid, signal) == 0) {
        return true;
    } else {
        debug(external, "Failed to send signal %d to process %d: %s", signal, info->pid, strerror(errno));
        return false;
    }
#else
    // Windows implementation would go here
    return false;
#endif
}

bool ProcessManager::kill_process(int socket_fd) {
    return terminate_process(socket_fd, SIGKILL);
}

bool ProcessManager::is_process_running(int socket_fd) {
    auto lock = lock_processes();
    
    auto it = processes_.find(socket_fd);
    if (it == processes_.end()) {
        return false;
    }
    
    return it->second->is_running;
}

int ProcessManager::get_process_exit_code(int socket_fd) {
    auto lock = lock_processes();
    
    auto it = processes_.find(socket_fd);
    if (it == processes_.end()) {
        return -1;
    }
    
    return it->second->exit_code;
}

ExternalProcessInfo* ProcessManager::get_process_info(int socket_fd) {
    auto lock = lock_processes();
    
    auto it = processes_.find(socket_fd);
    if (it == processes_.end()) {
        return nullptr;
    }
    
    return it->second.get();
}

int ProcessManager::write_to_process(int socket_fd, const char* data, size_t length) {
    auto lock = lock_processes();
    
    auto it = processes_.find(socket_fd);
    if (it == processes_.end() || !it->second->is_running) {
        return -1;
    }
    
    // Implementation would write to the process stdin pipe
    // This is a simplified version
    return write_nonblocking(it->second->stdin_fd, data, length);
}

int ProcessManager::read_from_process(int socket_fd, char* buffer, size_t max_length) {
    auto lock = lock_processes();
    
    auto it = processes_.find(socket_fd);
    if (it == processes_.end()) {
        return -1;
    }
    
    // Implementation would read from process stdout pipe
    // This is a simplified version
    return read_nonblocking(it->second->stdout_fd, buffer, max_length);
}

int ProcessManager::read_nonblocking(int fd, char* buffer, size_t max_length) {
    if (fd < 0) return -1;
    
    ssize_t bytes_read = read(fd, buffer, max_length);
    if (bytes_read < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
        return 0; // No data available
    }
    
    return static_cast<int>(bytes_read);
}

int ProcessManager::write_nonblocking(int fd, const char* data, size_t length) {
    if (fd < 0) return -1;
    
    ssize_t bytes_written = write(fd, data, length);
    if (bytes_written < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
        return 0; // Would block
    }
    
    return static_cast<int>(bytes_written);
}

void ProcessManager::cleanup_process(int socket_fd) {
    auto lock = lock_processes();
    
    auto it = processes_.find(socket_fd);
    if (it == processes_.end()) {
        return;
    }
    
    ExternalProcessInfo* info = it->second.get();
    
    debug(external, "Cleaning up process for socket %d (pid %d)", socket_fd, info->pid);
    
    // Terminate process if still running
    if (info->is_running && info->pid > 0) {
        terminate_process(socket_fd, SIGTERM);
        
        // Wait a short time for graceful termination
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        // Force kill if still running
        if (is_process_running(socket_fd)) {
            kill_process(socket_fd);
        }
    }
    
    // Cleanup resources
    cleanup_process_resources(info);
    
    // Remove from tracking
    processes_.erase(it);
    
    debug(external, "Process cleanup completed for socket %d", socket_fd);
}

void ProcessManager::cleanup_all_processes() {
    auto lock = lock_processes();
    
    debug(external, "Cleaning up all processes (%zu active)", processes_.size());
    
    for (auto& [socket_fd, info] : processes_) {
        if (info->is_running && info->pid > 0) {
            debug(external, "Terminating process %d (socket %d)", info->pid, socket_fd);
            kill(info->pid, SIGTERM);
        }
        cleanup_process_resources(info.get());
    }
    
    processes_.clear();
    debug(external, "All processes cleaned up");
}

void ProcessManager::cleanup_process_resources(ExternalProcessInfo* info) {
    if (!info) return;
    
    // Close file descriptors
    close_process_pipes(info);
    
    // Mark as not running
    info->is_running = false;
}

void ProcessManager::close_process_pipes(ExternalProcessInfo* info) {
    if (info->stdin_fd >= 0) {
        close(info->stdin_fd);
        info->stdin_fd = -1;
    }
    if (info->stdout_fd >= 0) {
        close(info->stdout_fd);
        info->stdout_fd = -1;
    }
    if (info->stderr_fd >= 0) {
        close(info->stderr_fd);
        info->stderr_fd = -1;
    }
}

void ProcessManager::monitor_thread_function() {
    debug(external, "Process monitor thread started");
    
    while (monitor_running_) {
        // Check for finished processes
        cleanup_finished_processes();
        
        // Check for timeouts
        check_process_timeouts();
        
        // Sleep for monitoring interval
        std::unique_lock<std::mutex> lock(monitor_mutex_);
        monitor_cv_.wait_for(lock, std::chrono::milliseconds(1000), [this] { return !monitor_running_; });
    }
    
    debug(external, "Process monitor thread stopped");
}

void ProcessManager::cleanup_finished_processes() {
    auto lock = lock_processes();
    
    std::vector<int> finished_sockets;
    
    for (auto& [socket_fd, info] : processes_) {
        if (!info->is_running) continue;
        
#ifndef _WIN32
        int status;
        pid_t result = waitpid(info->pid, &status, WNOHANG);
        
        if (result == info->pid) {
            // Process has finished
            info->is_running = false;
            if (WIFEXITED(status)) {
                info->exit_code = WEXITSTATUS(status);
                debug(external, "Process %d (socket %d) exited with code %d", 
                      info->pid, socket_fd, info->exit_code);
            } else if (WIFSIGNALED(status)) {
                info->exit_code = -WTERMSIG(status);
                debug(external, "Process %d (socket %d) terminated by signal %d", 
                      info->pid, socket_fd, WTERMSIG(status));
            }
            
            handle_process_completion(socket_fd, info.get());
        } else if (result == -1 && errno == ECHILD) {
            // Process no longer exists
            info->is_running = false;
            info->exit_code = -1;
            debug(external, "Process %d (socket %d) no longer exists", info->pid, socket_fd);
            handle_process_completion(socket_fd, info.get());
        }
#endif
    }
}

void ProcessManager::check_process_timeouts() {
    auto lock = lock_processes();
    
    time_t current_time = time(nullptr);
    
    for (auto& [socket_fd, info] : processes_) {
        if (!info->is_running) continue;
        
        time_t elapsed = current_time - info->start_time;
        if (elapsed > info->timeout_seconds) {
            debug(external, "Process %d (socket %d) timed out after %ld seconds", 
                  info->pid, socket_fd, elapsed);
            
            // Terminate the process
            terminate_process(socket_fd, SIGTERM);
            
            // Mark for cleanup
            info->is_running = false;
            info->exit_code = -ETIMEDOUT;
            info->error_message = "Process timed out";
        }
    }
}

void ProcessManager::handle_process_completion(int socket_fd, ExternalProcessInfo* info) {
    debug(external, "Process completed: socket=%d, pid=%d, exit_code=%d", 
          socket_fd, info->pid, info->exit_code);
    
    // Close pipes
    close_process_pipes(info);
    
    // Notify socket layer about process completion
    // This would integrate with the socket callback system
}

size_t ProcessManager::get_active_process_count() const {
    auto lock = lock_processes();
    
    size_t count = 0;
    for (const auto& [socket_fd, info] : processes_) {
        if (info->is_running) count++;
    }
    
    return count;
}

std::unique_lock<std::mutex> ProcessManager::lock_processes() const {
    return std::unique_lock<std::mutex>(processes_mutex_);
}

void ProcessManager::notify_process_change() {
    monitor_cv_.notify_one();
}

bool ProcessManager::validate_security_context(const SecurityContext& security) const {
    // Basic validation - in a real implementation this would be more comprehensive
    if (security.max_memory == 0 || security.max_cpu_time == 0) {
        return false;
    }
    
    return true;
}

/*
 * ProcessSecurityManager implementation
 */

bool ProcessSecurityManager::validate_and_apply_security(ExternalProcessInfo* info, const SecurityContext& security) {
    debug(external, "Applying security context for process %s", info->command.c_str());
    
    // Validate command
    if (!validate_command_execution(info->command, security)) {
        info->error_message = "Command not allowed by security policy";
        return false;
    }
    
    // Validate arguments
    if (!validate_command_arguments(info->args, security)) {
        info->error_message = "Arguments not allowed by security policy";
        return false;
    }
    
    // Validate environment
    if (!validate_environment_variables(info->environment, security)) {
        info->error_message = "Environment variables not allowed by security policy";
        return false;
    }
    
    // Validate working directory
    if (!info->working_dir.empty() && !validate_working_directory(info->working_dir, security)) {
        info->error_message = "Working directory not allowed by security policy";
        return false;
    }
    
    // Apply resource limits
    if (!set_process_limits(security)) {
        info->error_message = "Failed to apply resource limits";
        return false;
    }
    
    // Drop privileges if required
    if (security.drop_privileges && !drop_privileges(security.run_as_uid, security.run_as_gid)) {
        info->error_message = "Failed to drop privileges";
        return false;
    }
    
    debug(external, "Security context applied successfully");
    return true;
}

bool ProcessSecurityManager::validate_command_execution(const std::string& command, const SecurityContext& security) {
    // Check if command is in allowed list (if specified)
    if (!security.allowed_commands.empty()) {
        auto it = std::find(security.allowed_commands.begin(), security.allowed_commands.end(), command);
        if (it == security.allowed_commands.end()) {
            debug(external, "Command not in allowed list: %s", command.c_str());
            return false;
        }
    }
    
    // Check if command is in blocked list
    auto it = std::find(security.blocked_commands.begin(), security.blocked_commands.end(), command);
    if (it != security.blocked_commands.end()) {
        debug(external, "Command is blocked: %s", command.c_str());
        return false;
    }
    
    // Check for dangerous patterns
    if (!is_safe_command(command)) {
        debug(external, "Command contains dangerous patterns: %s", command.c_str());
        return false;
    }
    
    return true;
}

bool ProcessSecurityManager::is_safe_command(const std::string& command) {
    // Check for shell injection patterns
    const std::vector<std::string> dangerous_chars = {
        ";", "&", "|", "`", "$", "(", ")", "{", "}", "[", "]", "<", ">", "'"
    };
    
    for (const auto& dangerous : dangerous_chars) {
        if (command.find(dangerous) != std::string::npos) {
            return false;
        }
    }
    
    return true;
}

bool ProcessSecurityManager::drop_privileges(uid_t uid, gid_t gid) {
#ifndef _WIN32
    if (gid != 0 && setgid(gid) != 0) {
        debug(external, "Failed to set group ID %d: %s", gid, strerror(errno));
        return false;
    }
    
    if (uid != 0 && setuid(uid) != 0) {
        debug(external, "Failed to set user ID %d: %s", uid, strerror(errno));
        return false;
    }
    
    debug(external, "Privileges dropped to uid=%d, gid=%d", uid, gid);
    return true;
#else
    // Windows privilege dropping would go here
    return false;
#endif
}

bool ProcessSecurityManager::set_process_limits(const SecurityContext& security) {
#ifndef _WIN32
    struct rlimit limit;
    
    // Set memory limit
    if (security.max_memory > 0) {
        limit.rlim_cur = limit.rlim_max = security.max_memory;
        if (setrlimit(RLIMIT_AS, &limit) != 0) {
            debug(external, "Failed to set memory limit: %s", strerror(errno));
            return false;
        }
    }
    
    // Set CPU time limit
    if (security.max_cpu_time > 0) {
        limit.rlim_cur = limit.rlim_max = security.max_cpu_time;
        if (setrlimit(RLIMIT_CPU, &limit) != 0) {
            debug(external, "Failed to set CPU time limit: %s", strerror(errno));
            return false;
        }
    }
    
    // Set process limit
    if (security.max_processes > 0) {
        limit.rlim_cur = limit.rlim_max = security.max_processes;
        if (setrlimit(RLIMIT_NPROC, &limit) != 0) {
            debug(external, "Failed to set process limit: %s", strerror(errno));
            return false;
        }
    }
    
    // Set file descriptor limit
    if (security.max_open_files > 0) {
        limit.rlim_cur = limit.rlim_max = security.max_open_files;
        if (setrlimit(RLIMIT_NOFILE, &limit) != 0) {
            debug(external, "Failed to set file descriptor limit: %s", strerror(errno));
            return false;
        }
    }
    
    debug(external, "Process limits set successfully");
    return true;
#else
    // Windows resource limits would go here
    return true;
#endif
}

// Stub implementations for the remaining validation functions
bool ProcessSecurityManager::validate_command_arguments(const std::vector<std::string>& args, const SecurityContext& security) {
    for (const auto& arg : args) {
        if (!is_safe_argument(arg)) {
            return false;
        }
    }
    return true;
}

bool ProcessSecurityManager::validate_environment_variables(const std::map<std::string, std::string>& env, const SecurityContext& security) {
    for (const auto& [name, value] : env) {
        if (!is_safe_environment_variable(name, value)) {
            return false;
        }
    }
    return true;
}

bool ProcessSecurityManager::validate_working_directory(const std::string& workdir, const SecurityContext& security) {
    return !contains_path_traversal(workdir);
}

bool ProcessSecurityManager::is_safe_argument(const std::string& arg) {
    return !contains_shell_injection(arg);
}

bool ProcessSecurityManager::is_safe_environment_variable(const std::string& name, const std::string& value) {
    return !contains_shell_injection(name) && !contains_shell_injection(value);
}

bool ProcessSecurityManager::contains_shell_injection(const std::string& input) {
    const std::vector<std::string> dangerous_patterns = {
        ";", "&", "|", "`", "$", "$(", "${", "&&", "||", ">>", "<<", "../"
    };
    
    for (const auto& pattern : dangerous_patterns) {
        if (input.find(pattern) != std::string::npos) {
            return true;
        }
    }
    return false;
}

bool ProcessSecurityManager::contains_path_traversal(const std::string& path) {
    return path.find("../") != std::string::npos || path.find("..\\") != std::string::npos;
}