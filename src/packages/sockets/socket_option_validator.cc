#include "socket_option_validator.h"
#include <regex>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <cmath>

// Global validator instance
SocketOptionValidator* g_socket_option_validator = nullptr;

// Static member definitions
std::map<socket_options, OptionMetadata> SocketOptionValidator::option_metadata_;
bool SocketOptionValidator::metadata_initialized_ = false;

/*
 * SocketOptionValidator Implementation
 */

SocketOptionValidator::SocketOptionValidator() 
    : cache_hits_(0), cache_misses_(0), security_enabled_(true) {
    if (!metadata_initialized_) {
        initialize();
    }
}

SocketOptionValidator::~SocketOptionValidator() {
    clear_validation_cache();
}

void SocketOptionValidator::initialize(bool enable_security) {
    security_enabled_ = enable_security;
    
    if (!metadata_initialized_) {
        // Initialize all option metadata
        initialize_core_metadata();
        initialize_http_metadata(); 
        initialize_rest_metadata();
        initialize_websocket_metadata();
        initialize_mqtt_metadata();
        initialize_external_metadata();
        initialize_cache_metadata();
        initialize_tls_metadata();
        initialize_apache_metadata();
        
        metadata_initialized_ = true;
    }
    
    // Set up default security policies
    if (security_enabled_) {
        category_access_policy_[OPTION_CATEGORY_CORE] = OPTION_ACCESS_PUBLIC;
        category_access_policy_[OPTION_CATEGORY_HTTP] = OPTION_ACCESS_PUBLIC;
        category_access_policy_[OPTION_CATEGORY_REST] = OPTION_ACCESS_OWNER;
        category_access_policy_[OPTION_CATEGORY_WEBSOCKET] = OPTION_ACCESS_PUBLIC;
        category_access_policy_[OPTION_CATEGORY_MQTT] = OPTION_ACCESS_OWNER;
        category_access_policy_[OPTION_CATEGORY_EXTERNAL] = OPTION_ACCESS_PRIVILEGED;
        category_access_policy_[OPTION_CATEGORY_CACHE] = OPTION_ACCESS_OWNER;
        category_access_policy_[OPTION_CATEGORY_TLS] = OPTION_ACCESS_PRIVILEGED;
        category_access_policy_[OPTION_CATEGORY_APACHE] = OPTION_ACCESS_SYSTEM;
        category_access_policy_[OPTION_CATEGORY_INTERNAL] = OPTION_ACCESS_SYSTEM;
    }
}

ValidationResult SocketOptionValidator::validate_option(socket_options option, 
                                                       const svalue_t* value,
                                                       const ValidationContext& context) const {
    // Generate cache key for performance optimization
    std::string cache_key = generate_cache_key(option, value, context);
    
    // Check validation cache
    auto cache_it = validation_cache_.find(cache_key);
    if (cache_it != validation_cache_.end()) {
        cache_hits_++;
        return cache_it->second;
    }
    cache_misses_++;
    
    // Perform actual validation
    ValidationResult result = validate_option_internal(option, value, context);
    
    // Cache result for future use
    validation_cache_[cache_key] = result;
    
    return result;
}

