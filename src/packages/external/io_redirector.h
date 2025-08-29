#ifndef PACKAGES_EXTERNAL_IO_REDIRECTOR_H_
#define PACKAGES_EXTERNAL_IO_REDIRECTOR_H_

#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <memory>
#include <functional>

// Forward declarations for FluffOS types
struct svalue_t;

#ifndef _WIN32
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#endif

/*
 * I/O Redirector for External Process Package - Phase 3
 * 
 * This class provides comprehensive stdin/stdout/stderr redirection control
 * for the FluffOS unified socket architecture through socket options 150-152.
 * It enables bidirectional process communication and flexible I/O handling.
 * 
 * Features:
 * - Complete stdin/stdout/stderr redirection control
 * - Multiple redirection modes: pipe, file, console, null, merge
 * - Bidirectional process communication via pipes
 * - Non-blocking I/O operations
 * - Integration with existing event notification system
 * - Automatic cleanup on socket closure
 * - Thread-safe operations with proper resource management
 */

// I/O redirection modes for stdin/stdout/stderr control
enum class IORedirectMode {
    PIPE = 0,        // Redirect to pipe for programmatic access
    FILE = 1,        // Redirect to file (path specified separately)
    CONSOLE = 2,     // Keep connected to console/terminal
    NULL_DEV = 3,    // Redirect to /dev/null (discard)
    MERGE = 4,       // Merge stderr into stdout (stderr only)
    INHERIT = 5      // Inherit from parent process
};

// I/O stream types
enum class IOStreamType {
    STDIN = 0,
    STDOUT = 1,
    STDERR = 2
};

// I/O operation result structure
struct IOResult {
    bool success;               // Whether operation succeeded
    size_t bytes_processed;     // Number of bytes read/written
    std::string error_message;  // Error description if failed
    bool would_block;           // True if operation would block (non-blocking I/O)
    
    IOResult() : success(false), bytes_processed(0), would_block(false) {}
};

// I/O redirection configuration for a single stream
struct IOStreamConfig {
    IORedirectMode mode;        // Redirection mode
    std::string file_path;      // File path (for FILE mode)
    int fd;                     // File descriptor after redirection
    int pipe_read_fd;           // Read end of pipe (for PIPE mode)
    int pipe_write_fd;          // Write end of pipe (for PIPE mode)
    bool non_blocking;          // Whether to use non-blocking I/O
    size_t buffer_size;         // Buffer size for I/O operations
    
    IOStreamConfig() : mode(IORedirectMode::INHERIT), fd(-1), 
                      pipe_read_fd(-1), pipe_write_fd(-1), 
                      non_blocking(true), buffer_size(4096) {}
};

// Complete I/O redirection configuration for a process
struct ProcessIOConfig {
    int socket_fd;              // Associated socket
    IOStreamConfig stdin_config;
    IOStreamConfig stdout_config;
    IOStreamConfig stderr_config;
    
    // Process information
    pid_t pid;                  // Process ID after spawn
    bool is_redirected;         // Whether redirection is active
    
    ProcessIOConfig() : socket_fd(-1), pid(-1), is_redirected(false) {}
};

/*
 * IORedirector Class - Main I/O redirection management
 * 
 * Provides comprehensive I/O redirection capabilities for external processes
 * with integration into the FluffOS socket system.
 */
class IORedirector {
public:
    IORedirector();
    ~IORedirector();
    
    // Configuration management
    bool configure_stdio(int socket_fd, IOStreamType stream_type, 
                        IORedirectMode mode, const std::string& file_path = "");
    bool apply_redirection(int socket_fd, pid_t pid);
    void cleanup_redirection(int socket_fd);
    
    // I/O operations
    IOResult write_to_stdin(int socket_fd, const char* data, size_t length);
    IOResult read_from_stdout(int socket_fd, char* buffer, size_t max_length);
    IOResult read_from_stderr(int socket_fd, char* buffer, size_t max_length);
    
    // Status and information
    bool is_stdin_available(int socket_fd) const;
    bool is_stdout_available(int socket_fd) const;
    bool is_stderr_available(int socket_fd) const;
    size_t get_stdout_bytes_available(int socket_fd) const;
    size_t get_stderr_bytes_available(int socket_fd) const;
    
    // Configuration queries
    ProcessIOConfig* get_io_config(int socket_fd);
    std::vector<int> get_active_sockets() const;
    
    // Static instance management
    static IORedirector& instance();
    static bool initialize_global_redirector();
    static void shutdown_global_redirector();

private:
    std::unordered_map<int, std::unique_ptr<ProcessIOConfig>> socket_configs_;
    static std::unique_ptr<IORedirector> global_instance_;
    
