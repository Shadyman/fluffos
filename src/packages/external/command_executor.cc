#include "command_executor.h"
#include "base/internal/log.h"
#include <algorithm>
#include <sstream>
#include <random>
#include <iomanip>
#include <regex>

/*
 * CommandExecutor implementation
 */

CommandExecutor& CommandExecutor::instance() {
    static CommandExecutor instance;
    return instance;
}

CommandExecutor::CommandExecutor() :
    executor_running_(false),
    max_concurrent_commands_(10),
    max_queue_size_(100),
    max_history_size_(1000) {
    
    debug(external, "CommandExecutor initialized");
    
    // Start worker threads
    start_worker_threads();
}

CommandExecutor::~CommandExecutor() {
    debug(external, "CommandExecutor shutting down");
    stop_worker_threads();
}

void CommandExecutor::start_worker_threads() {
    executor_running_ = true;
    
    // Start worker threads (limited number for now)
    size_t thread_count = std::min(max_concurrent_commands_, size_t(4));
    worker_threads_.reserve(thread_count);
    
    for (size_t i = 0; i < thread_count; ++i) {
        worker_threads_.emplace_back(&CommandExecutor::worker_thread_function, this);
    }
    
    debug(external, "Started %zu command executor worker threads", thread_count);
}

void CommandExecutor::stop_worker_threads() {
    executor_running_ = false;
    queue_cv_.notify_all();
    
    for (auto& thread : worker_threads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    
    worker_threads_.clear();
    debug(external, "Command executor worker threads stopped");
}

std::string CommandExecutor::execute_command(const CommandRequest& request) {
    debug(external, "Executing synchronous command: %s", request.command.c_str());
    
    // Validate request
    if (!validate_command_request(request)) {
        debug(external, "Command request validation failed");
        return "";
    }
    
    // Create a copy for processing
    auto request_copy = std::make_unique<CommandRequest>(request);
    request_copy->request_id = generate_request_id();
    request_copy->async_execution = false;
    
    // Execute directly (synchronous)
    auto result = execute_command_internal(request_copy.get());
    
    if (result) {
        // Store result
        store_result(std::move(result));
        return request_copy->request_id;
    }
    
    return "";
}

std::string CommandExecutor::execute_command_async(const CommandRequest& request) {
    debug(external, "Queuing asynchronous command: %s", request.command.c_str());
    
    // Validate request
    if (!validate_command_request(request)) {
        debug(external, "Command request validation failed");
        return "";
    }
    
    std::unique_lock<std::mutex> lock(queue_mutex_);
    
    // Check queue size
    if (command_queue_.size() >= max_queue_size_) {
        debug(external, "Command queue is full (%zu >= %zu)", command_queue_.size(), max_queue_size_);
        return "";
    }
    
    // Create request
    auto request_copy = std::make_unique<CommandRequest>(request);
    request_copy->request_id = generate_request_id();
    request_copy->async_execution = true;
    
    std::string request_id = request_copy->request_id;
    
    // Add to queue
    command_queue_.push(std::move(request_copy));
    
    debug(external, "Command queued: %s (queue size: %zu)", request_id.c_str(), command_queue_.size());
    
    // Notify worker threads
    queue_cv_.notify_one();
    
    return request_id;
}

bool CommandExecutor::cancel_command(const std::string& request_id) {
    std::unique_lock<std::mutex> lock(queue_mutex_);
    
    // Check if command is active
    auto it = active_commands_.find(request_id);
    if (it != active_commands_.end()) {
        // Command is currently executing - try to terminate the process
        int socket_fd = it->second->socket_fd;
        if (socket_fd >= 0) {
            ProcessManager::instance().terminate_process(socket_fd, SIGTERM);
            debug(external, "Cancelled active command: %s", request_id.c_str());
            return true;
        }
    }
    
    // Check if command is in queue
    std::queue<std::unique_ptr<CommandRequest>> new_queue;
    bool found = false;
    
    while (!command_queue_.empty()) {
        auto request = std::move(command_queue_.front());
        command_queue_.pop();
        
        if (request->request_id == request_id) {
            found = true;
            debug(external, "Cancelled queued command: %s", request_id.c_str());
        } else {
            new_queue.push(std::move(request));
        }
    }
    
    command_queue_ = std::move(new_queue);
    return found;
}

void CommandExecutor::worker_thread_function() {
    debug(external, "Command executor worker thread started");
    
    while (executor_running_) {
        std::unique_ptr<CommandRequest> request;
        
        // Wait for a command to process
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            queue_cv_.wait(lock, [this] { 
                return !executor_running_ || !command_queue_.empty(); 
            });
            
            if (!executor_running_) {
                break;
            }
            
            if (!command_queue_.empty()) {
                request = std::move(command_queue_.front());
                command_queue_.pop();
                
                // Add to active commands
                active_commands_[request->request_id] = std::move(request);
            }
        }
        
        if (request) {
            debug(external, "Worker processing command: %s", request->request_id.c_str());
            process_command_request(std::move(request));
        }
    }
    
    debug(external, "Command executor worker thread stopped");
}