ValidationResult SocketOptionValidator::validate_option_internal(socket_options option,
                                                               const svalue_t* value,
                                                               const ValidationContext& context) const {
    // Check if option exists
    if (!is_valid_option(option)) {
        return ValidationResult(VALIDATION_ERROR_INVALID_OPTION, 
                              "Unknown or invalid socket option: " + std::to_string(option));
    }
    
    const OptionMetadata* metadata = get_option_metadata(option);
    if (!metadata) {
        return ValidationResult(VALIDATION_ERROR_INVALID_OPTION,
                              "No metadata found for option: " + std::to_string(option));
    }
    
    // Validate access permissions
    if (security_enabled_) {
        ValidationResult access_result = validate_access_permissions(
            option, context.access_level, context.caller_id);
        if (!access_result.is_valid) {
            return access_result;
        }
    }
    
    // Validate socket mode compatibility
    if (context.socket_mode >= 0) {
        if (!is_option_valid_for_mode(option, context.socket_mode)) {
            return ValidationResult(VALIDATION_ERROR_PROTOCOL_MISMATCH,
                                  "Option " + std::to_string(option) + 
                                  " is not valid for socket mode " + std::to_string(context.socket_mode));
        }
    }
    
    // Type-specific validation
    ValidationResult type_result;
    switch (metadata->value_type) {
        case OPTION_TYPE_INTEGER:
            if (value->type != T_NUMBER) {
                return ValidationResult(VALIDATION_ERROR_INVALID_TYPE,
                                      "Expected integer value for option " + std::to_string(option));
            }
            type_result = validate_integer_option(option, value->u.number, context);
            break;
            
        case OPTION_TYPE_STRING:
            if (value->type != T_STRING) {
                return ValidationResult(VALIDATION_ERROR_INVALID_TYPE,
                                      "Expected string value for option " + std::to_string(option));
            }
            type_result = validate_string_option(option, value->u.string, context);
            break;
            
        case OPTION_TYPE_BOOLEAN:
            if (value->type != T_NUMBER) { // bool is stored as number in FluffOS
                return ValidationResult(VALIDATION_ERROR_INVALID_TYPE,
                                      "Expected boolean value for option " + std::to_string(option));
            }
            type_result = validate_boolean_option(option, value->u.number != 0, context);
            break;
            
        case OPTION_TYPE_FLOAT:
            if (value->type != T_REAL && value->type != T_NUMBER) {
                return ValidationResult(VALIDATION_ERROR_INVALID_TYPE,
                                      "Expected numeric value for option " + std::to_string(option));
            }
            type_result = validate_float_option(option, 
                value->type == T_REAL ? value->u.real : static_cast<float>(value->u.number), context);
            break;
            
        case OPTION_TYPE_MAPPING:
            if (value->type != T_MAPPING) {
                return ValidationResult(VALIDATION_ERROR_INVALID_TYPE,
                                      "Expected mapping value for option " + std::to_string(option));
            }
            type_result = validate_mapping_option(option, value->u.map, context);
            break;
            
        case OPTION_TYPE_ARRAY:
            if (value->type != T_ARRAY) {
                return ValidationResult(VALIDATION_ERROR_INVALID_TYPE,
                                      "Expected array value for option " + std::to_string(option));
            }
            type_result = validate_array_option(option, value->u.arr, context);
            break;
            
        case OPTION_TYPE_MIXED:
            // Mixed type accepts any value, perform basic validation only
            type_result.is_valid = true;
            break;
            
        default:
            return ValidationResult(VALIDATION_ERROR_INVALID_TYPE,
                                  "Unknown type for option " + std::to_string(option));
    }
    
    if (!type_result.is_valid) {
        return type_result;
    }
    
    // Validate dependencies
    ValidationResult dep_result = validate_dependencies(option, context.current_options);
    if (!dep_result.is_valid) {
        return dep_result;
    }
    
    // Security validation
    if (security_enabled_ && context.security_mode) {
        if (!validate_security_constraints(option, value, context)) {
            return ValidationResult(VALIDATION_ERROR_SECURITY_VIOLATION,
                                  "Security constraint violation for option " + std::to_string(option));
        }
    }
    
    return ValidationResult(); // Success
}

