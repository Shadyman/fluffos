#ifndef SOCKET_ERROR_HANDLER_H_
#define SOCKET_ERROR_HANDLER_H_

#include "socket_options.h"
#include "socket_option_validator.h"
#include "vm/internal/base/svalue.h"
#include <string>
#include <map>
#include <vector>
#include <functional>

/*
 * Comprehensive Socket Error Handling System
 * 
 * Provides centralized error handling, logging, recovery strategies,
 * and user-friendly error reporting for the unified socket architecture.
 * 
 * Features:
 * - Structured error classification and reporting
 * - Automatic error recovery strategies
 * - Context-aware error logging
 * - User-friendly error messages with suggestions
 * - Error aggregation and analysis
 * - Performance impact tracking
 */

/*
 * Socket Error Categories
 */
enum SocketErrorCategory {
    ERROR_CATEGORY_VALIDATION = 0,     // Option validation errors
    ERROR_CATEGORY_CONNECTION = 1,     // Network connection errors  
    ERROR_CATEGORY_PROTOCOL = 2,       // Protocol-specific errors
    ERROR_CATEGORY_AUTHENTICATION = 3, // Authentication/security errors
    ERROR_CATEGORY_RESOURCE = 4,       // Resource exhaustion errors
    ERROR_CATEGORY_CONFIGURATION = 5,  // Configuration errors
    ERROR_CATEGORY_SYSTEM = 6,         // System-level errors
    ERROR_CATEGORY_TIMEOUT = 7,        // Timeout-related errors
    ERROR_CATEGORY_DATA = 8,           // Data processing errors
    ERROR_CATEGORY_INTERNAL = 9        // Internal/unexpected errors
};

/*
 * Socket Error Codes
 */
enum SocketErrorCode {
    // Validation errors (1000-1099)
    ERROR_INVALID_OPTION = 1000,
    ERROR_INVALID_VALUE = 1001,
    ERROR_MISSING_REQUIRED_OPTION = 1002,
    ERROR_CONFLICTING_OPTIONS = 1003,
    ERROR_ACCESS_DENIED = 1004,
    ERROR_PROTOCOL_MISMATCH = 1005,
    
    // Connection errors (1100-1199)
    ERROR_CONNECTION_FAILED = 1100,
    ERROR_CONNECTION_TIMEOUT = 1101,
    ERROR_CONNECTION_REFUSED = 1102,
    ERROR_CONNECTION_RESET = 1103,
    ERROR_HOST_UNREACHABLE = 1104,
    ERROR_DNS_RESOLUTION_FAILED = 1105,
    ERROR_NETWORK_UNREACHABLE = 1106,
    ERROR_CONNECTION_ABORTED = 1107,
    
    // Protocol errors (1200-1299)
    ERROR_HTTP_INVALID_RESPONSE = 1200,
    ERROR_HTTP_BAD_REQUEST = 1201,
    ERROR_HTTP_UNAUTHORIZED = 1202,
    ERROR_HTTP_FORBIDDEN = 1203,
    ERROR_HTTP_NOT_FOUND = 1204,
    ERROR_HTTP_SERVER_ERROR = 1205,
    ERROR_WEBSOCKET_HANDSHAKE_FAILED = 1210,
    ERROR_WEBSOCKET_PROTOCOL_ERROR = 1211,
    ERROR_MQTT_CONNECTION_REFUSED = 1220,
    ERROR_MQTT_PROTOCOL_VIOLATION = 1221,
    ERROR_REST_INVALID_JSON = 1230,
    ERROR_REST_SCHEMA_VALIDATION = 1231,
    
    // Authentication errors (1300-1399)
    ERROR_TLS_HANDSHAKE_FAILED = 1300,
    ERROR_TLS_CERTIFICATE_INVALID = 1301,
    ERROR_TLS_CERTIFICATE_EXPIRED = 1302,
    ERROR_AUTH_TOKEN_INVALID = 1310,
    ERROR_AUTH_TOKEN_EXPIRED = 1311,
    ERROR_AUTH_CREDENTIALS_INVALID = 1312,
    ERROR_AUTH_INSUFFICIENT_PRIVILEGES = 1313,
    