void CommandExecutor::process_command_request(std::unique_ptr<CommandRequest> request) {
    auto result = execute_command_internal(request.get());
    
    // Remove from active commands
    {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        active_commands_.erase(request->request_id);
    }
    
    if (result) {
        // Store result
        store_result(std::move(result));
        
        // Notify completion
        notify_command_completion(*result);
    }
}

std::unique_ptr<CommandResult> CommandExecutor::execute_command_internal(CommandRequest* request) {
    auto result = std::make_unique<CommandResult>();
    result->request_id = request->request_id;
    result->socket_fd = request->socket_fd;
    result->start_time = time(nullptr);
    
    debug(external, "Executing command internally: %s", request->command.c_str());
    
    try {
        // Prepare execution
        if (!prepare_command_execution(request)) {
            result->success = false;
            result->error_message = "Failed to prepare command execution";
            result->end_time = time(nullptr);
            return result;
        }
        
        // Create process info
        auto process_info = std::make_unique<ExternalProcessInfo>();
        process_info->socket_fd = request->socket_fd;
        process_info->command = request->command;
        process_info->args = request->args;
        process_info->environment = request->environment;
        process_info->working_dir = request->working_dir;
        process_info->timeout_seconds = request->timeout_seconds;
        
        // Spawn process
        if (!ProcessManager::instance().spawn_process(request->socket_fd, process_info.release(), request->security)) {
            result->success = false;
            result->error_message = "Failed to spawn process";
            result->end_time = time(nullptr);
            return result;
        }
        
        // Wait for process completion (for synchronous execution)
        if (!request->async_execution) {
            while (ProcessManager::instance().is_process_running(request->socket_fd)) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                
                // Check for timeout
                time_t current_time = time(nullptr);
                if (current_time - result->start_time > request->timeout_seconds) {
                    ProcessManager::instance().terminate_process(request->socket_fd, SIGKILL);
                    result->success = false;
                    result->error_message = "Command timed out";
                    result->end_time = current_time;
                    return result;
                }
            }
            
            // Get exit code
            result->exit_code = ProcessManager::instance().get_process_exit_code(request->socket_fd);
            result->success = (result->exit_code == 0);
        }
        
        result->end_time = time(nullptr);
        result->execution_time = difftime(result->end_time, result->start_time);
        
        debug(external, "Command execution completed: %s (exit_code=%d, time=%.2fs)", 
              request->request_id.c_str(), result->exit_code, result->execution_time);
        
    } catch (const std::exception& e) {
        result->success = false;
        result->error_message = std::string("Exception during execution: ") + e.what();
        result->end_time = time(nullptr);
        debug(external, "Command execution failed with exception: %s", e.what());
    }
    
    return result;
}

bool CommandExecutor::validate_command_request(const CommandRequest& request) {
    // Basic validation
    if (request.command.empty()) {
        debug(external, "Command is empty");
        return false;
    }
    
    if (request.socket_fd < 0) {
        debug(external, "Invalid socket file descriptor: %d", request.socket_fd);
        return false;
    }
    
    if (request.timeout_seconds <= 0 || request.timeout_seconds > 3600) {
        debug(external, "Invalid timeout: %d seconds", request.timeout_seconds);
        return false;
    }
    
    // Security validation
    if (!CommandUtils::validate_command_security(request.command, request.security)) {
        debug(external, "Command failed security validation: %s", request.command.c_str());
        return false;
    }
    
    return true;
}

