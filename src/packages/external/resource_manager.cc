/*
 * resource_manager.cc
 * 
 * Resource Manager for External Process Package - Phase 4
 * FluffOS Unified Socket Architecture - Resource Control and Sandboxing
 */

#include "resource_manager.h"
#include "external.h"
#include "process_manager.h"

#ifndef _WIN32
#include <sys/resource.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <signal.h>
#include <thread>
#include <fstream>
#include <sstream>
#include <cstring>
#include <algorithm>
#include <regex>
#include <dirent.h>
#endif

// Static instance for singleton pattern
std::unique_ptr<ResourceManager> ResourceManager::global_instance_;

/*
 * ResourceManager Implementation
 */

ResourceManager::ResourceManager() {
    // Constructor - initialize monitoring systems
}

ResourceManager::~ResourceManager() {
    // Stop all monitoring threads and cleanup resources
    for (auto& [socket_fd, config] : socket_configs_) {
        stop_monitoring(socket_fd);
    }
    socket_configs_.clear();
    monitoring_threads_.clear();
}

bool ResourceManager::configure_resource_limit(int socket_fd, ResourceLimitType type,
                                              double soft_limit, double hard_limit,
                                              EnforcementAction action) {
    auto config = get_resource_config(socket_fd);
    if (!config) {
        // Create new configuration
        socket_configs_[socket_fd] = std::make_unique<ProcessResourceConfig>();
        config = socket_configs_[socket_fd].get();
        config->socket_fd = socket_fd;
    }
    
    ResourceLimit limit;
    limit.type = type;
    limit.soft_limit = soft_limit;
    limit.hard_limit = hard_limit;
    limit.action = action;
    limit.enabled = true;
    limit.description = ResourceManagementUtils::get_resource_limit_description(type, hard_limit);
    
    config->limits[type] = limit;
    
#ifdef EXTERNAL_DEBUG
    debug_message("ResourceManager: Configured %s limit for socket %d: soft=%.2f hard=%.2f action=%d\n",
                  ResourceLimitManager::limit_type_to_string(type).c_str(), 
                  socket_fd, soft_limit, hard_limit, static_cast<int>(action));
#endif
    
    return true;
}

bool ResourceManager::configure_sandbox(int socket_fd, SandboxMode mode,
                                       const std::vector<std::string>& allowed_paths,
                                       const std::vector<std::string>& blocked_paths,
                                       const std::string& chroot_path) {
    auto config = get_resource_config(socket_fd);
    if (!config) {
        socket_configs_[socket_fd] = std::make_unique<ProcessResourceConfig>();
        config = socket_configs_[socket_fd].get();
        config->socket_fd = socket_fd;
    }
    
    config->sandbox_mode = mode;
    config->allowed_paths = allowed_paths;
    config->blocked_paths = blocked_paths;
    config->chroot_path = chroot_path;
    
#ifdef EXTERNAL_DEBUG
    debug_message("ResourceManager: Configured sandbox mode %s for socket %d\n",
                  ResourceLimitManager::sandbox_mode_to_string(mode).c_str(), socket_fd);
#endif
    
    return true;
}

