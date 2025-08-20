#ifndef SOCKET_OPTION_VALIDATOR_H_
#define SOCKET_OPTION_VALIDATOR_H_

#include "socket_options.h"
#include "base/package_api.h"
#include "vm/internal/base/svalue.h"
#include <string>
#include <map>
#include <vector>
#include <functional>

// Forward declarations for FluffOS types
struct mapping_t;
struct array_t;

/*
 * Socket Option Validation System
 *
 * Provides comprehensive validation for all socket options defined in
 * socket_options.h. Handles type validation, range checking, dependency
 * validation, and security constraints.
 *
 * Features:
 * - Type-safe validation for all option types
 * - Range and constraint validation
 * - Cross-option dependency checking
 * - Security access control validation
 * - Protocol-specific validation rules
 * - Detailed error reporting and suggestions
 * - Performance-optimized validation caching
 */

// Forward declarations
class SocketOptionManager;
struct ValidationContext;
struct ValidationResult;

/*
 * Validation Error Types
 */
enum ValidationErrorType {
    VALIDATION_ERROR_NONE = 0,
    VALIDATION_ERROR_INVALID_OPTION = 1,      // Unknown/invalid option ID
    VALIDATION_ERROR_INVALID_TYPE = 2,        // Type mismatch
    VALIDATION_ERROR_OUT_OF_RANGE = 3,        // Value outside allowed range
    VALIDATION_ERROR_INVALID_FORMAT = 4,      // String format validation failed
    VALIDATION_ERROR_MISSING_DEPENDENCY = 5,  // Required dependency not set
    VALIDATION_ERROR_CONFLICTING_OPTION = 6,  // Conflicting option values
    VALIDATION_ERROR_ACCESS_DENIED = 7,       // Insufficient access privileges
    VALIDATION_ERROR_PROTOCOL_MISMATCH = 8,   // Option not valid for socket mode
    VALIDATION_ERROR_RESOURCE_LIMIT = 9,      // Resource constraint exceeded
    VALIDATION_ERROR_SECURITY_VIOLATION = 10  // Security policy violation
};

/*
 * Validation Severity Levels
 */
enum ValidationSeverity {
    VALIDATION_SEVERITY_INFO = 0,     // Informational (suggestion)
    VALIDATION_SEVERITY_WARNING = 1,  // Warning (may cause issues)
    VALIDATION_SEVERITY_ERROR = 2,    // Error (will cause failure)
    VALIDATION_SEVERITY_FATAL = 3     // Fatal (security/stability risk)
};

/*
 * Validation Context - provides context for validation
 */
struct ValidationContext {
    int socket_mode;                              // Current socket mode
    socket_option_access access_level;            // Caller's access level
    std::map<socket_options, svalue_t> current_options;  // Currently set options
    std::string caller_id;                        // ID of calling object
    bool strict_mode;                             // Enable strict validation
    bool security_mode;                           // Enable security validation

    ValidationContext() :
        socket_mode(-1),
        access_level(OPTION_ACCESS_PUBLIC),
        caller_id(""),
        strict_mode(false),
        security_mode(true) {}
};

/*
 * Validation Result - detailed validation outcome
 */
struct ValidationResult {
    bool is_valid;                                // Overall validation result
    ValidationErrorType error_type;               // Primary error type
    ValidationSeverity severity;                  // Error severity
    std::string error_message;                    // Human-readable error
    std::string suggestion;                       // Fix suggestion
    std::vector<socket_options> conflicting_options;  // Conflicting options
    std::vector<socket_options> required_options;     // Missing dependencies

    ValidationResult() :
        is_valid(true),
        error_type(VALIDATION_ERROR_NONE),
        severity(VALIDATION_SEVERITY_INFO) {}

    ValidationResult(ValidationErrorType err_type, const std::string& message) :
        is_valid(false),
        error_type(err_type),
        severity(VALIDATION_SEVERITY_ERROR),
        error_message(message) {}
};

/*
 * Option Metadata - describes option properties
 */
struct OptionMetadata {
    socket_options option_id;
    socket_option_type value_type;
    socket_option_category category;
    socket_option_access access_level;
    std::vector<int> valid_socket_modes;  // Valid socket modes for this option
    bool has_default;
    svalue_t default_value;