bool CommandExecutor::prepare_command_execution(CommandRequest* request) {
    // Resolve command path if needed
    if (request->command.find('/') == std::string::npos) {
        std::string full_path = CommandUtils::find_command_in_path(request->command);
        if (full_path.empty()) {
            debug(external, "Command not found in PATH: %s", request->command.c_str());
            return false;
        }
        request->command = full_path;
    }
    
    // Validate command is executable
    if (!CommandUtils::is_executable_file(request->command)) {
        debug(external, "Command is not executable: %s", request->command.c_str());
        return false;
    }
    
    // Resolve working directory
    if (!request->working_dir.empty()) {
        request->working_dir = CommandUtils::resolve_relative_path(request->working_dir, ".");
        if (!CommandUtils::is_safe_path(request->working_dir)) {
            debug(external, "Unsafe working directory: %s", request->working_dir.c_str());
            return false;
        }
    }
    
    debug(external, "Command preparation completed: %s", request->command.c_str());
    return true;
}

CommandResult* CommandExecutor::get_result(const std::string& request_id) {
    std::unique_lock<std::mutex> lock(results_mutex_);
    
    auto it = command_results_.find(request_id);
    if (it != command_results_.end()) {
        return it->second.get();
    }
    
    return nullptr;
}

void CommandExecutor::store_result(std::unique_ptr<CommandResult> result) {
    std::unique_lock<std::mutex> lock(results_mutex_);
    
    command_results_[result->request_id] = std::move(result);
    
    // Cleanup old results if needed
    if (command_results_.size() > max_history_size_) {
        cleanup_old_results();
    }
}

void CommandExecutor::cleanup_old_results() {
    // Remove oldest results to maintain size limit
    // This is a simple implementation - could be improved with LRU
    
    std::vector<std::pair<time_t, std::string>> results_by_time;
    
    for (const auto& [id, result] : command_results_) {
        results_by_time.emplace_back(result->end_time, id);
    }
    
    std::sort(results_by_time.begin(), results_by_time.end());
    
    // Remove oldest entries
    size_t to_remove = command_results_.size() - max_history_size_ + 10; // Remove a few extra
    for (size_t i = 0; i < to_remove && i < results_by_time.size(); ++i) {
        command_results_.erase(results_by_time[i].second);
    }
    
    debug(external, "Cleaned up %zu old command results", to_remove);
}

void CommandExecutor::notify_command_completion(const CommandResult& result) {
    debug(external, "Command completed: %s (success=%s, exit_code=%d)", 
          result.request_id.c_str(), result.success ? "true" : "false", result.exit_code);
    
    // Here we would integrate with the LPC callback system
    // to notify the requesting object about command completion
}

std::string CommandExecutor::generate_request_id() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, 15);
    
    std::stringstream ss;
    ss << "cmd_";
    
    // Generate a random hex string
    for (int i = 0; i < 16; ++i) {
        ss << std::hex << dis(gen);
    }
    
    return ss.str();
}

size_t CommandExecutor::get_queue_size() const {
    std::unique_lock<std::mutex> lock(queue_mutex_);
    return command_queue_.size();
}

size_t CommandExecutor::get_active_commands() const {
    std::unique_lock<std::mutex> lock(queue_mutex_);
    return active_commands_.size();
}

/*
 * CommandBuilder implementation
 */

CommandBuilder::CommandBuilder(int socket_fd) {
    request_.socket_fd = socket_fd;
    request_.security = CommandExecutor::instance().get_default_security_context();
}

CommandBuilder& CommandBuilder::command(const std::string& cmd) {
    request_.command = cmd;
    return *this;
}

CommandBuilder& CommandBuilder::args(const std::vector<std::string>& arguments) {
    request_.args = arguments;
    return *this;
}

CommandBuilder& CommandBuilder::arg(const std::string& argument) {
    request_.args.push_back(argument);
    return *this;
}

CommandBuilder& CommandBuilder::env(const std::string& name, const std::string& value) {
    request_.environment[name] = value;
    return *this;
}

CommandBuilder& CommandBuilder::working_dir(const std::string& dir) {
    request_.working_dir = dir;
    return *this;
}

CommandBuilder& CommandBuilder::timeout(int seconds) {
    request_.timeout_seconds = seconds;
    return *this;
}

CommandBuilder& CommandBuilder::async(bool asynchronous) {
    request_.async_execution = asynchronous;
    return *this;
}

std::string CommandBuilder::execute() {
    request_.async_execution = false;
    return CommandExecutor::instance().execute_command(request_);
}

std::string CommandBuilder::execute_async() {
    request_.async_execution = true;
    return CommandExecutor::instance().execute_command_async(request_);
}

