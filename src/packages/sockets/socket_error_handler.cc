#include "socket_error_handler.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <chrono>
#include <thread>
#include <algorithm>
#include <cstring>

// Global error handler instance
SocketErrorHandler* g_socket_error_handler = nullptr;

/*
 * SocketErrorHandler Implementation
 */

SocketErrorHandler::SocketErrorHandler() {
    // Initialize with default configuration
    initialize(ErrorHandlerConfig());
    
    // Register default recovery handlers
    register_recovery_handler(ERROR_CONNECTION_TIMEOUT, 
        [this](SocketError& error) { return handle_connection_error(error); });
    register_recovery_handler(ERROR_CONNECTION_FAILED,
        [this](SocketError& error) { return handle_connection_error(error); });
    register_recovery_handler(ERROR_INVALID_OPTION,
        [this](SocketError& error) { return handle_validation_error(error); });
    register_recovery_handler(ERROR_AUTH_TOKEN_EXPIRED,
        [this](SocketError& error) { return handle_authentication_error(error); });
    register_recovery_handler(ERROR_CONNECTION_LIMIT_EXCEEDED,
        [this](SocketError& error) { return handle_resource_error(error); });
    register_recovery_handler(ERROR_OPERATION_TIMEOUT,
        [this](SocketError& error) { return handle_timeout_error(error); });
}

SocketErrorHandler::~SocketErrorHandler() {
    clear_error_cache();
}

void SocketErrorHandler::initialize(const ErrorHandlerConfig& config) {
    config_ = config;
    
    // Initialize error cache
    recent_errors_.reserve(config_.error_cache_size);
    
    // Set up logging
    if (config_.enable_logging && !config_.log_file_path.empty()) {
        log_file_path_ = config_.log_file_path;
    }
    
    // Initialize metrics
    metrics_enabled_ = config_.enable_metrics;
    
    // Reset statistics
    reset_statistics();
}

SocketError SocketErrorHandler::report_error(SocketErrorCode error_code,
                                            const std::string& message,
                                            const std::string& context) {
    SocketError error;
    error.error_code = error_code;
    error.category = classify_error(error_code);
    error.severity = determine_severity(error_code);
    error.recovery_strategy = suggest_recovery(error_code);
    error.message = message;
    error.technical_details = generate_technical_details(error);
    error.suggestion = generate_suggestion(error_code);
    error.context = context;
    error.timestamp_ms = get_current_time_ms();
    error.first_occurrence_ms = error.timestamp_ms;
    
    // Update statistics
    update_statistics(error);
    
    // Log error if enabled
    if (config_.enable_logging) {
        log_error(error);
    }
    
    // Aggregate error if enabled
    if (config_.enable_aggregation) {
        aggregate_error(error);
    }
    
    // Cache recent error
    recent_errors_.push_back(error);
    if (recent_errors_.size() > config_.error_cache_size) {
        recent_errors_.erase(recent_errors_.begin());
    }
    
    // Attempt recovery if enabled
    if (config_.enable_recovery) {
        handle_error(error);
    }
    
    return error;
}

SocketError SocketErrorHandler::report_validation_error(const ValidationResult& validation_result,
                                                       int socket_fd,
                                                       const std::string& context) {
    SocketErrorCode error_code;
    
    // Map validation error type to socket error code
    switch (validation_result.error_type) {
        case VALIDATION_ERROR_INVALID_OPTION:
            error_code = ERROR_INVALID_OPTION;
            break;
        case VALIDATION_ERROR_INVALID_TYPE:
        case VALIDATION_ERROR_OUT_OF_RANGE:
        case VALIDATION_ERROR_INVALID_FORMAT:
            error_code = ERROR_INVALID_VALUE;
            break;
        case VALIDATION_ERROR_MISSING_DEPENDENCY:
            error_code = ERROR_MISSING_REQUIRED_OPTION;
            break;
        case VALIDATION_ERROR_CONFLICTING_OPTION:
            error_code = ERROR_CONFLICTING_OPTIONS;
            break;
        case VALIDATION_ERROR_ACCESS_DENIED:
            error_code = ERROR_ACCESS_DENIED;
            break;
        case VALIDATION_ERROR_PROTOCOL_MISMATCH:
            error_code = ERROR_PROTOCOL_MISMATCH;
            break;
        case VALIDATION_ERROR_SECURITY_VIOLATION:
            error_code = ERROR_AUTH_INSUFFICIENT_PRIVILEGES;
            break;
        default:
            error_code = ERROR_INVALID_VALUE;
            break;
    }
    
    SocketError error = report_error(error_code, validation_result.error_message, context);
    error.socket_fd = socket_fd;
    error.suggestion = validation_result.suggestion;
    
    // Map validation severity to error severity
    switch (validation_result.severity) {
        case VALIDATION_SEVERITY_INFO:
            error.severity = ERROR_SEVERITY_INFO;
            break;
        case VALIDATION_SEVERITY_WARNING:
            error.severity = ERROR_SEVERITY_WARNING;
            break;
        case VALIDATION_SEVERITY_ERROR:
            error.severity = ERROR_SEVERITY_ERROR;
            break;
        case VALIDATION_SEVERITY_FATAL:
            error.severity = ERROR_SEVERITY_FATAL;
            break;
    }
    
    return error;
}

