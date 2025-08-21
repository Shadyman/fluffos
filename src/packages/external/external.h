#ifndef PACKAGES_EXTERNAL_H_
#define PACKAGES_EXTERNAL_H_

#include "base/package_api.h"
#include "packages/sockets/socket_efuns.h"
#include "packages/sockets/socket_option_manager.h"
#include <memory>
#include <string>
#include <vector>
#include <map>
#include <unordered_map>

/*
 * External Process Package for FluffOS Unified Socket Architecture
 * 
 * This package provides comprehensive external process integration
 * following the unified socket architecture patterns. It supports
 * process spawning, lifecycle management, I/O redirection, and
 * security sandboxing through the socket option system.
 * 
 * Socket Modes:
 * - EXTERNAL_PROCESS (22): Direct process communication
 * - EXTERNAL_COMMAND_MODE (23): Command execution mode
 * 
 * Socket Options (140-149):
 * - EXTERNAL_COMMAND (140): Command to execute
 * - EXTERNAL_ARGS (141): Command arguments array
 * - EXTERNAL_ENV (142): Environment variables
 * - EXTERNAL_WATCH_PATH (143): File path monitoring
 * - EXTERNAL_WORKING_DIR (144): Working directory
 * - EXTERNAL_USER (145): User context
 * - EXTERNAL_GROUP (146): Group context
 * - EXTERNAL_TIMEOUT (147): Execution timeout
 * - EXTERNAL_BUFFER_SIZE (148): I/O buffer size
 * - EXTERNAL_ASYNC (149): Async execution mode
 */

// Forward declarations
class ProcessManager;
class CommandExecutor;

// External process state information
struct ExternalProcessInfo {
    pid_t pid;
    int socket_fd;
    std::string command;
    std::vector<std::string> args;
    std::map<std::string, std::string> environment;
    std::string working_dir;
    std::string user;
    std::string group;
    int timeout_seconds;
    size_t buffer_size;
    bool async_mode;
    
    // Runtime state
    time_t start_time;
    bool is_running;
    int exit_code;
    std::string error_message;
    
    // I/O redirection
    int stdin_fd;
    int stdout_fd;
    int stderr_fd;
    
    ExternalProcessInfo() :
        pid(-1), socket_fd(-1), timeout_seconds(30), buffer_size(4096),
        async_mode(false), start_time(0), is_running(false), exit_code(-1),
        stdin_fd(-1), stdout_fd(-1), stderr_fd(-1) {}
};

// Security context for process execution
struct SecurityContext {
    bool enable_sandbox;
    std::vector<std::string> allowed_commands;
    std::vector<std::string> allowed_paths;
    std::vector<std::string> blocked_commands;
    std::vector<std::string> blocked_paths;
    uid_t run_as_uid;
    gid_t run_as_gid;
    bool drop_privileges;
    
    // Resource limits
    size_t max_memory;
    int max_cpu_time;
    int max_processes;
    int max_open_files;
    
    SecurityContext() :
        enable_sandbox(true), run_as_uid(0), run_as_gid(0),
        drop_privileges(true), max_memory(100 * 1024 * 1024), // 100MB
        max_cpu_time(30), max_processes(10), max_open_files(64) {}
};

// External socket handler class
class ExternalSocketHandler {
public:
    static int create_handler(enum socket_mode_extended mode, svalue_t *read_callback, svalue_t *close_callback);
    static int bind_handler(int socket_fd, int port, const char *addr);
    static int connect_handler(int socket_fd, const char *addr, svalue_t *read_callback, svalue_t *write_callback);
    static void cleanup_handler(int socket_fd);
    
    // Process management
    static int spawn_process(int socket_fd);
    static bool terminate_process(int socket_fd, int signal = SIGTERM);
    static bool kill_process(int socket_fd);
    static ExternalProcessInfo* get_process_info(int socket_fd);
    
    // I/O operations
    static int write_to_process(int socket_fd, const char* data, size_t length);
    static int read_from_process(int socket_fd, char* buffer, size_t max_length);
    
private:
    static std::unordered_map<int, std::unique_ptr<ExternalProcessInfo>> processes_;
    static SecurityContext default_security_context_;
    
