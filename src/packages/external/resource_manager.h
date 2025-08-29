#ifndef PACKAGES_EXTERNAL_RESOURCE_MANAGER_H_
#define PACKAGES_EXTERNAL_RESOURCE_MANAGER_H_

#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <memory>
#include <functional>
#include <chrono>

// Forward declarations for FluffOS types
struct svalue_t;

#ifndef _WIN32
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/resource.h>
#include <sys/prctl.h>
#include <sys/time.h>
#ifdef HAVE_SECCOMP
#include <sys/syscall.h>
#include <linux/seccomp.h>
#include <linux/filter.h>
#endif
#endif

/*
 * Resource Manager for External Process Package - Phase 4
 * 
 * This class provides comprehensive resource control and sandboxing capabilities
 * for external processes in the FluffOS unified socket architecture through 
 * socket options 153-159. It enables CPU limits, memory limits, file limits,
 * sandboxing, and comprehensive resource monitoring.
 * 
 * Features:
 * - CPU time limits and priority control
 * - Memory usage limits (virtual and resident)
 * - File descriptor limits and file access restrictions
 * - Security sandboxing with seccomp (Linux)
 * - Process isolation and chroot capabilities
 * - Resource usage monitoring and reporting
 * - Integration with existing socket system
 * - Automatic cleanup and resource enforcement
 * - Thread-safe operations with proper resource management
 */

// Resource limit types
enum class ResourceLimitType {
    CPU_TIME = 0,        // CPU time in seconds
    CPU_PERCENT = 1,     // CPU usage percentage limit
    MEMORY_VIRTUAL = 2,  // Virtual memory in bytes
    MEMORY_RSS = 3,      // Resident set size in bytes
    FILE_DESCRIPTORS = 4, // Number of open file descriptors
    FILE_SIZE = 5,       // Maximum file size in bytes
    NICE_VALUE = 6,      // Process priority (nice value)
    WALL_TIME = 7        // Wall clock time limit
};

// Security sandbox modes
enum class SandboxMode {
    NONE = 0,           // No sandboxing
    BASIC = 1,          // Basic resource limits only
    RESTRICTED = 2,     // File system restrictions
    ISOLATED = 3,       // Full process isolation
    STRICT = 4          // Strict seccomp filtering
};

// Resource enforcement actions
enum class EnforcementAction {
    WARN = 0,           // Log warning but continue
    THROTTLE = 1,       // Throttle resource usage
    SUSPEND = 2,        // Suspend process temporarily
    TERMINATE = 3       // Terminate process
};

// Resource monitoring result structure
struct ResourceUsage {
    // CPU usage
    double cpu_time_seconds;        // Total CPU time used
    double cpu_percent;             // Current CPU usage percentage
    
    // Memory usage
    size_t memory_virtual_bytes;    // Virtual memory usage
    size_t memory_rss_bytes;        // Resident set size
    size_t memory_peak_bytes;       // Peak memory usage
    
    // File system usage
    int file_descriptors_open;      // Currently open file descriptors
    size_t files_bytes_written;     // Total bytes written to files
    size_t files_bytes_read;        // Total bytes read from files
    
    // Process information
    std::chrono::steady_clock::time_point start_time;
    std::chrono::steady_clock::time_point last_update;
    pid_t pid;
    int nice_value;
    
    ResourceUsage() : cpu_time_seconds(0.0), cpu_percent(0.0),
                     memory_virtual_bytes(0), memory_rss_bytes(0), memory_peak_bytes(0),
                     file_descriptors_open(0), files_bytes_written(0), files_bytes_read(0),
                     pid(-1), nice_value(0) {
        start_time = std::chrono::steady_clock::now();
        last_update = start_time;
    }
};

// Resource limit configuration
struct ResourceLimit {
    ResourceLimitType type;         // Type of resource limit
    double soft_limit;              // Soft limit (warning threshold)
    double hard_limit;              // Hard limit (enforcement threshold)
    EnforcementAction action;       // Action to take when exceeded
    bool enabled;                   // Whether this limit is active
    std::string description;        // Human-readable description
    
    ResourceLimit() : type(ResourceLimitType::CPU_TIME), 
                     soft_limit(0.0), hard_limit(0.0),
                     action(EnforcementAction::WARN), enabled(false) {}
};

// Complete resource configuration for a process
struct ProcessResourceConfig {
    int socket_fd;                  // Associated socket
    pid_t pid;                      // Process ID
    SandboxMode sandbox_mode;       // Security sandbox mode
    
    // Resource limits
    std::map<ResourceLimitType, ResourceLimit> limits;
    