SocketError SocketErrorHandler::report_system_error(int system_errno,
                                                   const std::string& operation,
                                                   int socket_fd) {
    SocketErrorCode error_code = ERROR_SYSTEM_CALL_FAILED;
    
    // Map common system errors to specific error codes
    switch (system_errno) {
        case ECONNREFUSED:
            error_code = ERROR_CONNECTION_REFUSED;
            break;
        case ETIMEDOUT:
            error_code = ERROR_CONNECTION_TIMEOUT;
            break;
        case ECONNRESET:
            error_code = ERROR_CONNECTION_RESET;
            break;
        case ECONNABORTED:
            error_code = ERROR_CONNECTION_ABORTED;
            break;
        case EHOSTUNREACH:
            error_code = ERROR_HOST_UNREACHABLE;
            break;
        case ENETUNREACH:
            error_code = ERROR_NETWORK_UNREACHABLE;
            break;
        case EACCES:
            error_code = ERROR_PERMISSION_DENIED;
            break;
        case ENOENT:
            error_code = ERROR_FILE_NOT_FOUND;
            break;
        case EMFILE:
        case ENFILE:
            error_code = ERROR_FILE_DESCRIPTOR_LIMIT;
            break;
        case ENOMEM:
            error_code = ERROR_MEMORY_EXHAUSTED;
            break;
        case EBUSY:
            error_code = ERROR_DEVICE_BUSY;
            break;
    }
    
    std::string message = operation + " failed: " + std::string(strerror(system_errno));
    
    SocketError error = report_error(error_code, message, "system_call");
    error.socket_fd = socket_fd;
    error.system_errno = system_errno;
    
    return error;
}

SocketError SocketErrorHandler::report_protocol_error(SocketErrorCode error_code,
                                                     const std::string& protocol_details,
                                                     int socket_fd) {
    std::string context = "protocol_error";
    
    SocketError error = report_error(error_code, protocol_details, context);
    error.socket_fd = socket_fd;
    error.technical_details = protocol_details;
    
    return error;
}

bool SocketErrorHandler::handle_error(SocketError& error) {
    // Check if recovery is enabled and appropriate
    if (!config_.enable_recovery) {
        return false;
    }
    
    // Skip recovery for informational and debug errors
    if (error.severity <= ERROR_SEVERITY_INFO) {
        return true;
    }
    
    // Skip recovery if already attempted
    if (error.recovery_attempted) {
        return error.recovery_successful;
    }
    
    // Attempt recovery
    bool recovery_result = attempt_recovery(error);
    
    // Update error with recovery information
    error.recovery_attempted = true;
    error.recovery_successful = recovery_result;
    
    if (recovery_result) {
        stats_.successful_recoveries++;
        error.recovery_details = "Recovery successful";
    } else {
        error.recovery_details = "Recovery failed";
    }
    
    return recovery_result;
}