CommandRequest CommandBuilder::build() {
    return request_;
}

/*
 * CommandUtils implementation
 */

namespace CommandUtils {

bool is_valid_command(const std::string& command) {
    if (command.empty()) return false;
    
    // Check for dangerous characters
    const std::string dangerous_chars = ";|&`$(){}[]<>\"'\\";
    for (char c : dangerous_chars) {
        if (command.find(c) != std::string::npos) {
            return false;
        }
    }
    
    return true;
}

bool is_executable_file(const std::string& path) {
#ifndef _WIN32
    return access(path.c_str(), X_OK) == 0;
#else
    // Windows implementation would check for .exe, .bat, .cmd extensions
    return true; // Simplified for now
#endif
}

std::string find_command_in_path(const std::string& command) {
    const char* path_env = getenv("PATH");
    if (!path_env) return "";
    
    std::string path_str(path_env);
    std::istringstream path_stream(path_str);
    std::string path_dir;
    
    while (std::getline(path_stream, path_dir, ':')) {
        std::string full_path = path_dir + "/" + command;
        if (is_executable_file(full_path)) {
            return full_path;
        }
    }
    
    return "";
}

std::vector<std::string> parse_command_line(const std::string& cmdline) {
    std::vector<std::string> args;
    std::istringstream iss(cmdline);
    std::string arg;
    
    while (iss >> arg) {
        args.push_back(arg);
    }
    
    return args;
}

std::string escape_argument(const std::string& arg) {
    std::string escaped = arg;
    
    // Escape special characters
    const std::vector<std::pair<std::string, std::string>> replacements = {
        {"\\", "\\\\"},
        {"\"", "\\\""},
        {"'", "\\'"},
        {"`", "\\`"},
        {"$", "\\$"}
    };
    
    for (const auto& [search, replace] : replacements) {
        size_t pos = 0;
        while ((pos = escaped.find(search, pos)) != std::string::npos) {
            escaped.replace(pos, search.length(), replace);
            pos += replace.length();
        }
    }
    
    return escaped;
}

bool validate_command_security(const std::string& command, const SecurityContext& security) {
    // Check against allowed commands
    if (!security.allowed_commands.empty()) {
        auto it = std::find(security.allowed_commands.begin(), security.allowed_commands.end(), command);
        if (it == security.allowed_commands.end()) {
            return false;
        }
    }
    
    // Check against blocked commands
    auto it = std::find(security.blocked_commands.begin(), security.blocked_commands.end(), command);
    if (it != security.blocked_commands.end()) {
        return false;
    }
    
    return is_valid_command(command);
}

bool is_safe_path(const std::string& path) {
    // Check for path traversal
    if (path.find("../") != std::string::npos || path.find("..\\") != std::string::npos) {
        return false;
    }
    
    // Check for absolute paths that might be dangerous
    if (path.empty() || path[0] == '/') {
        // Only allow certain safe absolute paths
        const std::vector<std::string> safe_prefixes = {
            "/tmp/", "/var/tmp/", "/home/", "/usr/local/"
        };
        
        for (const auto& prefix : safe_prefixes) {
            if (path.substr(0, prefix.length()) == prefix) {
                return true;
            }
        }
        
        return false; // Unsafe absolute path
    }
    
    return true; // Relative path is generally safe
}

SecurityContext create_restricted_security_context() {
    SecurityContext ctx;
    ctx.enable_sandbox = true;
    ctx.drop_privileges = true;
    ctx.max_memory = 50 * 1024 * 1024; // 50MB
    ctx.max_cpu_time = 10; // 10 seconds
    ctx.max_processes = 5;
    ctx.max_open_files = 32;
    
    // Add some safe commands
    ctx.allowed_commands = {
        "/bin/echo", "/bin/cat", "/bin/ls", "/bin/pwd",
        "/usr/bin/wc", "/usr/bin/head", "/usr/bin/tail"
    };
    
    return ctx;
}

std::string resolve_relative_path(const std::string& path, const std::string& base_dir) {
    // Simple implementation - in a real system this would handle complex path resolution
    if (path.empty() || path[0] == '/') {
        return path; // Already absolute
    }
    
    std::string result = base_dir;
    if (!result.empty() && result.back() != '/') {
        result += "/";
    }
    result += path;
    
    return result;
}

} // namespace CommandUtils