    // Resource errors (1400-1499)
    ERROR_MEMORY_EXHAUSTED = 1400,
    ERROR_FILE_DESCRIPTOR_LIMIT = 1401,
    ERROR_CONNECTION_LIMIT_EXCEEDED = 1402,
    ERROR_BUFFER_OVERFLOW = 1403,
    ERROR_RATE_LIMIT_EXCEEDED = 1404,
    ERROR_DISK_SPACE_EXHAUSTED = 1405,
    
    // Configuration errors (1500-1599)
    ERROR_CONFIG_FILE_NOT_FOUND = 1500,
    ERROR_CONFIG_PARSE_ERROR = 1501,
    ERROR_CONFIG_INVALID_VALUE = 1502,
    ERROR_CONFIG_MISSING_REQUIRED = 1503,
    
    // System errors (1600-1699)
    ERROR_SYSTEM_CALL_FAILED = 1600,
    ERROR_PERMISSION_DENIED = 1601,
    ERROR_FILE_NOT_FOUND = 1602,
    ERROR_DEVICE_BUSY = 1603,
    ERROR_SIGNAL_RECEIVED = 1604,
    
    // Timeout errors (1700-1799)
    ERROR_OPERATION_TIMEOUT = 1700,
    ERROR_READ_TIMEOUT = 1701,
    ERROR_WRITE_TIMEOUT = 1702,
    ERROR_CONNECT_TIMEOUT = 1703,
    ERROR_HANDSHAKE_TIMEOUT = 1704,
    
    // Data errors (1800-1899)
    ERROR_INVALID_DATA_FORMAT = 1800,
    ERROR_DATA_CORRUPTION = 1801,
    ERROR_ENCODING_ERROR = 1802,
    ERROR_PARSING_ERROR = 1803,
    ERROR_SERIALIZATION_ERROR = 1804,
    
    // Internal errors (1900-1999)
    ERROR_INTERNAL_ERROR = 1900,
    ERROR_ASSERTION_FAILED = 1901,
    ERROR_UNIMPLEMENTED_FEATURE = 1902,
    ERROR_INVALID_STATE = 1903
};

/*
 * Error Severity Levels
 */
enum SocketErrorSeverity {
    ERROR_SEVERITY_DEBUG = 0,      // Debug information
    ERROR_SEVERITY_INFO = 1,       // Informational
    ERROR_SEVERITY_WARNING = 2,    // Warning condition
    ERROR_SEVERITY_ERROR = 3,      // Error condition
    ERROR_SEVERITY_CRITICAL = 4,   // Critical error
    ERROR_SEVERITY_FATAL = 5       // Fatal error (system unstable)
};

/*
 * Error Recovery Strategy
 */
enum SocketErrorRecovery {
    RECOVERY_NONE = 0,             // No automatic recovery
    RECOVERY_RETRY = 1,            // Retry the operation
    RECOVERY_FALLBACK = 2,         // Use fallback configuration
    RECOVERY_RECONNECT = 3,        // Reconnect and retry
    RECOVERY_RESTART = 4,          // Restart the socket/service
    RECOVERY_ESCALATE = 5,         // Escalate to admin/system
    RECOVERY_GRACEFUL_SHUTDOWN = 6 // Graceful shutdown
};

/*
 * Socket Error Information
 */
struct SocketError {
    SocketErrorCode error_code;
    SocketErrorCategory category;
    SocketErrorSeverity severity;
    SocketErrorRecovery recovery_strategy;
    
    std::string message;           // Human-readable error message
    std::string technical_details; // Technical details for debugging
    std::string suggestion;        // Fix suggestion for user
    std::string context;          // Context where error occurred
    