bool ResourceManager::apply_resource_limits(int socket_fd, pid_t pid) {
    auto config = get_resource_config(socket_fd);
    if (!config) {
        return false;
    }
    
    config->pid = pid;
    config->current_usage.pid = pid;
    
    // Apply all configured limits
    for (const auto& [type, limit] : config->limits) {
        if (!limit.enabled) continue;
        
        bool success = false;
        switch (type) {
            case ResourceLimitType::CPU_TIME:
            case ResourceLimitType::CPU_PERCENT:
                success = apply_cpu_limits(pid, limit);
                break;
            case ResourceLimitType::MEMORY_VIRTUAL:
            case ResourceLimitType::MEMORY_RSS:
                success = apply_memory_limits(pid, limit);
                break;
            case ResourceLimitType::FILE_DESCRIPTORS:
            case ResourceLimitType::FILE_SIZE:
                success = apply_file_limits(pid, limit);
                break;
            case ResourceLimitType::NICE_VALUE:
                success = set_process_priority(socket_fd, static_cast<int>(limit.hard_limit));
                break;
            default:
                success = true; // Skip unsupported limits
                break;
        }
        
        if (!success) {
#ifdef EXTERNAL_DEBUG
            debug_message("ResourceManager: Failed to apply %s limit for socket %d pid %d\n",
                          ResourceLimitManager::limit_type_to_string(type).c_str(), socket_fd, pid);
#endif
        }
    }
    
    // Apply sandbox restrictions
    if (config->sandbox_mode != SandboxMode::NONE) {
        apply_sandbox_restrictions(pid, *config);
    }
    
    // Start monitoring if enabled
    if (config->monitoring_enabled) {
        start_monitoring(socket_fd);
    }
    
    return true;
}

void ResourceManager::cleanup_resource_limits(int socket_fd) {
    stop_monitoring(socket_fd);
    socket_configs_.erase(socket_fd);
    monitoring_threads_.erase(socket_fd);
    
#ifdef EXTERNAL_DEBUG
    debug_message("ResourceManager: Cleaned up resources for socket %d\n", socket_fd);
#endif
}

bool ResourceManager::start_monitoring(int socket_fd) {
    auto config = get_resource_config(socket_fd);
    if (!config || config->pid == -1) {
        return false;
    }
    
    if (monitoring_threads_.find(socket_fd) != monitoring_threads_.end() && 
        monitoring_threads_[socket_fd]) {
        return true; // Already monitoring
    }
    
    monitoring_threads_[socket_fd] = true;
    
    // Start monitoring thread
    std::thread monitor_thread(&ResourceManager::monitoring_thread_function, this, socket_fd);
    monitor_thread.detach();
    
#ifdef EXTERNAL_DEBUG
    debug_message("ResourceManager: Started monitoring for socket %d pid %d\n", socket_fd, config->pid);
#endif
    
    return true;
}

void ResourceManager::stop_monitoring(int socket_fd) {
    monitoring_threads_[socket_fd] = false;
    
#ifdef EXTERNAL_DEBUG
    debug_message("ResourceManager: Stopped monitoring for socket %d\n", socket_fd);
#endif
}

void ResourceManager::monitoring_thread_function(int socket_fd) {
    auto config = get_resource_config(socket_fd);
    if (!config) return;
    
    while (monitoring_threads_[socket_fd] && is_process_running(config->pid)) {
        update_resource_usage(socket_fd);
        check_resource_limits(socket_fd);
        
        std::this_thread::sleep_for(config->monitor_interval);
    }
    
    // Process has terminated or monitoring stopped
    monitoring_threads_[socket_fd] = false;
}

bool ResourceManager::update_resource_usage(int socket_fd) {
    auto config = get_resource_config(socket_fd);
    if (!config || config->pid == -1) {
        return false;
    }
    
    ResourceUsage& usage = config->current_usage;
    usage.last_update = std::chrono::steady_clock::now();
    
    // Collect resource usage from system
    collect_cpu_usage(config->pid, usage);
    collect_memory_usage(config->pid, usage);
    collect_file_usage(config->pid, usage);
    
    // Store in history (keep last 100 entries)
    config->usage_history.push_back(usage);
    if (config->usage_history.size() > 100) {
        config->usage_history.erase(config->usage_history.begin());
    }
    
    return true;
}

ResourceUsage ResourceManager::get_current_usage(int socket_fd) const {
    auto it = socket_configs_.find(socket_fd);
    if (it != socket_configs_.end()) {
        return it->second->current_usage;
    }
    return ResourceUsage{};
}

std::vector<ResourceUsage> ResourceManager::get_usage_history(int socket_fd, size_t max_entries) const {
    auto it = socket_configs_.find(socket_fd);
    if (it != socket_configs_.end()) {
        const auto& history = it->second->usage_history;
        if (history.size() <= max_entries) {
            return history;
        } else {
            return std::vector<ResourceUsage>(history.end() - max_entries, history.end());
        }
    }
    return {};
}

