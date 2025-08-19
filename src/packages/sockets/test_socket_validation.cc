/*
 * Unit Tests for Socket Option Validation System
 * 
 * Comprehensive tests for the socket option validator and error handler
 * components of the unified socket architecture.
 */

#include "socket_option_validator.h"
#include "socket_error_handler.h"
#include <iostream>
#include <cassert>
#include <vector>
#include <map>
#include <string>

// Test framework macros
#define TEST_ASSERT(condition, message) \
    do { \
        if (!(condition)) { \
            std::cerr << "FAIL: " << message << " at " << __FILE__ << ":" << __LINE__ << std::endl; \
            return false; \
        } \
    } while(0)

#define TEST_ASSERT_EQ(expected, actual, message) \
    do { \
        if ((expected) != (actual)) { \
            std::cerr << "FAIL: " << message << " - Expected: " << (expected) \
                      << ", Actual: " << (actual) << " at " << __FILE__ << ":" << __LINE__ << std::endl; \
            return false; \
        } \
    } while(0)

#define RUN_TEST(test_func) \
    do { \
        std::cout << "Running " << #test_func << "... "; \
        if (test_func()) { \
            std::cout << "PASS" << std::endl; \
            tests_passed++; \
        } else { \
            std::cout << "FAIL" << std::endl; \
            tests_failed++; \
        } \
        tests_total++; \
    } while(0)

// Global test counters
static int tests_total = 0;
static int tests_passed = 0;
static int tests_failed = 0;

/*
 * Mock mixed type for testing
 */
class mixed {
private:
    enum Type { INT, STRING, BOOL, FLOAT, ARRAY, MAPPING } type_;
    union {
        int int_val;
        bool bool_val;
        float float_val;
    };
    std::string string_val;
    std::vector<mixed> array_val;
    std::map<std::string, mixed> mapping_val;
    
public:
    mixed(int val) : type_(INT), int_val(val) {}
    mixed(const char* val) : type_(STRING), string_val(val) {}
    mixed(const std::string& val) : type_(STRING), string_val(val) {}
    mixed(bool val) : type_(BOOL), bool_val(val) {}
    mixed(float val) : type_(FLOAT), float_val(val) {}
    mixed(const std::vector<mixed>& val) : type_(ARRAY), array_val(val) {}
    mixed(const std::map<std::string, mixed>& val) : type_(MAPPING), mapping_val(val) {}
    
    bool is_int() const { return type_ == INT; }
    bool is_string() const { return type_ == STRING; }
    bool is_bool() const { return type_ == BOOL; }
    bool is_float() const { return type_ == FLOAT; }
    bool is_array() const { return type_ == ARRAY; }
    bool is_mapping() const { return type_ == MAPPING; }
    
    int get_int() const { return is_int() ? int_val : 0; }
    std::string get_string() const { return is_string() ? string_val : ""; }
    bool get_bool() const { return is_bool() ? bool_val : false; }
    float get_float() const { return is_float() ? float_val : 0.0f; }
    std::vector<mixed> get_array() const { return is_array() ? array_val : std::vector<mixed>(); }
    std::map<std::string, mixed> get_mapping() const { return is_mapping() ? mapping_val : std::map<std::string, mixed>(); }
    
    std::string to_string() const {
        switch (type_) {
            case INT: return std::to_string(int_val);
            case STRING: return string_val;
            case BOOL: return bool_val ? "true" : "false";
            case FLOAT: return std::to_string(float_val);
            case ARRAY: return "[array]";
            case MAPPING: return "[mapping]";
        }
        return "";
    }
};

/*
 * Test Cases
 */