ValidationResult SocketOptionValidator::validate_integer_option(socket_options option, int value,
                                                              const ValidationContext& context) const {
    const OptionMetadata* metadata = get_option_metadata(option);
    if (!metadata) {
        return ValidationResult(VALIDATION_ERROR_INVALID_OPTION, "No metadata for option");
    }
    
    // Range validation
    if (metadata->has_range_limits) {
        if (value < metadata->min_int_value || value > metadata->max_int_value) {
            std::stringstream ss;
            ss << "Value " << value << " is out of range [" 
               << metadata->min_int_value << ", " << metadata->max_int_value 
               << "] for option " << option;
            
            ValidationResult result(VALIDATION_ERROR_OUT_OF_RANGE, ss.str());
            ss.str("");
            ss << "Use a value between " << metadata->min_int_value 
               << " and " << metadata->max_int_value;
            result.suggestion = ss.str();
            return result;
        }
    }
    
    // Option-specific validation
    switch (option) {
        case SOCKET_OPT_TIMEOUT:
        case HTTP_TIMEOUT:
        case HTTP_CONNECT_TIMEOUT:
        case HTTP_READ_TIMEOUT:
            if (!validate_timeout_value(value)) {
                return ValidationResult(VALIDATION_ERROR_OUT_OF_RANGE,
                                      "Invalid timeout value: " + std::to_string(value) + "ms");
            }
            break;
            
        case SOCKET_OPT_RCVBUF:
        case SOCKET_OPT_SNDBUF:
        case SOCKET_OPT_BUFFER_SIZE:
            if (value <= 0 || value > 16*1024*1024) { // 16MB max
                return ValidationResult(VALIDATION_ERROR_OUT_OF_RANGE,
                                      "Buffer size must be between 1 and 16MB");
            }
            break;
            
        case SOCKET_OPT_MAX_CONNECTIONS:
            if (value <= 0 || value > 10000) {
                return ValidationResult(VALIDATION_ERROR_OUT_OF_RANGE,
                                      "Max connections must be between 1 and 10000");
            }
            break;
            
        case WS_MAX_MESSAGE_SIZE:
            if (value < MIN_WS_MESSAGE_SIZE || value > MAX_WS_MESSAGE_SIZE) {
                return ValidationResult(VALIDATION_ERROR_OUT_OF_RANGE,
                                      "WebSocket message size out of range");
            }
            break;
            
        case MQTT_KEEP_ALIVE:
            if (value < MIN_MQTT_KEEP_ALIVE || value > MAX_MQTT_KEEP_ALIVE) {
                return ValidationResult(VALIDATION_ERROR_OUT_OF_RANGE,
                                      "MQTT keep-alive must be between 10 and 3600 seconds");
            }
            break;
            
        case MQTT_QOS:
            if (value < 0 || value > 2) {
                return ValidationResult(VALIDATION_ERROR_OUT_OF_RANGE,
                                      "MQTT QoS must be 0, 1, or 2");
            }
            break;
    }
    
    return ValidationResult(); // Success
}

ValidationResult SocketOptionValidator::validate_string_option(socket_options option, 
                                                             const std::string& value,
                                                             const ValidationContext& context) const {
    const OptionMetadata* metadata = get_option_metadata(option);
    if (!metadata) {
        return ValidationResult(VALIDATION_ERROR_INVALID_OPTION, "No metadata for option");
    }
    
    // Length validation
    if (metadata->has_string_constraints) {
        if (value.length() < metadata->min_string_length || 
            value.length() > metadata->max_string_length) {
            std::stringstream ss;
            ss << "String length " << value.length() << " is out of range ["
               << metadata->min_string_length << ", " << metadata->max_string_length 
               << "] for option " << option;
            return ValidationResult(VALIDATION_ERROR_OUT_OF_RANGE, ss.str());
        }
        
        // Valid values check (for enum-like strings)
        if (!metadata->valid_string_values.empty()) {
            auto it = std::find(metadata->valid_string_values.begin(),
                               metadata->valid_string_values.end(), value);
            if (it == metadata->valid_string_values.end()) {
                std::stringstream ss;
                ss << "Invalid value '" << value << "' for option " << option 
                   << ". Valid values are: ";
                for (size_t i = 0; i < metadata->valid_string_values.size(); ++i) {
                    if (i > 0) ss << ", ";
                    ss << "'" << metadata->valid_string_values[i] << "'";
                }
                return ValidationResult(VALIDATION_ERROR_INVALID_FORMAT, ss.str());
            }
        }
        
        // Regex format validation
        if (!metadata->string_format_regex.empty()) {
            try {
                std::regex pattern(metadata->string_format_regex);
                if (!std::regex_match(value, pattern)) {
                    return ValidationResult(VALIDATION_ERROR_INVALID_FORMAT,
                                          "String format validation failed for option " + std::to_string(option));
                }
            } catch (const std::regex_error& e) {
                return ValidationResult(VALIDATION_ERROR_INVALID_FORMAT,
                                      "Invalid regex pattern in metadata for option " + std::to_string(option));
            }
        }
    }
    
    // Option-specific validation
    switch (option) {
        case HTTP_URL:
            if (!validate_url_format(value)) {
                ValidationResult result(VALIDATION_ERROR_INVALID_FORMAT, 
                                      "Invalid URL format: " + value);
                result.suggestion = "Use format: http://host[:port][/path] or https://host[:port][/path]";
                return result;
            }
            break;
            
        case HTTP_METHOD:
            {
                std::vector<std::string> valid_methods = {
                    "GET", "POST", "PUT", "DELETE", "PATCH", "HEAD", "OPTIONS", "TRACE"
                };
                std::string upper_value = value;
                std::transform(upper_value.begin(), upper_value.end(), upper_value.begin(), ::toupper);
                
                if (std::find(valid_methods.begin(), valid_methods.end(), upper_value) == valid_methods.end()) {
                    return ValidationResult(VALIDATION_ERROR_INVALID_FORMAT,
                                          "Invalid HTTP method: " + value);
                }
            }
            break;
            
        case SOCKET_OPT_TLS_SNI_HOSTNAME:  // Use legacy constant (value 2)
            if (value.empty() || value.length() > 253) {
                return ValidationResult(VALIDATION_ERROR_INVALID_FORMAT,
                                      "Invalid SNI hostname length");
            }
            break;
            
        case MQTT_BROKER:
            if (!validate_url_format(value) && !validate_ip_address(value)) {
                return ValidationResult(VALIDATION_ERROR_INVALID_FORMAT,
                                      "Invalid MQTT broker address: " + value);
            }
            break;
            
        case MQTT_CLIENT_ID:
            if (value.length() > 23) { // MQTT 3.1 limit
                return ValidationResult(VALIDATION_ERROR_OUT_OF_RANGE,
                                      "MQTT client ID too long (max 23 characters)");
            }
            break;
            
        case WS_PROTOCOL:
        case WS_SUBPROTOCOL:
            if (!validate_websocket_protocol(value)) {
                return ValidationResult(VALIDATION_ERROR_INVALID_FORMAT,
                                      "Invalid WebSocket protocol name: " + value);
            }
            break;
            
        case EXTERNAL_COMMAND:
            if (!validate_file_path(value)) {
                return ValidationResult(VALIDATION_ERROR_INVALID_FORMAT,
                                      "Invalid executable path: " + value);
            }
            break;
            
        case REST_JWT_SECRET:
            if (!validate_jwt_secret(value)) {
                return ValidationResult(VALIDATION_ERROR_INVALID_FORMAT,
                                      "Invalid JWT secret format");
            }
            break;
    }
    
    return ValidationResult(); // Success
}