    // Associated data
    int socket_fd;                // Socket file descriptor (-1 if N/A)
    socket_options related_option; // Related option (if applicable)
    svalue_t option_value;        // Option value that caused error
    int system_errno;             // System errno (if applicable)
    
    // Timing information
    long long timestamp_ms;       // Error timestamp (milliseconds)
    int retry_count;              // Number of retries attempted
    long long first_occurrence_ms; // First occurrence timestamp
    
    // Recovery information
    bool recovery_attempted;      // Whether recovery was attempted
    bool recovery_successful;     // Whether recovery succeeded
    std::string recovery_details; // Details about recovery attempt
    
    SocketError() :
        error_code(ERROR_INTERNAL_ERROR),
        category(ERROR_CATEGORY_INTERNAL),
        severity(ERROR_SEVERITY_ERROR),
        recovery_strategy(RECOVERY_NONE),
        socket_fd(-1),
        related_option(static_cast<socket_options>(-1)),
        system_errno(0),
        timestamp_ms(0),
        retry_count(0),
        first_occurrence_ms(0),
        recovery_attempted(false),
        recovery_successful(false) {}
        
    SocketError(SocketErrorCode code, const std::string& msg) :
        error_code(code),
        category(ERROR_CATEGORY_INTERNAL),
        severity(ERROR_SEVERITY_ERROR),
        recovery_strategy(RECOVERY_NONE),
        message(msg),
        socket_fd(-1),
        related_option(static_cast<socket_options>(-1)),
        system_errno(0),
        timestamp_ms(0),
        retry_count(0),
        first_occurrence_ms(0),
        recovery_attempted(false),
        recovery_successful(false) {}
};

/*
 * Error Handler Configuration
 */
struct ErrorHandlerConfig {
    bool enable_logging;           // Enable error logging
    bool enable_recovery;          // Enable automatic recovery
    bool enable_aggregation;       // Enable error aggregation
    bool enable_metrics;           // Enable error metrics
    
    int max_retry_attempts;        // Maximum retry attempts
    int retry_delay_ms;            // Delay between retries
    int error_cache_size;          // Maximum cached errors
    int recovery_timeout_ms;       // Recovery operation timeout
    
    std::string log_file_path;     // Error log file path
    std::string metrics_endpoint;  // Metrics reporting endpoint
    
    ErrorHandlerConfig() :
        enable_logging(true),
        enable_recovery(true),
        enable_aggregation(true),
        enable_metrics(false),
        max_retry_attempts(3),
        retry_delay_ms(1000),
        error_cache_size(1000),
        recovery_timeout_ms(30000) {}
};

/*
 * Error Statistics
 */
struct ErrorStatistics {
    std::map<SocketErrorCode, int> error_counts;
    std::map<SocketErrorCategory, int> category_counts;
    std::map<SocketErrorSeverity, int> severity_counts;
    
    int total_errors;
    int recovery_attempts;
    int successful_recoveries;
    long long last_error_time;
    
    ErrorStatistics() :
        total_errors(0),
        recovery_attempts(0),
        successful_recoveries(0),
        last_error_time(0) {}
};

/*
 * Main Socket Error Handler Class
 */
class SocketErrorHandler {
private:
    ErrorHandlerConfig config_;
    ErrorStatistics stats_;
    
    // Error cache and aggregation
    std::vector<SocketError> recent_errors_;
    std::map<SocketErrorCode, SocketError> error_aggregation_;
    
    // Recovery strategies
    std::map<SocketErrorCode, std::function<bool(SocketError&)>> recovery_handlers_;
    
    // Logging and metrics
    std::string log_file_path_;
    bool metrics_enabled_;
    
public:
    SocketErrorHandler();
    ~SocketErrorHandler();
    
    // Initialize error handler
    void initialize(const ErrorHandlerConfig& config);
    
    // Error reporting and handling
    SocketError report_error(SocketErrorCode error_code, 
                            const std::string& message,
                            const std::string& context = "");
    
    SocketError report_validation_error(const ValidationResult& validation_result,
                                       int socket_fd = -1,
                                       const std::string& context = "");
    