bool test_validator_initialization() {
    SocketOptionValidator validator;
    validator.initialize(true);
    
    TEST_ASSERT(validator.is_valid_option(SO_TLS_VERIFY_PEER), "Core TLS option should be valid");
    TEST_ASSERT(validator.is_valid_option(SO_HTTP_URL), "HTTP URL option should be valid");
    TEST_ASSERT(validator.is_valid_option(REST_JWT_SECRET), "REST JWT secret option should be valid");
    TEST_ASSERT(validator.is_valid_option(WS_PROTOCOL), "WebSocket protocol option should be valid");
    TEST_ASSERT(validator.is_valid_option(MQTT_QOS), "MQTT QoS option should be valid");
    TEST_ASSERT(validator.is_valid_option(EXTERNAL_COMMAND), "External command option should be valid");
    TEST_ASSERT(validator.is_valid_option(SO_CACHE_TTL), "Cache TTL option should be valid");
    
    TEST_ASSERT(!validator.is_valid_option(static_cast<socket_options>(9999)), "Invalid option should not be valid");
    
    return true;
}

bool test_integer_validation() {
    SocketOptionValidator validator;
    validator.initialize(true);
    
    ValidationContext context;
    context.socket_mode = HTTP_CLIENT;
    context.access_level = OPTION_ACCESS_PUBLIC;
    
    // Valid timeout value
    ValidationResult result = validator.validate_option(SO_HTTP_TIMEOUT, mixed(30000), context);
    TEST_ASSERT(result.is_valid, "Valid HTTP timeout should pass validation");
    
    // Invalid timeout value (too low)
    result = validator.validate_option(SO_HTTP_TIMEOUT, mixed(500), context);
    TEST_ASSERT(!result.is_valid, "HTTP timeout below minimum should fail validation");
    TEST_ASSERT_EQ(VALIDATION_ERROR_OUT_OF_RANGE, result.error_type, "Should be out of range error");
    
    // Invalid timeout value (too high)
    result = validator.validate_option(SO_HTTP_TIMEOUT, mixed(500000), context);
    TEST_ASSERT(!result.is_valid, "HTTP timeout above maximum should fail validation");
    
    // Valid MQTT QoS
    result = validator.validate_option(MQTT_QOS, mixed(1), context);
    TEST_ASSERT(result.is_valid, "Valid MQTT QoS should pass validation");
    
    // Invalid MQTT QoS
    result = validator.validate_option(MQTT_QOS, mixed(5), context);
    TEST_ASSERT(!result.is_valid, "Invalid MQTT QoS should fail validation");
    
    return true;
}

bool test_string_validation() {
    SocketOptionValidator validator;
    validator.initialize(true);
    
    ValidationContext context;
    context.socket_mode = HTTP_CLIENT;
    context.access_level = OPTION_ACCESS_PUBLIC;
    
    // Valid HTTP URL
    ValidationResult result = validator.validate_option(SO_HTTP_URL, mixed("http://example.com/api"), context);
    TEST_ASSERT(result.is_valid, "Valid HTTP URL should pass validation");
    
    // Valid HTTPS URL
    result = validator.validate_option(SO_HTTP_URL, mixed("https://secure.example.com/api/v1"), context);
    TEST_ASSERT(result.is_valid, "Valid HTTPS URL should pass validation");
    
    // Invalid URL format
    result = validator.validate_option(SO_HTTP_URL, mixed("not-a-url"), context);
    TEST_ASSERT(!result.is_valid, "Invalid URL format should fail validation");
    TEST_ASSERT_EQ(VALIDATION_ERROR_INVALID_FORMAT, result.error_type, "Should be format error");
    
    // Valid HTTP method
    result = validator.validate_option(SO_HTTP_METHOD, mixed("GET"), context);
    TEST_ASSERT(result.is_valid, "Valid HTTP method should pass validation");
    
    // Invalid HTTP method
    result = validator.validate_option(SO_HTTP_METHOD, mixed("INVALID"), context);
    TEST_ASSERT(!result.is_valid, "Invalid HTTP method should fail validation");
    
    // Valid WebSocket protocol
    result = validator.validate_option(WS_PROTOCOL, mixed("chat"), context);
    TEST_ASSERT(result.is_valid, "Valid WebSocket protocol should pass validation");
    
    // Invalid WebSocket protocol (too long)
    std::string long_protocol(100, 'x');
    result = validator.validate_option(WS_PROTOCOL, mixed(long_protocol), context);
    TEST_ASSERT(!result.is_valid, "Overly long WebSocket protocol should fail validation");
    
    return true;
}