bool SocketErrorHandler::attempt_recovery(SocketError& error) {
    stats_.recovery_attempts++;
    
    // Look for specific recovery handler
    auto handler_it = recovery_handlers_.find(error.error_code);
    if (handler_it != recovery_handlers_.end()) {
        return handler_it->second(error);
    }
    
    // Default recovery based on recovery strategy
    switch (error.recovery_strategy) {
        case RECOVERY_RETRY:
            return retry_operation(error, [&error]() {
                // Generic retry - would need specific operation context
                return false; // Placeholder
            });
            
        case RECOVERY_RECONNECT:
            // Implement reconnection logic
            if (error.socket_fd >= 0) {
                // Would implement socket reconnection here
                return false; // Placeholder
            }
            break;
            
        case RECOVERY_FALLBACK:
            // Implement fallback configuration
            return true; // Assume fallback always works for now
            
        case RECOVERY_NONE:
        default:
            return false;
    }
    
    return false;
}

bool SocketErrorHandler::retry_operation(SocketError& error, std::function<bool()> operation) {
    int max_retries = config_.max_retry_attempts;
    int retry_delay = config_.retry_delay_ms;
    
    for (int attempt = 1; attempt <= max_retries; attempt++) {
        error.retry_count = attempt;
        
        // Wait before retry (except for first attempt)
        if (attempt > 1) {
            std::this_thread::sleep_for(std::chrono::milliseconds(retry_delay));
        }
        
        // Attempt operation
        if (operation()) {
            error.recovery_details = "Succeeded after " + std::to_string(attempt) + " attempts";
            return true;
        }
        
        // Exponential backoff for retry delay
        retry_delay = std::min(retry_delay * 2, 30000); // Max 30 seconds
    }
    
    error.recovery_details = "Failed after " + std::to_string(max_retries) + " attempts";
    return false;
}

std::vector<SocketError> SocketErrorHandler::get_recent_errors(int limit) const {
    if (limit <= 0 || limit >= recent_errors_.size()) {
        return recent_errors_;
    }
    
    // Return the most recent errors
    std::vector<SocketError> result;
    int start_index = std::max(0, static_cast<int>(recent_errors_.size()) - limit);
    
    for (size_t i = start_index; i < recent_errors_.size(); ++i) {
        result.push_back(recent_errors_[i]);
    }
    
    return result;
}

std::map<SocketErrorCode, int> SocketErrorHandler::get_error_summary() const {
    return stats_.error_counts;
}

void SocketErrorHandler::clear_error_cache() {
    recent_errors_.clear();
    error_aggregation_.clear();
}

void SocketErrorHandler::reset_statistics() {
    stats_ = ErrorStatistics();
}

void SocketErrorHandler::register_recovery_handler(SocketErrorCode error_code,
                                                   std::function<bool(SocketError&)> handler) {
    recovery_handlers_[error_code] = handler;
}

std::string SocketErrorHandler::format_error_message(const SocketError& error, 
                                                    bool include_technical) const {
    std::stringstream ss;
    
    // Basic error information
    ss << "[" << get_error_severity_name(error.severity) << "] ";
    ss << "Error " << error.error_code << ": " << error.message;
    
    // Context information
    if (!error.context.empty()) {
        ss << " (Context: " << error.context << ")";
    }
    
    // Socket information
    if (error.socket_fd >= 0) {
        ss << " (Socket FD: " << error.socket_fd << ")";
    }
    
    // Suggestion
    if (!error.suggestion.empty()) {
        ss << "\nSuggestion: " << error.suggestion;
    }
    
    // Technical details (if requested)
    if (include_technical && !error.technical_details.empty()) {
        ss << "\nTechnical Details: " << error.technical_details;
    }
    
    // Recovery information
    if (error.recovery_attempted) {
        ss << "\nRecovery: " << (error.recovery_successful ? "Successful" : "Failed");
        if (!error.recovery_details.empty()) {
            ss << " (" << error.recovery_details << ")";
        }
    }
    
    return ss.str();
}