    // Sandbox configuration
    std::vector<std::string> allowed_paths;     // Allowed file system paths
    std::vector<std::string> blocked_paths;     // Blocked file system paths
    std::string chroot_path;                    // Chroot directory (if any)
    std::string working_directory;              // Working directory
    
    // Monitoring configuration
    bool monitoring_enabled;                    // Whether to monitor resource usage
    std::chrono::milliseconds monitor_interval; // Monitoring update interval
    
    // Current resource usage
    ResourceUsage current_usage;
    std::vector<ResourceUsage> usage_history;   // Historical usage data
    
    ProcessResourceConfig() : socket_fd(-1), pid(-1), sandbox_mode(SandboxMode::NONE),
                             monitoring_enabled(true), 
                             monitor_interval(std::chrono::milliseconds(1000)) {}
};

/*
 * ResourceManager Class - Main resource control management
 * 
 * Provides comprehensive resource management capabilities for external processes
 * with integration into the FluffOS socket system.
 */
class ResourceManager {
public:
    ResourceManager();
    ~ResourceManager();
    
    // Configuration management
    bool configure_resource_limit(int socket_fd, ResourceLimitType type, 
                                 double soft_limit, double hard_limit,
                                 EnforcementAction action);
    bool configure_sandbox(int socket_fd, SandboxMode mode, 
                          const std::vector<std::string>& allowed_paths = {},
                          const std::vector<std::string>& blocked_paths = {},
                          const std::string& chroot_path = "");
    bool apply_resource_limits(int socket_fd, pid_t pid);
    void cleanup_resource_limits(int socket_fd);
    
    // Resource monitoring
    bool start_monitoring(int socket_fd);
    void stop_monitoring(int socket_fd);
    bool update_resource_usage(int socket_fd);
    ResourceUsage get_current_usage(int socket_fd) const;
    std::vector<ResourceUsage> get_usage_history(int socket_fd, size_t max_entries = 10) const;
    
    // Resource enforcement
    bool check_resource_limits(int socket_fd);
    bool enforce_resource_limit(int socket_fd, ResourceLimitType type, const ResourceLimit& limit);
    
    // Process control
    bool set_process_priority(int socket_fd, int nice_value);
    bool suspend_process(int socket_fd);
    bool resume_process(int socket_fd);
    bool terminate_process_safely(int socket_fd, int timeout_ms = 5000);
    
    // Information and statistics
    ProcessResourceConfig* get_resource_config(int socket_fd);
    std::vector<int> get_monitored_sockets() const;
    std::map<std::string, double> get_resource_statistics(int socket_fd) const;
    
    // Static instance management
    static ResourceManager& instance();
    static bool initialize_global_manager();
    static void shutdown_global_manager();

private:
    std::unordered_map<int, std::unique_ptr<ProcessResourceConfig>> socket_configs_;
    static std::unique_ptr<ResourceManager> global_instance_;
    
    // Resource monitoring thread management
    std::unordered_map<int, bool> monitoring_threads_;
    void monitoring_thread_function(int socket_fd);
    
    // Helper methods for resource limits
    bool apply_cpu_limits(pid_t pid, const ResourceLimit& limit);
    bool apply_memory_limits(pid_t pid, const ResourceLimit& limit);
    bool apply_file_limits(pid_t pid, const ResourceLimit& limit);
    bool apply_sandbox_restrictions(pid_t pid, const ProcessResourceConfig& config);
    
    // Resource usage collection
    bool collect_cpu_usage(pid_t pid, ResourceUsage& usage);
    bool collect_memory_usage(pid_t pid, ResourceUsage& usage);
    bool collect_file_usage(pid_t pid, ResourceUsage& usage);
    
    // Security and sandboxing
    bool setup_seccomp_filter(SandboxMode mode);
    bool setup_chroot_environment(const std::string& chroot_path);
    bool setup_file_access_restrictions(const std::vector<std::string>& allowed_paths,
                                       const std::vector<std::string>& blocked_paths);
    
    // System utilities
    bool is_process_running(pid_t pid) const;
    bool send_signal_to_process(pid_t pid, int signal);
    std::string get_process_status_file_path(pid_t pid) const;
    bool read_process_stat_info(pid_t pid, std::map<std::string, std::string>& info);
};

/*
 * ResourceLimitManager - Integration with External Package
 * 
 * Manages resource limits for external process sockets and handles
 * socket options 153-159 integration.
 */
class ResourceLimitManager {
public:
    // Socket option handling
    static bool handle_cpu_limit_option(int socket_fd, const std::string& limit_spec);
    static bool handle_memory_limit_option(int socket_fd, const std::string& limit_spec);
    static bool handle_file_limit_option(int socket_fd, const std::string& limit_spec);
    static bool handle_sandbox_option(int socket_fd, const std::string& sandbox_spec);
    static bool handle_nice_option(int socket_fd, int nice_value);
    static bool handle_monitoring_option(int socket_fd, bool enable_monitoring);
    