    SocketError report_system_error(int system_errno,
                                   const std::string& operation,
                                   int socket_fd = -1);
    
    SocketError report_protocol_error(SocketErrorCode error_code,
                                     const std::string& protocol_details,
                                     int socket_fd = -1);
    
    // Error handling and recovery
    bool handle_error(SocketError& error);
    bool attempt_recovery(SocketError& error);
    bool retry_operation(SocketError& error, std::function<bool()> operation);
    
    // Error analysis and reporting
    std::vector<SocketError> get_recent_errors(int limit = 100) const;
    ErrorStatistics get_statistics() const { return stats_; }
    std::map<SocketErrorCode, int> get_error_summary() const;
    
    // Configuration and management
    void set_config(const ErrorHandlerConfig& config) { config_ = config; }
    ErrorHandlerConfig get_config() const { return config_; }
    
    void clear_error_cache();
    void reset_statistics();
    
    // Recovery strategy registration
    void register_recovery_handler(SocketErrorCode error_code,
                                  std::function<bool(SocketError&)> handler);
    
    // Utility methods
    std::string format_error_message(const SocketError& error, bool include_technical = false) const;
    std::string get_error_category_name(SocketErrorCategory category) const;
    std::string get_error_severity_name(SocketErrorSeverity severity) const;
    std::string get_recovery_strategy_name(SocketErrorRecovery strategy) const;
    
    // Error classification
    SocketErrorCategory classify_error(SocketErrorCode error_code) const;
    SocketErrorSeverity determine_severity(SocketErrorCode error_code) const;
    SocketErrorRecovery suggest_recovery(SocketErrorCode error_code) const;
    
private:
    // Internal methods
    void log_error(const SocketError& error);
    void update_statistics(const SocketError& error);
    void aggregate_error(const SocketError& error);
    
    long long get_current_time_ms() const;
    
    // Default recovery handlers
    bool handle_connection_error(SocketError& error);
    bool handle_validation_error(SocketError& error);
    bool handle_authentication_error(SocketError& error);
    bool handle_resource_error(SocketError& error);
    bool handle_timeout_error(SocketError& error);
    
    // Error message generation
    std::string generate_user_friendly_message(SocketErrorCode error_code) const;
    std::string generate_technical_details(const SocketError& error) const;
    std::string generate_suggestion(SocketErrorCode error_code) const;
};

/*
 * Global error handler instance
 */
extern SocketErrorHandler* g_socket_error_handler;

/*
 * Convenience functions for error reporting
 */
SocketError report_socket_error(SocketErrorCode error_code, const std::string& message,
                               const std::string& context = "");

SocketError report_socket_validation_error(const ValidationResult& result, int socket_fd = -1);

SocketError report_socket_system_error(int errno_value, const std::string& operation, int socket_fd = -1);

bool handle_socket_error(SocketError& error);

/*
 * Error handling macros for common patterns
 */
#define REPORT_AND_RETURN_ERROR(code, msg, context) \
    do { \
        SocketError error = report_socket_error(code, msg, context); \
        return error; \
    } while(0)

#define HANDLE_SYSTEM_ERROR_OR_RETURN(errno_val, op, fd, result_var) \
    do { \
        if (errno_val != 0) { \
            result_var = report_socket_system_error(errno_val, op, fd); \
            return result_var; \
        } \
    } while(0)

#define VALIDATE_OR_REPORT_ERROR(validation_result, fd, error_var) \
    do { \
        if (!validation_result.is_valid) { \
            error_var = report_socket_validation_error(validation_result, fd); \
            return error_var; \
        } \
    } while(0)

#define TRY_RECOVERY_ON_ERROR(error, operation) \
    do { \
        if (!error.recovery_attempted && g_socket_error_handler) { \
            g_socket_error_handler->attempt_recovery(error); \
            if (error.recovery_successful) { \
                operation; \
            } \
        } \
    } while(0)

#endif  // SOCKET_ERROR_HANDLER_H_