    // Range validation (for numeric types)
    bool has_range_limits;
    int min_int_value;
    int max_int_value;
    float min_float_value;
    float max_float_value;

    // String validation
    bool has_string_constraints;
    int min_string_length;
    int max_string_length;
    std::vector<std::string> valid_string_values;  // For enum-like strings
    std::string string_format_regex;               // Format validation

    // Dependencies
    std::vector<socket_options> required_options;   // Must be set
    std::vector<socket_options> conflicting_options; // Cannot be set together
    std::vector<socket_options> recommended_options; // Should be set

    OptionMetadata() :
        option_id(static_cast<socket_options>(-1)),
        value_type(OPTION_TYPE_MIXED),
        category(OPTION_CATEGORY_CORE),
        access_level(OPTION_ACCESS_PUBLIC),
        has_default(false),
        has_range_limits(false),
        min_int_value(0), max_int_value(0),
        min_float_value(0.0f), max_float_value(0.0f),
        has_string_constraints(false),
        min_string_length(0), max_string_length(0) {}
};

/*
 * Main Socket Option Validator Class
 */
class SocketOptionValidator {
private:
    // Option metadata registry
    static std::map<socket_options, OptionMetadata> option_metadata_;
    static bool metadata_initialized_;

    // Validation cache for performance
    mutable std::map<std::string, ValidationResult> validation_cache_;
    mutable int cache_hits_;
    mutable int cache_misses_;

    // Security policy configuration
    bool security_enabled_;
    std::vector<std::string> trusted_callers_;
    std::map<socket_option_category, socket_option_access> category_access_policy_;

public:
    SocketOptionValidator();
    ~SocketOptionValidator();

    // Initialize validator with security configuration
    void initialize(bool enable_security = true);

    // Main validation methods
    ValidationResult validate_option(socket_options option,
                                   const svalue_t* value,
                                   const ValidationContext& context) const;

    ValidationResult validate_option_set(const std::map<socket_options, svalue_t>& options,
                                       const ValidationContext& context) const;

    ValidationResult validate_socket_mode_compatibility(int socket_mode,
                                                       const std::map<socket_options, svalue_t>& options) const;

    // Type-specific validation
    ValidationResult validate_integer_option(socket_options option, int value,
                                           const ValidationContext& context) const;
    ValidationResult validate_string_option(socket_options option, const std::string& value,
                                          const ValidationContext& context) const;
    ValidationResult validate_boolean_option(socket_options option, bool value,
                                           const ValidationContext& context) const;
    ValidationResult validate_float_option(socket_options option, float value,
                                         const ValidationContext& context) const;
    ValidationResult validate_mapping_option(socket_options option, mapping_t* value,
                                           const ValidationContext& context) const;
    ValidationResult validate_array_option(socket_options option, array_t* value,
                                         const ValidationContext& context) const;

    // Protocol-specific validation
    ValidationResult validate_http_options(const std::map<socket_options, svalue_t>& options,
                                         const ValidationContext& context) const;
    ValidationResult validate_rest_options(const std::map<socket_options, svalue_t>& options,
                                         const ValidationContext& context) const;
    ValidationResult validate_websocket_options(const std::map<socket_options, svalue_t>& options,
                                              const ValidationContext& context) const;
    ValidationResult validate_mqtt_options(const std::map<socket_options, svalue_t>& options,
                                         const ValidationContext& context) const;
    ValidationResult validate_external_options(const std::map<socket_options, svalue_t>& options,
                                              const ValidationContext& context) const;
    ValidationResult validate_tls_options(const std::map<socket_options, svalue_t>& options,
                                        const ValidationContext& context) const;
    ValidationResult validate_cache_options(const std::map<socket_options, svalue_t>& options,
                                          const ValidationContext& context) const;

    // Access control validation
    ValidationResult validate_access_permissions(socket_options option,
                                               socket_option_access caller_access,
                                               const std::string& caller_id) const;

    // Dependency validation
    ValidationResult validate_dependencies(socket_options option,
                                         const std::map<socket_options, svalue_t>& current_options) const;

    ValidationResult check_conflicting_options(const std::map<socket_options, svalue_t>& options) const;