std::string SocketErrorHandler::get_error_category_name(SocketErrorCategory category) const {
    switch (category) {
        case ERROR_CATEGORY_VALIDATION: return "VALIDATION";
        case ERROR_CATEGORY_CONNECTION: return "CONNECTION";
        case ERROR_CATEGORY_PROTOCOL: return "PROTOCOL";
        case ERROR_CATEGORY_AUTHENTICATION: return "AUTHENTICATION";
        case ERROR_CATEGORY_RESOURCE: return "RESOURCE";
        case ERROR_CATEGORY_CONFIGURATION: return "CONFIGURATION";
        case ERROR_CATEGORY_SYSTEM: return "SYSTEM";
        case ERROR_CATEGORY_TIMEOUT: return "TIMEOUT";
        case ERROR_CATEGORY_DATA: return "DATA";
        case ERROR_CATEGORY_INTERNAL: return "INTERNAL";
        default: return "UNKNOWN";
    }
}

std::string SocketErrorHandler::get_error_severity_name(SocketErrorSeverity severity) const {
    switch (severity) {
        case ERROR_SEVERITY_DEBUG: return "DEBUG";
        case ERROR_SEVERITY_INFO: return "INFO";
        case ERROR_SEVERITY_WARNING: return "WARNING";
        case ERROR_SEVERITY_ERROR: return "ERROR";
        case ERROR_SEVERITY_CRITICAL: return "CRITICAL";
        case ERROR_SEVERITY_FATAL: return "FATAL";
        default: return "UNKNOWN";
    }
}

std::string SocketErrorHandler::get_recovery_strategy_name(SocketErrorRecovery strategy) const {
    switch (strategy) {
        case RECOVERY_NONE: return "NONE";
        case RECOVERY_RETRY: return "RETRY";
        case RECOVERY_FALLBACK: return "FALLBACK";
        case RECOVERY_RECONNECT: return "RECONNECT";
        case RECOVERY_RESTART: return "RESTART";
        case RECOVERY_ESCALATE: return "ESCALATE";
        case RECOVERY_GRACEFUL_SHUTDOWN: return "GRACEFUL_SHUTDOWN";
        default: return "UNKNOWN";
    }
}

SocketErrorCategory SocketErrorHandler::classify_error(SocketErrorCode error_code) const {
    // Classify errors by code ranges
    if (error_code >= 1000 && error_code < 1100) return ERROR_CATEGORY_VALIDATION;
    if (error_code >= 1100 && error_code < 1200) return ERROR_CATEGORY_CONNECTION;
    if (error_code >= 1200 && error_code < 1300) return ERROR_CATEGORY_PROTOCOL;
    if (error_code >= 1300 && error_code < 1400) return ERROR_CATEGORY_AUTHENTICATION;
    if (error_code >= 1400 && error_code < 1500) return ERROR_CATEGORY_RESOURCE;
    if (error_code >= 1500 && error_code < 1600) return ERROR_CATEGORY_CONFIGURATION;
    if (error_code >= 1600 && error_code < 1700) return ERROR_CATEGORY_SYSTEM;
    if (error_code >= 1700 && error_code < 1800) return ERROR_CATEGORY_TIMEOUT;
    if (error_code >= 1800 && error_code < 1900) return ERROR_CATEGORY_DATA;
    return ERROR_CATEGORY_INTERNAL;
}

SocketErrorSeverity SocketErrorHandler::determine_severity(SocketErrorCode error_code) const {
    // Determine severity based on error code
    switch (error_code) {
        // Fatal errors
        case ERROR_MEMORY_EXHAUSTED:
        case ERROR_ASSERTION_FAILED:
        case ERROR_TLS_CERTIFICATE_INVALID:
            return ERROR_SEVERITY_FATAL;
            
        // Critical errors  
        case ERROR_CONNECTION_FAILED:
        case ERROR_AUTH_INSUFFICIENT_PRIVILEGES:
        case ERROR_FILE_DESCRIPTOR_LIMIT:
        case ERROR_INVALID_STATE:
            return ERROR_SEVERITY_CRITICAL;
            
        // Regular errors
        case ERROR_INVALID_OPTION:
        case ERROR_INVALID_VALUE:
        case ERROR_CONNECTION_TIMEOUT:
        case ERROR_HTTP_NOT_FOUND:
        case ERROR_AUTH_TOKEN_INVALID:
            return ERROR_SEVERITY_ERROR;
            
        // Warnings
        case ERROR_RATE_LIMIT_EXCEEDED:
        case ERROR_CONNECTION_RESET:
            return ERROR_SEVERITY_WARNING;
            
        default:
            return ERROR_SEVERITY_ERROR;
    }
}