bool ResourceManager::check_resource_limits(int socket_fd) {
    auto config = get_resource_config(socket_fd);
    if (!config) return false;
    
    bool all_limits_ok = true;
    
    for (const auto& [type, limit] : config->limits) {
        if (!limit.enabled) continue;
        
        bool limit_exceeded = false;
        double current_value = 0.0;
        
        const ResourceUsage& usage = config->current_usage;
        
        switch (type) {
            case ResourceLimitType::CPU_TIME:
                current_value = usage.cpu_time_seconds;
                break;
            case ResourceLimitType::CPU_PERCENT:
                current_value = usage.cpu_percent;
                break;
            case ResourceLimitType::MEMORY_VIRTUAL:
                current_value = static_cast<double>(usage.memory_virtual_bytes);
                break;
            case ResourceLimitType::MEMORY_RSS:
                current_value = static_cast<double>(usage.memory_rss_bytes);
                break;
            case ResourceLimitType::FILE_DESCRIPTORS:
                current_value = static_cast<double>(usage.file_descriptors_open);
                break;
            default:
                continue;
        }
        
        if (current_value > limit.hard_limit) {
            limit_exceeded = true;
            all_limits_ok = false;
            enforce_resource_limit(socket_fd, type, limit);
        } else if (current_value > limit.soft_limit) {
            // Soft limit exceeded - log warning
#ifdef EXTERNAL_DEBUG
            debug_message("ResourceManager: Soft limit exceeded for %s on socket %d: %.2f > %.2f\n",
                          ResourceLimitManager::limit_type_to_string(type).c_str(),
                          socket_fd, current_value, limit.soft_limit);
#endif
        }
    }
    
    return all_limits_ok;
}

bool ResourceManager::enforce_resource_limit(int socket_fd, ResourceLimitType type, const ResourceLimit& limit) {
    auto config = get_resource_config(socket_fd);
    if (!config || config->pid == -1) return false;
    
    pid_t pid = config->pid;
    
    switch (limit.action) {
        case EnforcementAction::WARN:
#ifdef EXTERNAL_DEBUG
            debug_message("ResourceManager: Hard limit exceeded for %s on socket %d pid %d\n",
                          ResourceLimitManager::limit_type_to_string(type).c_str(), socket_fd, pid);
#endif
            return true;
            
        case EnforcementAction::THROTTLE:
            // Send SIGSTOP to throttle the process temporarily
            return send_signal_to_process(pid, SIGSTOP);
            
        case EnforcementAction::SUSPEND:
            return suspend_process(socket_fd);
            
        case EnforcementAction::TERMINATE:
            return terminate_process_safely(socket_fd);
            
        default:
            return false;
    }
}

ProcessResourceConfig* ResourceManager::get_resource_config(int socket_fd) {
    auto it = socket_configs_.find(socket_fd);
    return (it != socket_configs_.end()) ? it->second.get() : nullptr;
}

std::vector<int> ResourceManager::get_monitored_sockets() const {
    std::vector<int> sockets;
    for (const auto& [socket_fd, _] : socket_configs_) {
        sockets.push_back(socket_fd);
    }
    return sockets;
}

// Helper method implementations
bool ResourceManager::apply_cpu_limits(pid_t pid, const ResourceLimit& limit) {
#ifndef _WIN32
    struct rlimit rlim;
    
    if (limit.type == ResourceLimitType::CPU_TIME) {
        rlim.rlim_cur = static_cast<rlim_t>(limit.soft_limit);
        rlim.rlim_max = static_cast<rlim_t>(limit.hard_limit);
        
        if (setrlimit(RLIMIT_CPU, &rlim) == -1) {
            return false;
        }
    }
    
    return true;
#else
    return false; // Windows not supported
#endif
}

