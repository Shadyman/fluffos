#ifndef PACKAGES_EXTERNAL_COMMAND_EXECUTOR_H_
#define PACKAGES_EXTERNAL_COMMAND_EXECUTOR_H_

#include "base/package_api.h"
#include "external.h"
#include "process_manager.h"
#include <string>
#include <vector>
#include <map>
#include <queue>
#include <memory>
#include <mutex>
#include <condition_variable>
#include <atomic>

/*
 * CommandExecutor - High-level command execution engine
 * 
 * This class provides a high-level interface for executing external
 * commands with comprehensive validation, queuing, and result handling.
 * It builds on top of ProcessManager to provide:
 * 
 * - Command queue management
 * - Batch execution capabilities
 * - Command templates and macros
 * - Result caching and history
 * - Async execution with callbacks
 * - Command chaining and pipelines
 */

// Command execution request
struct CommandRequest {
    int socket_fd;
    std::string command;
    std::vector<std::string> args;
    std::map<std::string, std::string> environment;
    std::string working_dir;
    std::string input_data;
    
    // Execution options
    int timeout_seconds;
    bool capture_output;
    bool async_execution;
    int priority;
    
    // Callbacks
    std::string success_callback;
    std::string error_callback;
    std::string progress_callback;
    
    // Security context
    SecurityContext security;
    
    // Metadata
    std::string request_id;
    time_t created_time;
    std::string requester_object;
    
    CommandRequest() :
        socket_fd(-1), timeout_seconds(30), capture_output(true),
        async_execution(false), priority(0), created_time(time(nullptr)) {}
};

// Command execution result
struct CommandResult {
    std::string request_id;
    int socket_fd;
    bool success;
    int exit_code;
    std::string stdout_data;
    std::string stderr_data;
    std::string error_message;
    
    // Timing information
    time_t start_time;
    time_t end_time;
    double execution_time;
    
    // Resource usage
    size_t peak_memory_usage;
    double cpu_time_used;
    
    CommandResult() :
        socket_fd(-1), success(false), exit_code(-1),
        start_time(0), end_time(0), execution_time(0.0),
        peak_memory_usage(0), cpu_time_used(0.0) {}
};

// Command template for reusable command patterns
struct CommandTemplate {
    std::string name;
    std::string command_pattern;
    std::vector<std::string> default_args;
    std::map<std::string, std::string> default_env;
    SecurityContext security_template;
    int default_timeout;
    std::string description;
    
    CommandTemplate() : default_timeout(30) {}
};

/*
 * CommandExecutor - Main command execution engine
 */
class CommandExecutor {
public:
    static CommandExecutor& instance();
    
    // Command execution
    std::string execute_command(const CommandRequest& request);
    std::string execute_command_async(const CommandRequest& request);
    bool cancel_command(const std::string& request_id);
    
    // Command templates
    bool register_template(const CommandTemplate& tmpl);
    bool unregister_template(const std::string& name);
    CommandTemplate* get_template(const std::string& name);
    std::string execute_template(const std::string& template_name, 
                                const std::map<std::string, std::string>& params,
                                int socket_fd);
    
    // Queue management
    size_t get_queue_size() const;
    size_t get_active_commands() const;
    void set_max_concurrent_commands(size_t max_concurrent);
    void set_max_queue_size(size_t max_queue);
    
    // Command history and results
    CommandResult* get_result(const std::string& request_id);
    std::vector<CommandResult> get_command_history(int socket_fd = -1, size_t limit = 100);
    void clear_command_history(int socket_fd = -1);
    
    // Status and monitoring
    void dump_status(outbuffer_t* buffer) const;
    array_t* get_active_commands_info() const;
    array_t* get_queue_info() const;
    
    // Configuration
    void set_default_security_context(const SecurityContext& security);
    SecurityContext get_default_security_context() const;
    
private:
    CommandExecutor();
    ~CommandExecutor();
    
    // Command queue
    std::queue<std::unique_ptr<CommandRequest>> command_queue_;
    std::map<std::string, std::unique_ptr<CommandRequest>> active_commands_;
    std::map<std::string, std::unique_ptr<CommandResult>> command_results_;
    
    // Templates
    std::map<std::string, std::unique_ptr<CommandTemplate>> templates_;
    
    // Thread management
    std::vector<std::thread> worker_threads_;
    std::atomic<bool> executor_running_;
    mutable std::mutex queue_mutex_;
    std::condition_variable queue_cv_;
    mutable std::mutex results_mutex_;
    mutable std::mutex templates_mutex_;
    
    // Configuration
    size_t max_concurrent_commands_;
    size_t max_queue_size_;
    size_t max_history_size_;
    SecurityContext default_security_;
    
    // Worker thread functions
    void worker_thread_function();
    void process_command_request(std::unique_ptr<CommandRequest> request);
    
    // Command processing helpers
    bool validate_command_request(const CommandRequest& request);
    bool prepare_command_execution(CommandRequest* request);
    std::unique_ptr<CommandResult> execute_command_internal(CommandRequest* request);
    
    // Template processing
    std::string expand_template(const std::string& pattern, const std::map<std::string, std::string>& params);
    bool validate_template_params(const CommandTemplate& tmpl, const std::map<std::string, std::string>& params);
    