SocketErrorRecovery SocketErrorHandler::suggest_recovery(SocketErrorCode error_code) const {
    switch (error_code) {
        // Retry-able errors
        case ERROR_CONNECTION_TIMEOUT:
        case ERROR_CONNECTION_FAILED:
        case ERROR_DNS_RESOLUTION_FAILED:
        case ERROR_OPERATION_TIMEOUT:
            return RECOVERY_RETRY;
            
        // Reconnection errors
        case ERROR_CONNECTION_RESET:
        case ERROR_CONNECTION_ABORTED:
        case ERROR_NETWORK_UNREACHABLE:
            return RECOVERY_RECONNECT;
            
        // Fallback errors
        case ERROR_AUTH_TOKEN_EXPIRED:
        case ERROR_TLS_HANDSHAKE_FAILED:
        case ERROR_HTTP_SERVER_ERROR:
            return RECOVERY_FALLBACK;
            
        // No recovery
        case ERROR_INVALID_OPTION:
        case ERROR_INVALID_VALUE:
        case ERROR_ACCESS_DENIED:
        case ERROR_PERMISSION_DENIED:
            return RECOVERY_NONE;
            
        // Critical errors requiring escalation
        case ERROR_MEMORY_EXHAUSTED:
        case ERROR_FILE_DESCRIPTOR_LIMIT:
        case ERROR_ASSERTION_FAILED:
            return RECOVERY_ESCALATE;
            
        default:
            return RECOVERY_NONE;
    }
}

void SocketErrorHandler::log_error(const SocketError& error) {
    if (log_file_path_.empty()) {
        // Log to stderr if no file specified
        std::cerr << format_error_message(error, true) << std::endl;
        return;
    }
    
    std::ofstream log_file(log_file_path_, std::ios::app);
    if (log_file.is_open()) {
        log_file << "[" << error.timestamp_ms << "] "
                 << format_error_message(error, true) << std::endl;
        log_file.close();
    }
}

void SocketErrorHandler::update_statistics(const SocketError& error) {
    stats_.total_errors++;
    stats_.error_counts[error.error_code]++;
    stats_.category_counts[error.category]++;
    stats_.severity_counts[error.severity]++;
    stats_.last_error_time = error.timestamp_ms;
}

void SocketErrorHandler::aggregate_error(const SocketError& error) {
    auto it = error_aggregation_.find(error.error_code);
    if (it != error_aggregation_.end()) {
        // Update existing aggregated error
        it->second.retry_count++;
        it->second.timestamp_ms = error.timestamp_ms; // Most recent occurrence
    } else {
        // New error type
        error_aggregation_[error.error_code] = error;
    }
}

long long SocketErrorHandler::get_current_time_ms() const {
    auto now = std::chrono::system_clock::now();
    auto duration = now.time_since_epoch();
    return std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
}

// Default recovery handlers

bool SocketErrorHandler::handle_connection_error(SocketError& error) {
    // Implement connection-specific recovery
    if (error.socket_fd >= 0) {
        // Could implement socket reconnection logic here
        // For now, just return false to indicate recovery not implemented
        error.recovery_details = "Connection recovery not yet implemented";
    }
    return false;
}

bool SocketErrorHandler::handle_validation_error(SocketError& error) {
    // Validation errors generally cannot be automatically recovered
    // Log the error and suggest manual intervention
    error.recovery_details = "Validation errors require manual correction";
    return false;
}

bool SocketErrorHandler::handle_authentication_error(SocketError& error) {
    // Authentication errors might be recoverable with token refresh
    if (error.error_code == ERROR_AUTH_TOKEN_EXPIRED) {
        error.recovery_details = "Token refresh not yet implemented";
        // Could implement automatic token refresh here
        return false;
    }
    return false;
}

bool SocketErrorHandler::handle_resource_error(SocketError& error) {
    // Resource errors might be recoverable by waiting or cleanup
    if (error.error_code == ERROR_CONNECTION_LIMIT_EXCEEDED) {
        error.recovery_details = "Waiting for connection slots to become available";
        // Could implement connection pool management here
        return false;
    }
    return false;
}