    // Helper methods
    static bool validate_command(const std::string& command, const SecurityContext& security);
    static bool setup_process_environment(ExternalProcessInfo* info);
    static bool setup_process_io(ExternalProcessInfo* info);
    static bool apply_security_context(ExternalProcessInfo* info, const SecurityContext& security);
    static void cleanup_process_resources(ExternalProcessInfo* info);
    static void monitor_process(int socket_fd, pid_t pid);
    
    // Option handling helpers
    static bool extract_process_options(int socket_fd, ExternalProcessInfo* info);
    static SecurityContext get_security_context(int socket_fd);
};

// Command execution validation and security
class CommandValidator {
public:
    static bool validate_command_path(const std::string& command, const SecurityContext& context);
    static bool validate_arguments(const std::vector<std::string>& args, const SecurityContext& context);
    static bool validate_environment(const std::map<std::string, std::string>& env, const SecurityContext& context);
    static bool validate_working_directory(const std::string& workdir, const SecurityContext& context);
    
    // Security checks
    static bool is_command_allowed(const std::string& command, const SecurityContext& context);
    static bool is_path_allowed(const std::string& path, const SecurityContext& context);
    static bool check_resource_limits(const SecurityContext& context);
    
private:
    static bool contains_dangerous_patterns(const std::string& input);
    static bool is_absolute_path(const std::string& path);
    static bool path_traversal_check(const std::string& path);
};

// Process monitoring and lifecycle management
class ProcessMonitor {
public:
    static void start_monitoring(int socket_fd, pid_t pid);
    static void stop_monitoring(int socket_fd);
    static bool is_process_running(pid_t pid);
    static int get_process_exit_code(pid_t pid);
    static void cleanup_finished_processes();
    
private:
    static std::map<int, pid_t> monitored_processes_;
    static void monitor_thread_function();
    static bool monitor_thread_running_;
};

// Utility functions for external process operations
namespace ExternalUtils {
    // String utilities
    std::vector<std::string> split_command_line(const std::string& cmdline);
    std::string escape_shell_argument(const std::string& arg);
    std::string join_arguments(const std::vector<std::string>& args);
    
    // Environment utilities
    std::vector<char*> build_environment_array(const std::map<std::string, std::string>& env);
    void cleanup_environment_array(std::vector<char*>& env_array);
    
    // Path utilities
    std::string resolve_path(const std::string& path);
    bool is_executable(const std::string& path);
    std::string find_in_path(const std::string& command);
    
    // Security utilities
    bool drop_privileges(uid_t uid, gid_t gid);
    bool set_resource_limits(const SecurityContext& context);
    bool setup_chroot_jail(const std::string& jail_path);
}

// Registration and initialization functions
void init_external_socket_handlers();
void cleanup_external_socket_handlers();
void register_external_option_handlers();

// Global configuration
extern SecurityContext g_external_security_context;
extern bool g_external_package_initialized;

// EFun implementations (for compatibility with existing external_start)
#ifdef F_EXTERNAL_START
void f_external_start();
#endif

// New unified architecture EFuns (called through socket system)
#ifdef F_EXTERNAL_SPAWN_PROCESS
void f_external_spawn_process();
#endif

#ifdef F_EXTERNAL_KILL_PROCESS
void f_external_kill_process();
#endif

#ifdef F_EXTERNAL_PROCESS_STATUS
void f_external_process_status();
#endif

#ifdef F_EXTERNAL_WRITE_PROCESS
void f_external_write_process();
#endif

#ifdef F_EXTERNAL_READ_PROCESS
void f_external_read_process();
#endif

/*
 * Integration with SocketOptionManager - these functions register
 * handlers for external-specific socket options (140-149 range)
 */
void register_external_command_handler();
void register_external_args_handler();
void register_external_env_handler();
void register_external_working_dir_handler();
void register_external_user_handler();
void register_external_group_handler();
void register_external_timeout_handler();
void register_external_buffer_size_handler();
void register_external_async_handler();

/*
 * Validation functions for socket options
 */
bool validate_external_command(const svalue_t* value);
bool validate_external_args(const svalue_t* value);
bool validate_external_env(const svalue_t* value);
bool validate_external_working_dir(const svalue_t* value);
bool validate_external_user(const svalue_t* value);
bool validate_external_group(const svalue_t* value);
bool validate_external_timeout(const svalue_t* value);
bool validate_external_buffer_size(const svalue_t* value);
bool validate_external_async(const svalue_t* value);

#endif  // PACKAGES_EXTERNAL_H_