    // Helper methods
    bool setup_pipe_redirection(IOStreamConfig& config, IOStreamType stream_type);
    bool setup_file_redirection(IOStreamConfig& config, IOStreamType stream_type, 
                                const std::string& file_path);
    bool setup_null_redirection(IOStreamConfig& config, IOStreamType stream_type);
    bool apply_stream_redirection(const IOStreamConfig& config, IOStreamType stream_type);
    
    // Pipe management
    bool create_pipe_pair(int& read_fd, int& write_fd);
    void close_pipe_pair(int read_fd, int write_fd);
    bool set_non_blocking(int fd);
    
    // File descriptor utilities
    bool is_fd_ready_for_read(int fd, int timeout_ms = 0) const;
    bool is_fd_ready_for_write(int fd, int timeout_ms = 0) const;
    void cleanup_file_descriptor(int fd);
};

/*
 * IORedirectionManager - Integration with External Package
 * 
 * Manages I/O redirection for external process sockets and handles
 * socket options 150-152 integration.
 */
class IORedirectionManager {
public:
    // Socket option handling
    static bool handle_stdin_mode_option(int socket_fd, const std::string& mode);
    static bool handle_stdout_mode_option(int socket_fd, const std::string& mode);
    static bool handle_stderr_mode_option(int socket_fd, const std::string& mode);
    
    // Process lifecycle integration
    static bool setup_process_io_redirection(int socket_fd, pid_t pid);
    static void cleanup_process_io_redirection(int socket_fd);
    
    // LPC interface functions
    static IOResult external_write_to_process(int socket_fd, const std::string& data);
    static IOResult external_read_from_process(int socket_fd, size_t max_bytes);
    static std::map<std::string, size_t> external_get_io_stats(int socket_fd);
    
    // Validation and conversion
    static IORedirectMode string_to_redirect_mode(const std::string& mode_str);
    static std::string redirect_mode_to_string(IORedirectMode mode);
    static bool validate_redirect_mode(const std::string& mode_str);
    static bool validate_file_path_for_redirection(const std::string& file_path);
    
private:
    static IOResult perform_stdin_write(int socket_fd, const char* data, size_t length);
    static IOResult perform_stdout_read(int socket_fd, char* buffer, size_t max_length);
    static IOResult perform_stderr_read(int socket_fd, char* buffer, size_t max_length);
};

/*
 * ProcessIOMonitor - Integration with async event system
 * 
 * Monitors I/O operations and signals async events when data is available
 * or when I/O operations complete. Integrates with Phase 2 eventfd system.
 */
class ProcessIOMonitor {
public:
    // Event monitoring
    static bool start_monitoring_io(int socket_fd);
    static void stop_monitoring_io(int socket_fd);
    
    // Event signaling (integrates with Phase 2 async system)
    static bool signal_stdout_data_available(int socket_fd, size_t bytes);
    static bool signal_stderr_data_available(int socket_fd, size_t bytes);
    static bool signal_stdin_write_complete(int socket_fd, size_t bytes_written);
    static bool signal_io_error(int socket_fd, const std::string& error_message);
    
private:
    static std::map<int, bool> monitored_sockets_;
    static void io_monitor_thread_function(int socket_fd);
};

/*
 * Utility functions for I/O redirection
 */
namespace IORedirectionUtils {
    // Mode validation and conversion
    bool is_valid_redirect_mode(const std::string& mode);
    IORedirectMode parse_redirect_mode(const std::string& mode);
    std::vector<std::string> get_supported_modes();
    
    // File system utilities
    bool is_writable_path(const std::string& path);
    bool is_readable_path(const std::string& path);
    std::string resolve_io_file_path(const std::string& path);
    
    // Pipe utilities
    size_t get_pipe_buffer_size(int fd);
    bool flush_pipe_buffer(int fd);
    bool drain_pipe_buffer(int fd, std::string& output);
    
    // Error handling
    std::string get_io_error_description(int error_code);
    bool is_recoverable_io_error(int error_code);
}

// Global initialization functions
bool init_io_redirection_system();
void cleanup_io_redirection_system();

// Socket option registration functions
void register_io_redirection_option_handlers();
bool validate_external_stdin_mode(const svalue_t* value);
bool validate_external_stdout_mode(const svalue_t* value);
bool validate_external_stderr_mode(const svalue_t* value);

#endif  // PACKAGES_EXTERNAL_IO_REDIRECTOR_H_