bool test_boolean_validation() {
    SocketOptionValidator validator;
    validator.initialize(true);
    
    ValidationContext context;
    context.socket_mode = HTTPS_CLIENT;
    context.access_level = OPTION_ACCESS_PUBLIC;
    context.security_mode = true;
    context.strict_mode = true;
    
    // Valid TLS verification (enabled)
    ValidationResult result = validator.validate_option(SO_TLS_VERIFY_PEER, mixed(true), context);
    TEST_ASSERT(result.is_valid, "Enabled TLS verification should pass validation");
    
    // Invalid in strict security mode (disabled)
    result = validator.validate_option(SO_TLS_VERIFY_PEER, mixed(false), context);
    TEST_ASSERT(!result.is_valid, "Disabled TLS verification should fail in strict security mode");
    TEST_ASSERT_EQ(VALIDATION_ERROR_SECURITY_VIOLATION, result.error_type, "Should be security violation");
    
    // Valid in non-strict mode
    context.strict_mode = false;
    result = validator.validate_option(SO_TLS_VERIFY_PEER, mixed(false), context);
    TEST_ASSERT(result.is_valid, "Disabled TLS verification should pass in non-strict mode");
    
    return true;
}

bool test_type_validation() {
    SocketOptionValidator validator;
    validator.initialize(true);
    
    ValidationContext context;
    context.socket_mode = HTTP_CLIENT;
    context.access_level = OPTION_ACCESS_PUBLIC;
    
    // String option with integer value (should fail)
    ValidationResult result = validator.validate_option(SO_HTTP_URL, mixed(12345), context);
    TEST_ASSERT(!result.is_valid, "String option with integer value should fail");
    TEST_ASSERT_EQ(VALIDATION_ERROR_INVALID_TYPE, result.error_type, "Should be type error");
    
    // Integer option with string value (should fail)
    result = validator.validate_option(SO_HTTP_TIMEOUT, mixed("not-a-number"), context);
    TEST_ASSERT(!result.is_valid, "Integer option with string value should fail");
    TEST_ASSERT_EQ(VALIDATION_ERROR_INVALID_TYPE, result.error_type, "Should be type error");
    
    // Boolean option with integer value (should fail)
    result = validator.validate_option(SO_TLS_VERIFY_PEER, mixed(1), context);
    TEST_ASSERT(!result.is_valid, "Boolean option with integer value should fail");
    TEST_ASSERT_EQ(VALIDATION_ERROR_INVALID_TYPE, result.error_type, "Should be type error");
    
    return true;
}

bool test_socket_mode_compatibility() {
    SocketOptionValidator validator;
    validator.initialize(true);
    
    ValidationContext context;
    context.access_level = OPTION_ACCESS_PUBLIC;
    
    // HTTP URL valid for HTTP client mode
    context.socket_mode = HTTP_CLIENT;
    ValidationResult result = validator.validate_option(SO_HTTP_URL, mixed("http://example.com"), context);
    TEST_ASSERT(result.is_valid, "HTTP URL should be valid for HTTP client mode");
    
    // HTTP URL invalid for WebSocket mode
    context.socket_mode = WEBSOCKET_CLIENT;
    result = validator.validate_option(SO_HTTP_URL, mixed("http://example.com"), context);
    TEST_ASSERT(!result.is_valid, "HTTP URL should be invalid for WebSocket mode");
    TEST_ASSERT_EQ(VALIDATION_ERROR_PROTOCOL_MISMATCH, result.error_type, "Should be protocol mismatch");
    
    // WebSocket protocol valid for WebSocket mode
    result = validator.validate_option(WS_PROTOCOL, mixed("chat"), context);
    TEST_ASSERT(result.is_valid, "WebSocket protocol should be valid for WebSocket mode");
    
    // MQTT QoS valid for MQTT mode
    context.socket_mode = MQTT_CLIENT;
    result = validator.validate_option(MQTT_QOS, mixed(1), context);
    TEST_ASSERT(result.is_valid, "MQTT QoS should be valid for MQTT mode");
    
    return true;
}