bool ResourceManager::apply_memory_limits(pid_t pid, const ResourceLimit& limit) {
#ifndef _WIN32
    struct rlimit rlim;
    
    if (limit.type == ResourceLimitType::MEMORY_VIRTUAL) {
        rlim.rlim_cur = static_cast<rlim_t>(limit.soft_limit);
        rlim.rlim_max = static_cast<rlim_t>(limit.hard_limit);
        
        if (setrlimit(RLIMIT_AS, &rlim) == -1) {
            return false;
        }
    }
    
    return true;
#else
    return false; // Windows not supported
#endif
}

bool ResourceManager::apply_file_limits(pid_t pid, const ResourceLimit& limit) {
#ifndef _WIN32
    struct rlimit rlim;
    
    if (limit.type == ResourceLimitType::FILE_DESCRIPTORS) {
        rlim.rlim_cur = static_cast<rlim_t>(limit.soft_limit);
        rlim.rlim_max = static_cast<rlim_t>(limit.hard_limit);
        
        if (setrlimit(RLIMIT_NOFILE, &rlim) == -1) {
            return false;
        }
    }
    
    return true;
#else
    return false; // Windows not supported
#endif
}

bool ResourceManager::collect_cpu_usage(pid_t pid, ResourceUsage& usage) {
#ifndef _WIN32
    std::map<std::string, std::string> stat_info;
    if (!read_process_stat_info(pid, stat_info)) {
        return false;
    }
    
    // Parse CPU times from /proc/pid/stat
    if (stat_info.count("utime") && stat_info.count("stime")) {
        long utime = std::stol(stat_info["utime"]);
        long stime = std::stol(stat_info["stime"]);
        long clock_ticks = sysconf(_SC_CLK_TCK);
        
        usage.cpu_time_seconds = static_cast<double>(utime + stime) / clock_ticks;
    }
    
    return true;
#else
    return false; // Windows not supported
#endif
}

bool ResourceManager::collect_memory_usage(pid_t pid, ResourceUsage& usage) {
#ifndef _WIN32
    std::string status_file = "/proc/" + std::to_string(pid) + "/status";
    std::ifstream file(status_file);
    if (!file.is_open()) {
        return false;
    }
    
    std::string line;
    while (std::getline(file, line)) {
        if (line.substr(0, 6) == "VmSize") {
            std::regex pattern(R"(VmSize:\s+(\d+)\s+kB)");
            std::smatch match;
            if (std::regex_search(line, match, pattern)) {
                usage.memory_virtual_bytes = std::stol(match[1]) * 1024;
            }
        } else if (line.substr(0, 5) == "VmRSS") {
            std::regex pattern(R"(VmRSS:\s+(\d+)\s+kB)");
            std::smatch match;
            if (std::regex_search(line, match, pattern)) {
                usage.memory_rss_bytes = std::stol(match[1]) * 1024;
            }
        } else if (line.substr(0, 6) == "VmHWM") {
            std::regex pattern(R"(VmHWM:\s+(\d+)\s+kB)");
            std::smatch match;
            if (std::regex_search(line, match, pattern)) {
                usage.memory_peak_bytes = std::stol(match[1]) * 1024;
            }
        }
    }
    
    return true;
#else
    return false; // Windows not supported
#endif
}

bool ResourceManager::collect_file_usage(pid_t pid, ResourceUsage& usage) {
#ifndef _WIN32
    std::string fd_dir = "/proc/" + std::to_string(pid) + "/fd";
    int fd_count = 0;
    
    // Count open file descriptors
    DIR* dir = opendir(fd_dir.c_str());
    if (dir) {
        struct dirent* entry;
        while ((entry = readdir(dir)) != nullptr) {
            if (entry->d_name[0] != '.') {
                fd_count++;
            }
        }
        closedir(dir);
    }
    
    usage.file_descriptors_open = fd_count;
    return true;
#else
    return false; // Windows not supported
#endif
}