    // Process lifecycle integration
    static bool setup_process_resource_limits(int socket_fd, pid_t pid);
    static void cleanup_process_resource_limits(int socket_fd);
    
    // LPC interface functions
    static std::map<std::string, double> external_get_resource_usage(int socket_fd);
    static std::vector<std::string> external_get_resource_limits(int socket_fd);
    static bool external_set_resource_limit(int socket_fd, const std::string& limit_type, 
                                           double soft_limit, double hard_limit);
    static bool external_enable_sandbox(int socket_fd, const std::string& mode);
    
    // Validation and conversion
    static ResourceLimitType string_to_limit_type(const std::string& type_str);
    static std::string limit_type_to_string(ResourceLimitType type);
    static SandboxMode string_to_sandbox_mode(const std::string& mode_str);
    static std::string sandbox_mode_to_string(SandboxMode mode);
    static EnforcementAction string_to_enforcement_action(const std::string& action_str);
    static bool validate_limit_specification(const std::string& limit_spec);
    static bool parse_limit_specification(const std::string& limit_spec, 
                                         double& soft_limit, double& hard_limit,
                                         EnforcementAction& action);
    
private:
    static bool apply_cpu_time_limit(int socket_fd, double soft_limit, double hard_limit, 
                                    EnforcementAction action);
    static bool apply_memory_limit(int socket_fd, size_t soft_limit, size_t hard_limit, 
                                  EnforcementAction action);
    static bool apply_file_descriptor_limit(int socket_fd, int soft_limit, int hard_limit, 
                                           EnforcementAction action);
};

/*
 * ResourceSecurityManager - Advanced security and sandboxing for resource management
 * 
 * Provides advanced security features including seccomp filtering,
 * namespace isolation, and comprehensive process containment.
 */
class ResourceSecurityManager {
public:
    // Security setup
    static bool setup_process_security(int socket_fd, pid_t pid, SandboxMode mode);
    static bool apply_seccomp_filter(pid_t pid, SandboxMode mode);
    static bool setup_namespace_isolation(pid_t pid);
    static bool setup_capability_restrictions(pid_t pid);
    
    // File system security
    static bool apply_file_system_restrictions(pid_t pid, 
                                              const std::vector<std::string>& allowed_paths,
                                              const std::vector<std::string>& blocked_paths);
    static bool setup_chroot_jail(pid_t pid, const std::string& chroot_path);
    
    // Network security
    static bool restrict_network_access(pid_t pid, bool allow_network);
    static bool setup_network_namespace(pid_t pid);
    
private:
    static bool create_seccomp_basic_filter(std::vector<struct sock_filter>& filter);
    static bool create_seccomp_restricted_filter(std::vector<struct sock_filter>& filter);
    static bool create_seccomp_strict_filter(std::vector<struct sock_filter>& filter);
    static bool install_seccomp_filter(pid_t pid, const std::vector<struct sock_filter>& filter);
};

/*
 * Utility functions for resource management
 */
namespace ResourceManagementUtils {
    // Resource parsing and validation
    bool is_valid_limit_specification(const std::string& spec);
    bool parse_resource_limit(const std::string& spec, double& value, std::string& unit);
    size_t convert_memory_unit_to_bytes(double value, const std::string& unit);
    double convert_time_unit_to_seconds(double value, const std::string& unit);
    
    // System resource queries
    size_t get_system_memory_total();
    int get_system_cpu_count();
    int get_system_max_file_descriptors();
    
    // Process utilities
    bool is_valid_nice_value(int nice_value);
    std::string get_resource_limit_description(ResourceLimitType type, double limit);
    std::vector<std::string> get_supported_sandbox_modes();
    
    // Security utilities
    bool is_seccomp_available();
    bool is_namespace_isolation_available();
    bool can_use_chroot(const std::string& path);
    
    // Error handling
    std::string get_resource_error_description(int error_code);
    bool is_recoverable_resource_error(int error_code);
}

// Global initialization functions
bool init_resource_management_system();
void cleanup_resource_management_system();

// Socket option registration functions
void register_resource_management_option_handlers();
bool validate_external_cpu_limit(const svalue_t* value);
bool validate_external_memory_limit(const svalue_t* value);
bool validate_external_file_limit(const svalue_t* value);
bool validate_external_sandbox(const svalue_t* value);
bool validate_external_nice(const svalue_t* value);
bool validate_external_monitoring(const svalue_t* value);

#endif  // PACKAGES_EXTERNAL_RESOURCE_MANAGER_H_