bool SocketErrorHandler::handle_timeout_error(SocketError& error) {
    // Timeout errors are often recoverable by retry
    error.recovery_details = "Timeout errors handled by retry mechanism";
    return true; // Indicate that retry should be attempted
}

std::string SocketErrorHandler::generate_user_friendly_message(SocketErrorCode error_code) const {
    switch (error_code) {
        case ERROR_INVALID_OPTION:
            return "Invalid socket option specified";
        case ERROR_INVALID_VALUE:
            return "Invalid value for socket option";
        case ERROR_CONNECTION_FAILED:
            return "Failed to establish connection";
        case ERROR_CONNECTION_TIMEOUT:
            return "Connection attempt timed out";
        case ERROR_CONNECTION_REFUSED:
            return "Connection refused by remote host";
        case ERROR_HTTP_NOT_FOUND:
            return "HTTP resource not found (404)";
        case ERROR_HTTP_SERVER_ERROR:
            return "HTTP server error (5xx)";
        case ERROR_AUTH_TOKEN_INVALID:
            return "Authentication token is invalid";
        case ERROR_AUTH_TOKEN_EXPIRED:
            return "Authentication token has expired";
        case ERROR_TLS_HANDSHAKE_FAILED:
            return "TLS handshake failed";
        case ERROR_MEMORY_EXHAUSTED:
            return "System out of memory";
        case ERROR_PERMISSION_DENIED:
            return "Permission denied";
        default:
            return "Unknown error occurred";
    }
}

std::string SocketErrorHandler::generate_technical_details(const SocketError& error) const {
    std::stringstream ss;
    
    ss << "Error Code: " << error.error_code;
    ss << ", Category: " << get_error_category_name(error.category);
    ss << ", Severity: " << get_error_severity_name(error.severity);
    
    if (error.system_errno != 0) {
        ss << ", System Error: " << error.system_errno << " (" << strerror(error.system_errno) << ")";
    }
    
    if (error.socket_fd >= 0) {
        ss << ", Socket FD: " << error.socket_fd;
    }
    
    return ss.str();
}

std::string SocketErrorHandler::generate_suggestion(SocketErrorCode error_code) const {
    switch (error_code) {
        case ERROR_INVALID_OPTION:
            return "Check socket option documentation for valid options";
        case ERROR_INVALID_VALUE:
            return "Verify the option value is within acceptable range and format";
        case ERROR_CONNECTION_FAILED:
            return "Check network connectivity and remote host availability";
        case ERROR_CONNECTION_TIMEOUT:
            return "Increase timeout value or check network conditions";
        case ERROR_CONNECTION_REFUSED:
            return "Verify remote service is running and accessible";
        case ERROR_AUTH_TOKEN_EXPIRED:
            return "Refresh authentication token and retry";
        case ERROR_TLS_HANDSHAKE_FAILED:
            return "Check TLS configuration and certificate validity";
        case ERROR_MEMORY_EXHAUSTED:
            return "Free system memory or increase available memory";
        case ERROR_PERMISSION_DENIED:
            return "Check file permissions and user privileges";
        default:
            return "Review error details and consult documentation";
    }
}

// Convenience functions

SocketError report_socket_error(SocketErrorCode error_code, const std::string& message,
                               const std::string& context) {
    if (g_socket_error_handler) {
        return g_socket_error_handler->report_error(error_code, message, context);
    }
    
    // Fallback if handler not initialized
    return SocketError(error_code, message);
}

SocketError report_socket_validation_error(const ValidationResult& result, int socket_fd) {
    if (g_socket_error_handler) {
        return g_socket_error_handler->report_validation_error(result, socket_fd);
    }
    
    return SocketError(ERROR_INVALID_VALUE, result.error_message);
}

SocketError report_socket_system_error(int errno_value, const std::string& operation, int socket_fd) {
    if (g_socket_error_handler) {
        return g_socket_error_handler->report_system_error(errno_value, operation, socket_fd);
    }
    
    return SocketError(ERROR_SYSTEM_CALL_FAILED, operation + " failed");
}

bool handle_socket_error(SocketError& error) {
    if (g_socket_error_handler) {
        return g_socket_error_handler->handle_error(error);
    }
    
    return false;
}