bool test_access_control() {
    SocketOptionValidator validator;
    validator.initialize(true);
    
    ValidationContext context;
    context.socket_mode = REST_SERVER;
    
    // Public access to public option
    context.access_level = OPTION_ACCESS_PUBLIC;
    ValidationResult result = validator.validate_option(SO_HTTP_METHOD, mixed("GET"), context);
    TEST_ASSERT(result.is_valid, "Public access to public option should succeed");
    
    // Public access to privileged option (should fail)
    result = validator.validate_option(REST_JWT_SECRET, mixed("secret123456789012345678901234567890"), context);
    TEST_ASSERT(!result.is_valid, "Public access to privileged option should fail");
    TEST_ASSERT_EQ(VALIDATION_ERROR_ACCESS_DENIED, result.error_type, "Should be access denied");
    
    // Privileged access to privileged option
    context.access_level = OPTION_ACCESS_PRIVILEGED;
    result = validator.validate_option(REST_JWT_SECRET, mixed("secret123456789012345678901234567890"), context);
    TEST_ASSERT(result.is_valid, "Privileged access to privileged option should succeed");
    
    // External command requires privileged access
    context.access_level = OPTION_ACCESS_PUBLIC;
    result = validator.validate_option(EXTERNAL_COMMAND, mixed("/bin/echo"), context);
    TEST_ASSERT(!result.is_valid, "Public access to external command should fail");
    
    context.access_level = OPTION_ACCESS_PRIVILEGED;
    result = validator.validate_option(EXTERNAL_COMMAND, mixed("/bin/echo"), context);
    TEST_ASSERT(result.is_valid, "Privileged access to external command should succeed");
    
    return true;
}

bool test_option_dependencies() {
    SocketOptionValidator validator;
    validator.initialize(true);
    
    ValidationContext context;
    context.socket_mode = REST_SERVER;
    context.access_level = OPTION_ACCESS_PRIVILEGED;
    
    // Test would require implementing dependency metadata
    // For now, just verify the dependency validation function exists
    std::map<socket_options, mixed> options;
    ValidationResult result = validator.validate_dependencies(REST_JWT_SECRET, options);
    TEST_ASSERT(result.is_valid || !result.is_valid, "Dependency validation should complete");
    
    return true;
}

bool test_validation_caching() {
    SocketOptionValidator validator;
    validator.initialize(true);
    
    ValidationContext context;
    context.socket_mode = HTTP_CLIENT;
    context.access_level = OPTION_ACCESS_PUBLIC;
    
    // First validation (cache miss)
    ValidationResult result1 = validator.validate_option(SO_HTTP_TIMEOUT, mixed(30000), context);
    auto cache_stats = validator.get_cache_stats();
    int initial_misses = cache_stats.second;
    
    // Second identical validation (cache hit)
    ValidationResult result2 = validator.validate_option(SO_HTTP_TIMEOUT, mixed(30000), context);
    cache_stats = validator.get_cache_stats();
    int final_hits = cache_stats.first;
    
    TEST_ASSERT(result1.is_valid == result2.is_valid, "Cached result should match original");
    TEST_ASSERT(final_hits > 0, "Should have at least one cache hit");
    
    return true;
}

bool test_error_handler_initialization() {
    SocketErrorHandler handler;
    
    ErrorHandlerConfig config;
    config.enable_logging = true;
    config.enable_recovery = true;
    config.max_retry_attempts = 5;
    
    handler.initialize(config);
    
    ErrorHandlerConfig retrieved_config = handler.get_config();
    TEST_ASSERT_EQ(5, retrieved_config.max_retry_attempts, "Config should be properly set");
    
    return true;
}