ValidationResult SocketOptionValidator::validate_boolean_option(socket_options option, bool value,
                                                              const ValidationContext& context) const {
    // Boolean options generally don't need special validation beyond type checking
    // But we can add option-specific logic here if needed
    
    switch (option) {
        case SOCKET_OPT_TLS_VERIFY_PEER:  // Use legacy constant (value 1)
            // In strict security mode, always require peer verification
            if (context.security_mode && context.strict_mode && !value) {
                ValidationResult result(VALIDATION_ERROR_SECURITY_VIOLATION,
                                      "TLS peer verification cannot be disabled in strict security mode");
                result.severity = VALIDATION_SEVERITY_FATAL;
                result.suggestion = "Enable TLS peer verification for security";
                return result;
            }
            break;
            
        case EXTERNAL_ASYNC:
            // Warn about potential resource usage
            if (value && context.strict_mode) {
                ValidationResult result;
                result.is_valid = true;
                result.severity = VALIDATION_SEVERITY_WARNING;
                result.error_message = "Async external processes may consume additional resources";
                result.suggestion = "Monitor process resource usage";
                return result;
            }
            break;
    }
    
    return ValidationResult(); // Success
}

ValidationResult SocketOptionValidator::validate_float_option(socket_options option, float value,
                                                            const ValidationContext& context) const {
    const OptionMetadata* metadata = get_option_metadata(option);
    if (!metadata) {
        return ValidationResult(VALIDATION_ERROR_INVALID_OPTION, "No metadata for option");
    }
    
    // NaN and infinity checks
    if (std::isnan(value) || std::isinf(value)) {
        return ValidationResult(VALIDATION_ERROR_OUT_OF_RANGE,
                              "Float value cannot be NaN or infinity");
    }
    
    // Range validation
    if (metadata->has_range_limits) {
        if (value < metadata->min_float_value || value > metadata->max_float_value) {
            std::stringstream ss;
            ss << "Float value " << value << " is out of range ["
               << metadata->min_float_value << ", " << metadata->max_float_value
               << "] for option " << option;
            return ValidationResult(VALIDATION_ERROR_OUT_OF_RANGE, ss.str());
        }
    }
    
    return ValidationResult(); // Success
}