bool ResourceManager::set_process_priority(int socket_fd, int nice_value) {
    auto config = get_resource_config(socket_fd);
    if (!config || config->pid == -1) return false;
    
#ifndef _WIN32
    if (setpriority(PRIO_PROCESS, config->pid, nice_value) == -1) {
        return false;
    }
    
    config->current_usage.nice_value = nice_value;
    return true;
#else
    return false; // Windows not supported
#endif
}

bool ResourceManager::terminate_process_safely(int socket_fd, int timeout_ms) {
    auto config = get_resource_config(socket_fd);
    if (!config || config->pid == -1) return false;
    
    pid_t pid = config->pid;
    
    // Send SIGTERM first
    if (!send_signal_to_process(pid, SIGTERM)) {
        return false;
    }
    
    // Wait for graceful termination
    auto start_time = std::chrono::steady_clock::now();
    while (is_process_running(pid)) {
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start_time).count();
        
        if (elapsed > timeout_ms) {
            // Force kill with SIGKILL
            return send_signal_to_process(pid, SIGKILL);
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    return true;
}

bool ResourceManager::is_process_running(pid_t pid) const {
#ifndef _WIN32
    return kill(pid, 0) == 0;
#else
    return false; // Windows not supported
#endif
}

bool ResourceManager::send_signal_to_process(pid_t pid, int signal) {
#ifndef _WIN32
    return kill(pid, signal) == 0;
#else
    return false; // Windows not supported
#endif
}

bool ResourceManager::read_process_stat_info(pid_t pid, std::map<std::string, std::string>& info) {
#ifndef _WIN32
    std::string stat_file = "/proc/" + std::to_string(pid) + "/stat";
    std::ifstream file(stat_file);
    if (!file.is_open()) {
        return false;
    }
    
    std::string line;
    std::getline(file, line);
    
    // Parse the stat file format - simplified version
    std::istringstream iss(line);
    std::string token;
    std::vector<std::string> tokens;
    
    while (iss >> token) {
        tokens.push_back(token);
    }
    
    if (tokens.size() >= 15) {
        info["utime"] = tokens[13];  // User CPU time
        info["stime"] = tokens[14];  // System CPU time
    }
    
    return true;
#else
    return false; // Windows not supported
#endif
}

// Static instance management
ResourceManager& ResourceManager::instance() {
    if (!global_instance_) {
        global_instance_ = std::make_unique<ResourceManager>();
    }
    return *global_instance_;
}

bool ResourceManager::initialize_global_manager() {
    if (!global_instance_) {
        global_instance_ = std::make_unique<ResourceManager>();
        return true;
    }
    return false;
}

void ResourceManager::shutdown_global_manager() {
    global_instance_.reset();
}

/*
 * ResourceLimitManager Implementation
 */

bool ResourceLimitManager::handle_cpu_limit_option(int socket_fd, const std::string& limit_spec) {
    double soft_limit, hard_limit;
    EnforcementAction action;
    
    if (!parse_limit_specification(limit_spec, soft_limit, hard_limit, action)) {
        return false;
    }
    
    return ResourceManager::instance().configure_resource_limit(
        socket_fd, ResourceLimitType::CPU_TIME, soft_limit, hard_limit, action);
}

bool ResourceLimitManager::handle_memory_limit_option(int socket_fd, const std::string& limit_spec) {
    double soft_limit, hard_limit;
    EnforcementAction action;
    
    if (!parse_limit_specification(limit_spec, soft_limit, hard_limit, action)) {
        return false;
    }
    
    return ResourceManager::instance().configure_resource_limit(
        socket_fd, ResourceLimitType::MEMORY_VIRTUAL, soft_limit, hard_limit, action);
}

std::string ResourceLimitManager::limit_type_to_string(ResourceLimitType type) {
    switch (type) {
        case ResourceLimitType::CPU_TIME: return "cpu_time";
        case ResourceLimitType::CPU_PERCENT: return "cpu_percent";
        case ResourceLimitType::MEMORY_VIRTUAL: return "memory_virtual";
        case ResourceLimitType::MEMORY_RSS: return "memory_rss";
        case ResourceLimitType::FILE_DESCRIPTORS: return "file_descriptors";
        case ResourceLimitType::FILE_SIZE: return "file_size";
        case ResourceLimitType::NICE_VALUE: return "nice_value";
        case ResourceLimitType::WALL_TIME: return "wall_time";
        default: return "unknown";
    }
}

std::string ResourceLimitManager::sandbox_mode_to_string(SandboxMode mode) {
    switch (mode) {
        case SandboxMode::NONE: return "none";
        case SandboxMode::BASIC: return "basic";
        case SandboxMode::RESTRICTED: return "restricted";
        case SandboxMode::ISOLATED: return "isolated";
        case SandboxMode::STRICT: return "strict";
        default: return "unknown";
    }
}

bool ResourceLimitManager::parse_limit_specification(const std::string& limit_spec,
                                                    double& soft_limit, double& hard_limit,
                                                    EnforcementAction& action) {
    // Parse format: "soft:hard:action" or "hard:action" or "hard"
    std::regex pattern(R"(^(?:(\d+(?:\.\d+)?):)?(\d+(?:\.\d+)?):?(\w+)?$)");
    std::smatch match;
    
    if (!std::regex_match(limit_spec, match, pattern)) {
        return false;
    }
    
    if (match[1].matched) {
        soft_limit = std::stod(match[1]);
    } else {
        soft_limit = 0.0;
    }
    
    hard_limit = std::stod(match[2]);
    
    if (match[3].matched) {
        std::string action_str = match[3];
        action = string_to_enforcement_action(action_str);
    } else {
        action = EnforcementAction::WARN;
    }
    
    return hard_limit > 0.0 && soft_limit <= hard_limit;
}

EnforcementAction ResourceLimitManager::string_to_enforcement_action(const std::string& action_str) {
    if (action_str == "warn") return EnforcementAction::WARN;
    if (action_str == "throttle") return EnforcementAction::THROTTLE;
    if (action_str == "suspend") return EnforcementAction::SUSPEND;
    if (action_str == "terminate") return EnforcementAction::TERMINATE;
    return EnforcementAction::WARN;
}

/*
 * ResourceManagementUtils Implementation
 */

namespace ResourceManagementUtils {

std::string get_resource_limit_description(ResourceLimitType type, double limit) {
    std::ostringstream oss;
    
    switch (type) {
        case ResourceLimitType::CPU_TIME:
            oss << "CPU time limit: " << limit << " seconds";
            break;
        case ResourceLimitType::MEMORY_VIRTUAL:
            oss << "Virtual memory limit: " << (limit / (1024*1024)) << " MB";
            break;
        case ResourceLimitType::FILE_DESCRIPTORS:
            oss << "File descriptor limit: " << static_cast<int>(limit);
            break;
        default:
            oss << "Resource limit: " << limit;
            break;
    }
    
    return oss.str();
}

bool is_valid_limit_specification(const std::string& spec) {
    std::regex pattern(R"(^(?:\d+(?:\.\d+)?:)?\d+(?:\.\d+)?:?(?:warn|throttle|suspend|terminate)?$)");
    return std::regex_match(spec, pattern);
}

size_t get_system_memory_total() {
#ifndef _WIN32
    long pages = sysconf(_SC_PHYS_PAGES);
    long page_size = sysconf(_SC_PAGE_SIZE);
    return static_cast<size_t>(pages) * static_cast<size_t>(page_size);
#else
    return 0; // Windows not supported
#endif
}

int get_system_cpu_count() {
#ifndef _WIN32
    return sysconf(_SC_NPROCESSORS_ONLN);
#else
    return 0; // Windows not supported
#endif
}

}  // namespace ResourceManagementUtils

// Global initialization functions
bool init_resource_management_system() {
    return ResourceManager::initialize_global_manager();
}

void cleanup_resource_management_system() {
    ResourceManager::shutdown_global_manager();
}