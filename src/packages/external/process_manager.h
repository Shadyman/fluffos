#ifndef PACKAGES_EXTERNAL_PROCESS_MANAGER_H_
#define PACKAGES_EXTERNAL_PROCESS_MANAGER_H_

#include "base/package_api.h"
#include "external.h"
#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <chrono>

/*
 * ProcessManager - Lifecycle Management for External Processes
 * 
 * This class handles the complete lifecycle of external processes
 * spawned through the unified socket architecture. It provides:
 * 
 * - Process spawning with security validation
 * - I/O redirection and monitoring
 * - Resource limit enforcement
 * - Timeout handling
 * - Process cleanup and resource management
 * - Thread-safe process monitoring
 */

class ProcessManager {
public:
    // Singleton pattern for global process management
    static ProcessManager& instance();
    
    // Process lifecycle operations
    bool spawn_process(int socket_fd, ExternalProcessInfo* process_info, const SecurityContext& security);
    bool terminate_process(int socket_fd, int signal = SIGTERM);
    bool kill_process(int socket_fd);
    
    // Process monitoring
    bool is_process_running(int socket_fd);
    int get_process_exit_code(int socket_fd);
    ExternalProcessInfo* get_process_info(int socket_fd);
    
    // I/O operations
    int write_to_process(int socket_fd, const char* data, size_t length);
    int read_from_process(int socket_fd, char* buffer, size_t max_length);
    
    // Process cleanup
    void cleanup_process(int socket_fd);
    void cleanup_all_processes();
    void cleanup_finished_processes();
    
    // Resource management
    size_t get_active_process_count() const;
    size_t get_total_memory_usage() const;
    void set_global_process_limit(size_t limit);
    
    // Monitoring and debugging
    void dump_process_status(outbuffer_t* buffer) const;
    array_t* get_process_list() const;
    
private:
    ProcessManager();
    ~ProcessManager();
    
    // Process storage
    std::unordered_map<int, std::unique_ptr<ExternalProcessInfo>> processes_;
    mutable std::mutex processes_mutex_;
    
    // Monitoring thread
    std::thread monitor_thread_;
    std::atomic<bool> monitor_running_;
    std::condition_variable monitor_cv_;
    std::mutex monitor_mutex_;
    
    // Configuration
    size_t max_processes_;
    std::chrono::seconds default_timeout_;
    size_t default_buffer_size_;
    
    // Helper methods
    bool setup_process_io_pipes(ExternalProcessInfo* info);
    bool setup_process_environment(ExternalProcessInfo* info, const SecurityContext& security);
    bool apply_resource_limits(const SecurityContext& security);
    bool drop_process_privileges(const SecurityContext& security);
    
    // Process spawning helpers
    pid_t spawn_unix_process(ExternalProcessInfo* info, const SecurityContext& security);
    #ifdef _WIN32
    PROCESS_INFORMATION spawn_windows_process(ExternalProcessInfo* info, const SecurityContext& security);
    #endif
    
    // I/O management
    void setup_nonblocking_io(int fd);
    int read_nonblocking(int fd, char* buffer, size_t max_length);
    int write_nonblocking(int fd, const char* data, size_t length);
    
    // Monitoring
    void monitor_thread_function();
    void check_process_timeouts();
    void check_process_status();
    void handle_process_completion(int socket_fd, ExternalProcessInfo* info);
    
    // Cleanup helpers
    void close_process_pipes(ExternalProcessInfo* info);
    void cleanup_process_resources(ExternalProcessInfo* info);
    
    // Validation
    bool validate_process_limits() const;
    bool validate_security_context(const SecurityContext& security) const;
    
    // Thread safety helpers
    std::unique_lock<std::mutex> lock_processes() const;
    void notify_process_change();
};

/*
 * ProcessIOHandler - Manages I/O redirection for external processes
 * 
 * Handles stdin/stdout/stderr redirection through socket pairs,
 * providing non-blocking I/O operations and buffer management.
 */
class ProcessIOHandler {
public:
    ProcessIOHandler(ExternalProcessInfo* process_info);
    ~ProcessIOHandler();
    
    // I/O setup
    bool setup_io_redirection();
    void cleanup_io();
    
    // I/O operations
    int write_stdin(const char* data, size_t length);
    int read_stdout(char* buffer, size_t max_length);
    int read_stderr(char* buffer, size_t max_length);
    