ValidationResult SocketOptionValidator::validate_mapping_option(socket_options option,
                                                              mapping_t* value,
                                                              const ValidationContext& context) const {
    // Basic mapping validation - null check for now
    if (!value) {
        return ValidationResult(VALIDATION_ERROR_INVALID_TYPE,
                               "Mapping value cannot be null");
    }
    
    // TODO: Implement proper FluffOS mapping_t validation
    // This is a stub to allow compilation
    switch (option) {
        case HTTP_HEADERS:
        case REST_OPENAPI_INFO:
        case REST_CORS_CONFIG:
        case EXTERNAL_ENV:
            // Basic validation passed - detailed validation requires FluffOS mapping_t API
            break;
        
        default:
            // Unknown mapping option
            break;
    }
    
    return ValidationResult(); // Success
}

ValidationResult SocketOptionValidator::validate_array_option(socket_options option,
                                                            array_t* value,
                                                            const ValidationContext& context) const {
    // Basic array validation - null check for now
    if (!value) {
        return ValidationResult(VALIDATION_ERROR_INVALID_TYPE,
                               "Array value cannot be null");
    }
    
    // TODO: Implement proper FluffOS array_t validation
    // This is a stub to allow compilation
    switch (option) {
        case EXTERNAL_ARGS:
        case WS_EXTENSIONS:
        case REST_MIDDLEWARE:
            // Basic validation passed - detailed validation requires FluffOS array_t API
            break;
        
        default:
            // Unknown array option
            break;
    }
    
    return ValidationResult(); // Success
}

ValidationResult SocketOptionValidator::validate_access_permissions(socket_options option,
                                                                   socket_option_access caller_access,
                                                                   const std::string& caller_id) const {
    if (!security_enabled_) {
        return ValidationResult(); // Security disabled, allow all
    }
    
    const OptionMetadata* metadata = get_option_metadata(option);
    if (!metadata) {
        return ValidationResult(VALIDATION_ERROR_INVALID_OPTION, "No metadata for option");
    }
    
    // Check if caller is trusted
    if (is_caller_trusted(caller_id)) {
        return ValidationResult(); // Trusted callers bypass access checks
    }
    
    // Check option access level
    if (caller_access < metadata->access_level) {
        std::string access_names[] = {"PUBLIC", "OWNER", "PRIVILEGED", "SYSTEM", "READONLY"};
        std::stringstream ss;
        ss << "Access denied for option " << option 
           << ". Required access level: " << access_names[metadata->access_level]
           << ", caller access level: " << access_names[caller_access];
        
        ValidationResult result(VALIDATION_ERROR_ACCESS_DENIED, ss.str());
        result.severity = VALIDATION_SEVERITY_FATAL;
        return result;
    }
    
    // Check category access policy
    socket_option_category category = metadata->category;
    auto policy_it = category_access_policy_.find(category);
    if (policy_it != category_access_policy_.end() && caller_access < policy_it->second) {
        return ValidationResult(VALIDATION_ERROR_ACCESS_DENIED,
                              "Insufficient access level for option category");
    }
    
    return ValidationResult(); // Success
}

ValidationResult SocketOptionValidator::validate_dependencies(socket_options option,
                                                            const std::map<socket_options, svalue_t>& current_options) const {
    const OptionMetadata* metadata = get_option_metadata(option);
    if (!metadata) {
        return ValidationResult(VALIDATION_ERROR_INVALID_OPTION, "No metadata for option");
    }
    
    // Check required dependencies
    for (socket_options required : metadata->required_options) {
        if (current_options.find(required) == current_options.end()) {
            ValidationResult result(VALIDATION_ERROR_MISSING_DEPENDENCY,
                                  "Option " + std::to_string(option) + 
                                  " requires option " + std::to_string(required) + " to be set");
            result.required_options.push_back(required);
            return result;
        }
    }
    
    // Check conflicting options
    for (socket_options conflicting : metadata->conflicting_options) {
        if (current_options.find(conflicting) != current_options.end()) {
            ValidationResult result(VALIDATION_ERROR_CONFLICTING_OPTION,
                                  "Option " + std::to_string(option) + 
                                  " conflicts with option " + std::to_string(conflicting));
            result.conflicting_options.push_back(conflicting);
            return result;
        }
    }
    
    return ValidationResult(); // Success
}