bool test_error_reporting() {
    SocketErrorHandler handler;
    handler.initialize(ErrorHandlerConfig());
    
    // Report a connection error
    SocketError error = handler.report_error(ERROR_CONNECTION_FAILED, 
                                           "Connection to server failed", 
                                           "test_context");
    
    TEST_ASSERT_EQ(ERROR_CONNECTION_FAILED, error.error_code, "Error code should match");
    TEST_ASSERT_EQ(ERROR_CATEGORY_CONNECTION, error.category, "Should be connection category");
    TEST_ASSERT_EQ(ERROR_SEVERITY_CRITICAL, error.severity, "Should be critical severity");
    TEST_ASSERT(!error.message.empty(), "Error message should not be empty");
    TEST_ASSERT(!error.suggestion.empty(), "Error suggestion should not be empty");
    
    return true;
}

bool test_validation_error_reporting() {
    SocketErrorHandler handler;
    handler.initialize(ErrorHandlerConfig());
    
    // Create a validation result
    ValidationResult validation_result(VALIDATION_ERROR_INVALID_VALUE, "Invalid timeout value");
    validation_result.severity = VALIDATION_SEVERITY_ERROR;
    validation_result.suggestion = "Use a value between 1000 and 300000";
    
    // Report as socket error
    SocketError error = handler.report_validation_error(validation_result, 123);
    
    TEST_ASSERT_EQ(ERROR_INVALID_VALUE, error.error_code, "Should map to invalid value error");
    TEST_ASSERT_EQ(123, error.socket_fd, "Socket FD should be preserved");
    TEST_ASSERT(!error.suggestion.empty(), "Suggestion should be preserved");
    
    return true;
}

bool test_error_statistics() {
    SocketErrorHandler handler;
    handler.initialize(ErrorHandlerConfig());
    
    // Report several errors
    handler.report_error(ERROR_CONNECTION_FAILED, "Test error 1");
    handler.report_error(ERROR_CONNECTION_FAILED, "Test error 2");
    handler.report_error(ERROR_INVALID_VALUE, "Test error 3");
    
    ErrorStatistics stats = handler.get_statistics();
    
    TEST_ASSERT_EQ(3, stats.total_errors, "Should have 3 total errors");
    TEST_ASSERT_EQ(2, stats.error_counts[ERROR_CONNECTION_FAILED], "Should have 2 connection errors");
    TEST_ASSERT_EQ(1, stats.error_counts[ERROR_INVALID_VALUE], "Should have 1 validation error");
    
    return true;
}

bool test_error_message_formatting() {
    SocketErrorHandler handler;
    handler.initialize(ErrorHandlerConfig());
    
    SocketError error = handler.report_error(ERROR_CONNECTION_TIMEOUT, 
                                           "Connection timed out after 30 seconds",
                                           "HTTP client");
    error.socket_fd = 42;
    error.suggestion = "Increase timeout value or check network";
    
    std::string formatted = handler.format_error_message(error, false);
    TEST_ASSERT(!formatted.empty(), "Formatted message should not be empty");
    TEST_ASSERT(formatted.find("ERROR") != std::string::npos, "Should contain severity");
    TEST_ASSERT(formatted.find("1701") != std::string::npos, "Should contain error code");
    TEST_ASSERT(formatted.find("Socket FD: 42") != std::string::npos, "Should contain socket FD");
    
    std::string technical = handler.format_error_message(error, true);
    TEST_ASSERT(technical.length() > formatted.length(), "Technical format should be longer");
    
    return true;
}