    // Utility methods
    bool is_valid_option(socket_options option) const;
    socket_option_type get_option_type(socket_options option) const;
    socket_option_category get_option_category(socket_options option) const;
    socket_option_access get_option_access_level(socket_options option) const;

    std::vector<int> get_valid_socket_modes(socket_options option) const;
    bool is_option_valid_for_mode(socket_options option, int socket_mode) const;

    svalue_t get_default_value(socket_options option) const;
    bool has_default_value(socket_options option) const;

    // Metadata management
    void register_option_metadata(const OptionMetadata& metadata);
    const OptionMetadata* get_option_metadata(socket_options option) const;

    // Security configuration
    void set_security_enabled(bool enabled) { security_enabled_ = enabled; }
    void add_trusted_caller(const std::string& caller_id);
    void set_category_access_policy(socket_option_category category, socket_option_access min_access);

    // Cache management
    void clear_validation_cache() { validation_cache_.clear(); }
    std::pair<int, int> get_cache_stats() const { return {cache_hits_, cache_misses_}; }

    // Error formatting
    std::string format_validation_error(const ValidationResult& result) const;
    std::string format_option_help(socket_options option) const;

private:
    // Internal validation helpers
    ValidationResult validate_option_internal(socket_options option,
                                             const svalue_t* value,
                                             const ValidationContext& context) const;

    std::string generate_cache_key(socket_options option,
                                 const svalue_t* value,
                                 const ValidationContext& context) const;

    void initialize_core_metadata();
    void initialize_http_metadata();
    void initialize_rest_metadata();
    void initialize_websocket_metadata();
    void initialize_mqtt_metadata();
    void initialize_external_metadata();
    void initialize_cache_metadata();
    void initialize_tls_metadata();
    void initialize_apache_metadata();

    // Validation rule implementations
    bool validate_url_format(const std::string& url) const;
    bool validate_ip_address(const std::string& ip) const;
    bool validate_port_number(int port) const;
    bool validate_timeout_value(int timeout_ms) const;
    bool validate_file_path(const std::string& path) const;
    bool validate_regex_pattern(const std::string& pattern) const;
    bool validate_json_schema(const std::string& schema) const;
    bool validate_jwt_secret(const std::string& secret) const;
    bool validate_mqtt_topic(const std::string& topic) const;
    bool validate_websocket_protocol(const std::string& protocol) const;

    // Security validation
    bool is_caller_trusted(const std::string& caller_id) const;
    bool check_resource_limits(socket_options option, const svalue_t& value) const;
    bool validate_security_constraints(socket_options option, const svalue_t* value,
                                     const ValidationContext& context) const;
};

/*
 * Global validator instance - managed by SocketOptionManager
 */
extern SocketOptionValidator* g_socket_option_validator;

/*
 * Convenience functions for common validation tasks
 */
ValidationResult validate_socket_option(socket_options option, const svalue_t* value,
                                       int socket_mode = -1,
                                       socket_option_access access = OPTION_ACCESS_PUBLIC);

ValidationResult validate_socket_options(const std::map<socket_options, svalue_t>& options,
                                        int socket_mode = -1,
                                        socket_option_access access = OPTION_ACCESS_PUBLIC);

bool is_socket_option_valid(socket_options option, const svalue_t& value);

std::string get_socket_option_error(socket_options option, const svalue_t& value);

/*
 * Validation macros for common checks
 */
#define VALIDATE_OPTION_OR_RETURN(option, value, context, result) \
    do { \
        result = g_socket_option_validator->validate_option(option, value, context); \
        if (!result.is_valid) return result; \
    } while(0)

#define VALIDATE_ACCESS_OR_RETURN(option, access, caller, result) \
    do { \
        result = g_socket_option_validator->validate_access_permissions(option, access, caller); \
        if (!result.is_valid) return result; \
    } while(0)

#define VALIDATE_SOCKET_MODE_OR_RETURN(mode, options, result) \
    do { \
        result = g_socket_option_validator->validate_socket_mode_compatibility(mode, options); \
        if (!result.is_valid) return result; \
    } while(0)

#endif  // SOCKET_OPTION_VALIDATOR_H_