bool SocketOptionValidator::is_valid_option(socket_options option) const {
    return option_metadata_.find(option) != option_metadata_.end();
}

socket_option_type SocketOptionValidator::get_option_type(socket_options option) const {
    const OptionMetadata* metadata = get_option_metadata(option);
    return metadata ? metadata->value_type : OPTION_TYPE_MIXED;
}

socket_option_category SocketOptionValidator::get_option_category(socket_options option) const {
    const OptionMetadata* metadata = get_option_metadata(option);
    return metadata ? metadata->category : OPTION_CATEGORY_CORE;
}

const OptionMetadata* SocketOptionValidator::get_option_metadata(socket_options option) const {
    auto it = option_metadata_.find(option);
    return (it != option_metadata_.end()) ? &it->second : nullptr;
}

std::string SocketOptionValidator::generate_cache_key(socket_options option,
                                                     const svalue_t* value,
                                                     const ValidationContext& context) const {
    std::stringstream ss;
    ss << option << "|" << "svalue_ptr" << "|" << context.socket_mode 
       << "|" << context.access_level << "|" << context.strict_mode;
    return ss.str();
}

// Validation rule implementations

bool SocketOptionValidator::validate_url_format(const std::string& url) const {
    // Basic URL validation regex
    std::regex url_pattern(R"(^https?:\/\/[a-zA-Z0-9.-]+(?::[0-9]+)?(?:\/[^\s]*)?$)");
    return std::regex_match(url, url_pattern);
}

bool SocketOptionValidator::validate_ip_address(const std::string& ip) const {
    // Simple IPv4 validation
    std::regex ipv4_pattern(R"(^(?:(?:25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.){3}(?:25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)$)");
    return std::regex_match(ip, ipv4_pattern);
}

bool SocketOptionValidator::validate_timeout_value(int timeout_ms) const {
    return timeout_ms >= 0 && timeout_ms <= 300000; // 0-300 seconds
}

bool SocketOptionValidator::validate_jwt_secret(const std::string& secret) const {
    // JWT secret should be at least 32 characters for security
    return secret.length() >= 32;
}

bool SocketOptionValidator::validate_websocket_protocol(const std::string& protocol) const {
    // WebSocket protocol name validation (RFC 6455)
    if (protocol.empty() || protocol.length() > 64) return false;
    
    for (char c : protocol) {
        if (!std::isalnum(c) && c != '-' && c != '.') {
            return false;
        }
    }
    return true;
}

bool SocketOptionValidator::validate_file_path(const std::string& path) const {
    // Basic file path validation - should be absolute and reasonable length
    return !path.empty() && path[0] == '/' && path.length() < 4096;
}

bool SocketOptionValidator::is_caller_trusted(const std::string& caller_id) const {
    return std::find(trusted_callers_.begin(), trusted_callers_.end(), caller_id) != trusted_callers_.end();
}

void SocketOptionValidator::add_trusted_caller(const std::string& caller_id) {
    trusted_callers_.push_back(caller_id);
}

// Initialize metadata for different option categories