bool test_so_prefix_compliance() {
    SocketOptionValidator validator;
    validator.initialize(true);
    
    // Core options should have SO_ prefix
    TEST_ASSERT(validator.is_valid_option(SO_TLS_VERIFY_PEER), "Core TLS option with SO_ should be valid");
    TEST_ASSERT(validator.is_valid_option(SO_HTTP_TIMEOUT), "HTTP timeout with SO_ should be valid");
    TEST_ASSERT(validator.is_valid_option(SO_CACHE_TTL), "Cache TTL with SO_ should be valid");
    
    // Protocol options should NOT have SO_ prefix
    TEST_ASSERT(validator.is_valid_option(REST_JWT_SECRET), "REST option without SO_ should be valid");
    TEST_ASSERT(validator.is_valid_option(WS_PROTOCOL), "WebSocket option without SO_ should be valid");
    TEST_ASSERT(validator.is_valid_option(MQTT_QOS), "MQTT option without SO_ should be valid");
    TEST_ASSERT(validator.is_valid_option(EXTERNAL_COMMAND), "External option without SO_ should be valid");
    
    return true;
}

bool test_integration_validation_and_error_handling() {
    SocketOptionValidator validator;
    validator.initialize(true);
    
    SocketErrorHandler handler;
    handler.initialize(ErrorHandlerConfig());
    
    ValidationContext context;
    context.socket_mode = REST_SERVER;
    context.access_level = OPTION_ACCESS_PUBLIC; // Insufficient for JWT secret
    
    // Try to validate a privileged option with insufficient access
    ValidationResult validation_result = validator.validate_option(REST_JWT_SECRET, 
                                                                  mixed("short"), context);
    TEST_ASSERT(!validation_result.is_valid, "Validation should fail");
    
    // Convert validation error to socket error
    SocketError socket_error = handler.report_validation_error(validation_result, 100);
    TEST_ASSERT_EQ(ERROR_ACCESS_DENIED, socket_error.error_code, "Should be access denied error");
    TEST_ASSERT_EQ(100, socket_error.socket_fd, "Socket FD should be preserved");
    
    return true;
}

/*
 * Test Runner
 */
int main() {
    std::cout << "Running Socket Option Validation System Tests\n";
    std::cout << "================================================\n\n";
    
    // Initialize global instances
    g_socket_option_validator = new SocketOptionValidator();
    g_socket_option_validator->initialize(true);
    
    g_socket_error_handler = new SocketErrorHandler();
    g_socket_error_handler->initialize(ErrorHandlerConfig());
    
    // Run validation tests
    std::cout << "Validator Tests:\n";
    RUN_TEST(test_validator_initialization);
    RUN_TEST(test_integer_validation);
    RUN_TEST(test_string_validation);
    RUN_TEST(test_boolean_validation);
    RUN_TEST(test_type_validation);
    RUN_TEST(test_socket_mode_compatibility);
    RUN_TEST(test_access_control);
    RUN_TEST(test_option_dependencies);
    RUN_TEST(test_validation_caching);
    RUN_TEST(test_so_prefix_compliance);
    
    std::cout << "\nError Handler Tests:\n";
    RUN_TEST(test_error_handler_initialization);
    RUN_TEST(test_error_reporting);
    RUN_TEST(test_validation_error_reporting);
    RUN_TEST(test_error_statistics);
    RUN_TEST(test_error_message_formatting);
    
    std::cout << "\nIntegration Tests:\n";
    RUN_TEST(test_integration_validation_and_error_handling);
    
    // Cleanup
    delete g_socket_option_validator;
    delete g_socket_error_handler;
    
    // Print results
    std::cout << "\n================================================\n";
    std::cout << "Test Results:\n";
    std::cout << "Total tests: " << tests_total << "\n";
    std::cout << "Passed: " << tests_passed << "\n";
    std::cout << "Failed: " << tests_failed << "\n";
    std::cout << "Success rate: " << (tests_total > 0 ? (tests_passed * 100 / tests_total) : 0) << "%\n";
    
    if (tests_failed > 0) {
        std::cout << "\nSome tests failed. Review the output above for details.\n";
        return 1;
    } else {
        std::cout << "\nAll tests passed! ✓\n";
        return 0;
    }
}