    // Status
    bool has_stdout_data() const;
    bool has_stderr_data() const;
    bool is_stdin_writable() const;
    
private:
    ExternalProcessInfo* process_info_;
    
    // Pipe file descriptors
    int stdin_pipe_[2];   // [0] = child read, [1] = parent write
    int stdout_pipe_[2];  // [0] = parent read, [1] = child write
    int stderr_pipe_[2];  // [0] = parent read, [1] = child write
    
    // Buffer management
    std::string stdin_buffer_;
    std::string stdout_buffer_;
    std::string stderr_buffer_;
    size_t max_buffer_size_;
    
    // Helper methods
    bool create_pipe_pair(int pipe_fds[2]);
    void close_pipe_pair(int pipe_fds[2]);
    void setup_child_io();
    void setup_parent_io();
    int read_pipe_data(int fd, std::string& buffer, char* output, size_t max_length);
};

/*
 * ProcessSecurityManager - Security enforcement for external processes
 * 
 * Implements security policies including command validation,
 * privilege dropping, resource limits, and sandboxing.
 */
class ProcessSecurityManager {
public:
    static bool validate_and_apply_security(ExternalProcessInfo* info, const SecurityContext& security);
    
    // Command validation
    static bool validate_command_execution(const std::string& command, const SecurityContext& security);
    static bool validate_command_arguments(const std::vector<std::string>& args, const SecurityContext& security);
    static bool validate_environment_variables(const std::map<std::string, std::string>& env, const SecurityContext& security);
    
    // Path security
    static bool validate_working_directory(const std::string& workdir, const SecurityContext& security);
    static bool validate_file_access(const std::string& path, const SecurityContext& security);
    
    // Privilege management
    static bool drop_privileges(uid_t uid, gid_t gid);
    static bool set_process_limits(const SecurityContext& security);
    
    // Sandboxing
    static bool setup_sandbox(const SecurityContext& security);
    static bool restrict_filesystem_access(const std::vector<std::string>& allowed_paths);
    
private:
    // Security validation helpers
    static bool is_safe_command(const std::string& command);
    static bool is_safe_argument(const std::string& arg);
    static bool is_safe_environment_variable(const std::string& name, const std::string& value);
    static bool contains_shell_injection(const std::string& input);
    static bool contains_path_traversal(const std::string& path);
    
    // System security
    static bool apply_rlimits(const SecurityContext& security);
    static bool setup_seccomp_filter();
    static bool setup_namespace_isolation();
    
    // Validation patterns
    static const std::vector<std::string> dangerous_commands_;
    static const std::vector<std::string> dangerous_patterns_;
    static const std::vector<std::string> safe_paths_;
};

/*
 * ProcessTimeout - Timeout management for external processes
 * 
 * Handles process timeouts, both for execution time and I/O operations.
 */
class ProcessTimeout {
public:
    ProcessTimeout(int socket_fd, std::chrono::seconds timeout);
    ~ProcessTimeout();
    
    // Timeout management
    void start_timeout();
    void cancel_timeout();
    void reset_timeout();
    bool is_expired() const;
    
    // Callback for timeout handling
    void set_timeout_callback(std::function<void(int)> callback);
    
private:
    int socket_fd_;
    std::chrono::seconds timeout_duration_;
    std::chrono::time_point<std::chrono::steady_clock> start_time_;
    std::atomic<bool> timeout_active_;
    std::function<void(int)> timeout_callback_;
    
    // Timeout monitoring
    std::thread timeout_thread_;
    std::atomic<bool> thread_running_;
    std::condition_variable timeout_cv_;
    std::mutex timeout_mutex_;
    
    void timeout_thread_function();
};

/*
 * Global process management utilities
 */
namespace ProcessUtils {
    // Process information
    bool is_pid_running(pid_t pid);
    int get_pid_exit_code(pid_t pid);
    std::string get_process_command_line(pid_t pid);
    
    // Signal handling
    bool send_signal_to_process(pid_t pid, int signal);
    bool terminate_process_gracefully(pid_t pid, std::chrono::seconds timeout = std::chrono::seconds(5));
    
    // Resource monitoring
    size_t get_process_memory_usage(pid_t pid);
    double get_process_cpu_usage(pid_t pid);
    size_t get_process_file_descriptors(pid_t pid);
    
    // System limits
    size_t get_system_process_limit();
    size_t get_system_memory_available();
    bool check_system_resources();
}

#endif  // PACKAGES_EXTERNAL_PROCESS_MANAGER_H_