void SocketOptionValidator::initialize_core_metadata() {
    // Legacy TLS options (original values for backwards compatibility)
    {
        OptionMetadata metadata;
        metadata.option_id = SOCKET_OPT_TLS_VERIFY_PEER;  // Value 1 (legacy)
        metadata.value_type = OPTION_TYPE_BOOLEAN;
        metadata.category = OPTION_CATEGORY_TLS;
        metadata.access_level = OPTION_ACCESS_OWNER;
        metadata.has_default = true;
        metadata.default_value.type = T_NUMBER;
        metadata.default_value.u.number = 1;
        metadata.valid_socket_modes = {SOCKET_STREAM_TLS, SOCKET_STREAM_TLS_BINARY, HTTPS_SERVER, HTTPS_CLIENT};
        register_option_metadata(metadata);
    }
    
    {
        OptionMetadata metadata;
        metadata.option_id = SOCKET_OPT_TLS_SNI_HOSTNAME;  // Value 2 (legacy)
        metadata.value_type = OPTION_TYPE_STRING;
        metadata.category = OPTION_CATEGORY_TLS;
        metadata.access_level = OPTION_ACCESS_PUBLIC;
        metadata.has_string_constraints = true;
        metadata.min_string_length = 1;
        metadata.max_string_length = 253;
        metadata.valid_socket_modes = {SOCKET_STREAM_TLS, SOCKET_STREAM_TLS_BINARY, HTTPS_CLIENT, WEBSOCKET_TLS_CLIENT};
        register_option_metadata(metadata);
    }
    
    // Core socket options (renumbered to start at 3)
    {
        OptionMetadata metadata;
        metadata.option_id = SOCKET_OPT_KEEPALIVE;  // Value 3 (was 2)
        metadata.value_type = OPTION_TYPE_BOOLEAN;
        metadata.category = OPTION_CATEGORY_CORE;
        metadata.access_level = OPTION_ACCESS_PUBLIC;
        metadata.has_default = true;
        metadata.default_value.type = T_NUMBER;
        metadata.default_value.u.number = 0;
        register_option_metadata(metadata);
    }
    
    {
        OptionMetadata metadata;
        metadata.option_id = SOCKET_OPT_TIMEOUT;  // Value 8 (was 7)
        metadata.value_type = OPTION_TYPE_INTEGER;
        metadata.category = OPTION_CATEGORY_CORE;
        metadata.access_level = OPTION_ACCESS_PUBLIC;
        metadata.has_range_limits = true;
        metadata.min_int_value = 1000;
        metadata.max_int_value = 300000;
        metadata.has_default = true;
        metadata.default_value.type = T_NUMBER;
        metadata.default_value.u.number = 30000;
        register_option_metadata(metadata);
    }
    
    // Add more core options...
}

void SocketOptionValidator::initialize_http_metadata() {
    // HTTP URL
    {
        OptionMetadata metadata;
        metadata.option_id = HTTP_URL;
        metadata.value_type = OPTION_TYPE_STRING;
        metadata.category = OPTION_CATEGORY_HTTP;
        metadata.access_level = OPTION_ACCESS_PUBLIC;
        metadata.has_string_constraints = true;
        metadata.min_string_length = 1;
        metadata.max_string_length = 2048;
        metadata.valid_socket_modes = {HTTP_CLIENT, HTTPS_CLIENT};
        register_option_metadata(metadata);
    }
    
    // HTTP Method
    {
        OptionMetadata metadata;
        metadata.option_id = HTTP_METHOD;
        metadata.value_type = OPTION_TYPE_STRING;
        metadata.category = OPTION_CATEGORY_HTTP;
        metadata.access_level = OPTION_ACCESS_PUBLIC;
        metadata.has_string_constraints = true;
        metadata.valid_string_values = {"GET", "POST", "PUT", "DELETE", "PATCH", "HEAD", "OPTIONS", "TRACE"};
        metadata.has_default = true;
        metadata.default_value.type = T_STRING;
        metadata.default_value.u.string = strdup("GET");
        metadata.valid_socket_modes = {HTTP_CLIENT, HTTPS_CLIENT};
        register_option_metadata(metadata);
    }
    
    // Add more HTTP options...
}

void SocketOptionValidator::initialize_rest_metadata() {
    // REST JWT Secret
    {
        OptionMetadata metadata;
        metadata.option_id = REST_JWT_SECRET;
        metadata.value_type = OPTION_TYPE_STRING;
        metadata.category = OPTION_CATEGORY_REST;
        metadata.access_level = OPTION_ACCESS_PRIVILEGED;
        metadata.has_string_constraints = true;
        metadata.min_string_length = 32;
        metadata.max_string_length = 512;
        metadata.valid_socket_modes = {REST_SERVER};
        register_option_metadata(metadata);
    }
    
    // Add more REST options...
}

void SocketOptionValidator::initialize_websocket_metadata() {
    // WebSocket Protocol
    {
        OptionMetadata metadata;
        metadata.option_id = WS_PROTOCOL;
        metadata.value_type = OPTION_TYPE_STRING;
        metadata.category = OPTION_CATEGORY_WEBSOCKET;
        metadata.access_level = OPTION_ACCESS_PUBLIC;
        metadata.has_string_constraints = true;
        metadata.min_string_length = 1;
        metadata.max_string_length = 64;
        metadata.valid_socket_modes = {WEBSOCKET_SERVER, WEBSOCKET_CLIENT, WEBSOCKET_TLS_SERVER, WEBSOCKET_TLS_CLIENT};
        register_option_metadata(metadata);
    }
    
    // Add more WebSocket options...
}