    // Result management
    void store_result(std::unique_ptr<CommandResult> result);
    void cleanup_old_results();
    void notify_command_completion(const CommandResult& result);
    
    // Utility functions
    std::string generate_request_id();
    bool should_queue_command() const;
    void start_worker_threads();
    void stop_worker_threads();
};

/*
 * CommandBuilder - Fluent interface for building command requests
 */
class CommandBuilder {
public:
    CommandBuilder(int socket_fd);
    
    // Command specification
    CommandBuilder& command(const std::string& cmd);
    CommandBuilder& args(const std::vector<std::string>& arguments);
    CommandBuilder& arg(const std::string& argument);
    CommandBuilder& env(const std::string& name, const std::string& value);
    CommandBuilder& env(const std::map<std::string, std::string>& environment);
    CommandBuilder& working_dir(const std::string& dir);
    CommandBuilder& input(const std::string& data);
    
    // Execution options
    CommandBuilder& timeout(int seconds);
    CommandBuilder& capture_output(bool capture = true);
    CommandBuilder& async(bool asynchronous = true);
    CommandBuilder& priority(int prio);
    
    // Callbacks
    CommandBuilder& on_success(const std::string& callback);
    CommandBuilder& on_error(const std::string& callback);
    CommandBuilder& on_progress(const std::string& callback);
    
    // Security
    CommandBuilder& security(const SecurityContext& ctx);
    CommandBuilder& allow_command(const std::string& cmd);
    CommandBuilder& block_command(const std::string& cmd);
    CommandBuilder& memory_limit(size_t bytes);
    CommandBuilder& cpu_limit(int seconds);
    
    // Build and execute
    std::string execute();
    std::string execute_async();
    CommandRequest build();
    
private:
    CommandRequest request_;
};

/*
 * CommandPipeline - Command chaining and pipeline execution
 */
class CommandPipeline {
public:
    CommandPipeline(int socket_fd);
    
    // Pipeline building
    CommandPipeline& add_command(const CommandRequest& request);
    CommandPipeline& add_command(const std::string& command, const std::vector<std::string>& args = {});
    CommandPipeline& pipe_to(const std::string& command, const std::vector<std::string>& args = {});
    CommandPipeline& redirect_to_file(const std::string& filename);
    CommandPipeline& redirect_from_file(const std::string& filename);
    
    // Pipeline configuration
    CommandPipeline& fail_fast(bool enabled = true);
    CommandPipeline& timeout(int seconds);
    CommandPipeline& working_dir(const std::string& dir);
    
    // Execution
    std::string execute();
    std::string execute_async();
    
    // Status
    size_t size() const { return commands_.size(); }
    void clear() { commands_.clear(); }
    
private:
    int socket_fd_;
    std::vector<CommandRequest> commands_;
    bool fail_fast_enabled_;
    int pipeline_timeout_;
    std::string pipeline_working_dir_;
    
    // Pipeline execution helpers
    std::unique_ptr<CommandResult> execute_pipeline_internal();
    bool setup_pipeline_io();
    void cleanup_pipeline_io();
};

/*
 * CommandHistory - Command execution history and analysis
 */
class CommandHistory {
public:
    static CommandHistory& instance();
    
    // History management
    void add_result(const CommandResult& result);
    std::vector<CommandResult> get_history(int socket_fd = -1, size_t limit = 100);
    void clear_history(int socket_fd = -1);
    
    // Statistics
    size_t get_total_commands() const;
    size_t get_successful_commands() const;
    size_t get_failed_commands() const;
    double get_average_execution_time() const;
    
    // Analysis
    std::map<std::string, size_t> get_command_frequency();
    std::map<int, size_t> get_exit_code_frequency();
    std::vector<CommandResult> get_slowest_commands(size_t limit = 10);
    std::vector<CommandResult> get_failed_commands(size_t limit = 100);
    
    // Persistence
    bool save_history_to_file(const std::string& filename);
    bool load_history_from_file(const std::string& filename);
    
private:
    CommandHistory() = default;
    
    std::vector<CommandResult> history_;
    mutable std::mutex history_mutex_;
    size_t max_history_size_;
    
    void cleanup_old_entries();
};

/*
 * Utility functions for command execution
 */
namespace CommandUtils {
    // Command validation
    bool is_valid_command(const std::string& command);
    bool is_executable_file(const std::string& path);
    std::string find_command_in_path(const std::string& command);
    
    // Argument processing
    std::vector<std::string> parse_command_line(const std::string& cmdline);
    std::string escape_argument(const std::string& arg);
    std::string quote_argument(const std::string& arg);
    
    // Environment processing
    std::string expand_environment_variables(const std::string& input, 
                                           const std::map<std::string, std::string>& env);
    
    // Path utilities
    std::string resolve_relative_path(const std::string& path, const std::string& base_dir);
    bool is_safe_path(const std::string& path);
    
    // Security utilities
    bool validate_command_security(const std::string& command, const SecurityContext& security);
    SecurityContext create_restricted_security_context();
    SecurityContext create_sandbox_security_context();
}

#endif  // PACKAGES_EXTERNAL_COMMAND_EXECUTOR_H_