void SocketOptionValidator::initialize_mqtt_metadata() {
    // MQTT QoS
    {
        OptionMetadata metadata;
        metadata.option_id = MQTT_QOS;
        metadata.value_type = OPTION_TYPE_INTEGER;
        metadata.category = OPTION_CATEGORY_MQTT;
        metadata.access_level = OPTION_ACCESS_PUBLIC;
        metadata.has_range_limits = true;
        metadata.min_int_value = 0;
        metadata.max_int_value = 2;
        metadata.has_default = true;
        metadata.default_value.type = T_NUMBER;
        metadata.default_value.u.number = 0;
        metadata.valid_socket_modes = {MQTT_CLIENT, MQTT_TLS_CLIENT};
        register_option_metadata(metadata);
    }
    
    // Add more MQTT options...
}

void SocketOptionValidator::initialize_external_metadata() {
    // External Command
    {
        OptionMetadata metadata;
        metadata.option_id = EXTERNAL_COMMAND;
        metadata.value_type = OPTION_TYPE_STRING;
        metadata.category = OPTION_CATEGORY_EXTERNAL;
        metadata.access_level = OPTION_ACCESS_PRIVILEGED;
        metadata.has_string_constraints = true;
        metadata.min_string_length = 1;
        metadata.max_string_length = 4096;
        metadata.valid_socket_modes = {EXTERNAL_PROCESS, EXTERNAL_COMMAND_MODE};
        register_option_metadata(metadata);
    }
    
    // Add more external options...
}

void SocketOptionValidator::initialize_cache_metadata() {
    // Cache TTL
    {
        OptionMetadata metadata;
        metadata.option_id = CACHE_TTL;
        metadata.value_type = OPTION_TYPE_INTEGER;
        metadata.category = OPTION_CATEGORY_CACHE;
        metadata.access_level = OPTION_ACCESS_OWNER;
        metadata.has_range_limits = true;
        metadata.min_int_value = MIN_CACHE_TTL;
        metadata.max_int_value = MAX_CACHE_TTL;
        metadata.has_default = true;
        metadata.default_value.type = T_NUMBER;
        metadata.default_value.u.number = DEFAULT_CACHE_TTL;
        register_option_metadata(metadata);
    }
    
    // Add more cache options...
}

void SocketOptionValidator::initialize_tls_metadata() {
    // Legacy TLS options are handled in initialize_core_metadata()
    // This function is for advanced TLS options (320-339 range)
    
    // Add advanced TLS options when implemented...
}

void SocketOptionValidator::initialize_apache_metadata() {
    // Apache options are future scope - placeholder implementation
    // Add Apache-specific metadata when implementing Apache integration
}

void SocketOptionValidator::register_option_metadata(const OptionMetadata& metadata) {
    option_metadata_[metadata.option_id] = metadata;
}

// Convenience functions

ValidationResult validate_socket_option(socket_options option, const svalue_t* value,
                                       int socket_mode, socket_option_access access) {
    if (!g_socket_option_validator) {
        return ValidationResult(VALIDATION_ERROR_INVALID_OPTION, "Validator not initialized");
    }
    
    ValidationContext context;
    context.socket_mode = socket_mode;
    context.access_level = access;
    
    return g_socket_option_validator->validate_option(option, value, context);
}

ValidationResult validate_socket_options(const std::map<socket_options, svalue_t>& options,
                                        int socket_mode, socket_option_access access) {
    if (!g_socket_option_validator) {
        return ValidationResult(VALIDATION_ERROR_INVALID_OPTION, "Validator not initialized");
    }
    
    ValidationContext context;
    context.socket_mode = socket_mode;
    context.access_level = access;
    context.current_options = options;
    
    return g_socket_option_validator->validate_option_set(options, context);
}

bool is_socket_option_valid(socket_options option, const svalue_t& value) {
    ValidationResult result = validate_socket_option(option, &value);
    return result.is_valid;
}

std::string get_socket_option_error(socket_options option, const svalue_t& value) {
    ValidationResult result = validate_socket_option(option, &value);
    return result.is_valid ? "" : result.error_message;
}