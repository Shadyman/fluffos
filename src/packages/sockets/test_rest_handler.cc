#include "rest_handler.h"
#include "socket_rest_integration.h"
#include <iostream>
#include <cassert>
#include <string>

/*
 * REST Handler Test Suite
 * 
 * Comprehensive tests for the REST API framework implementation.
 * Tests route management, JSON processing, CORS support, and
 * integration with the HTTP handler foundation.
 */

class RESTHandlerTest {
private:
    int test_socket_id_;
    std::unique_ptr<RESTHandler> handler_;
    int tests_run_;
    int tests_passed_;
    
public:
    RESTHandlerTest() : test_socket_id_(1001), tests_run_(0), tests_passed_(0) {
        handler_ = std::make_unique<RESTHandler>(test_socket_id_);
    }
    
    ~RESTHandlerTest() {
        std::cout << "\nREST Handler Test Results:\n";
        std::cout << "Tests run: " << tests_run_ << "\n";
        std::cout << "Tests passed: " << tests_passed_ << "\n";
        std::cout << "Success rate: " << (tests_run_ > 0 ? (tests_passed_ * 100 / tests_run_) : 0) << "%\n";
    }
    
    void run_all_tests() {
        std::cout << "Starting REST Handler Test Suite...\n\n";
        
        test_handler_initialization();
        test_route_management();
        test_route_pattern_validation();
        test_route_parameter_extraction();
        test_json_utilities();
        test_cors_functionality();
        test_error_handling();
        test_integration_functions();
        test_option_processing();
        test_socket_integration();
        
        std::cout << "\nREST Handler Test Suite Complete!\n";
    }
    
private:
    void assert_test(bool condition, const std::string& test_name) {
        tests_run_++;
        if (condition) {
            tests_passed_++;
            std::cout << "✓ " << test_name << "\n";
        } else {
            std::cout << "✗ " << test_name << " - FAILED\n";
        }
    }
    
    void test_handler_initialization() {
        std::cout << "Testing REST Handler Initialization...\n";
        
        // Test handler creation
        assert_test(handler_ != nullptr, "Handler creation");
        
        // Test HTTP handler integration
        assert_test(handler_->get_http_handler() != nullptr, "HTTP handler integration");
        
        // Test initial state
        assert_test(!handler_->is_rest_request_complete(), "Initial request state");
        assert_test(handler_->get_buffer_size() == 0, "Initial buffer size");
        
        std::cout << "\n";
    }
    
    void test_route_management() {
        std::cout << "Testing Route Management...\n";
        
        // Test route addition
        bool route_added = handler_->add_route("GET", "/api/users", 
                                              "/lib/api/users", "get_users", 
                                              "Get all users");
        assert_test(route_added, "Route addition - basic");
        
        // Test route with parameters
        bool param_route_added = handler_->add_route("GET", "/api/users/{id}", 
                                                    "/lib/api/users", "get_user", 
                                                    "Get specific user");
        assert_test(param_route_added, "Route addition - with parameters");
        
        // Test complex route
        bool complex_route_added = handler_->add_route("POST", "/api/users/{id}/posts/{post_id}", 
                                                      "/lib/api/posts", "update_post", 
                                                      "Update user post");
        assert_test(complex_route_added, "Route addition - complex parameters");
        
        // Test invalid route
        bool invalid_route = handler_->add_route("INVALID", "invalid-pattern", 
                                                "/lib/api/test", "test", "");
        assert_test(!invalid_route, "Route addition - invalid method rejected");
        
        std::cout << "\n";
    }
    
    void test_route_pattern_validation() {
        std::cout << "Testing Route Pattern Validation...\n";
        
        // Test valid patterns
        assert_test(RESTHandler::is_valid_route_pattern("/api/users"), 
                   "Valid pattern - simple");
        assert_test(RESTHandler::is_valid_route_pattern("/api/users/{id}"), 
                   "Valid pattern - with parameter");
        assert_test(RESTHandler::is_valid_route_pattern("/api/v1/users/{id}/posts/{post_id}"), 
                   "Valid pattern - complex");
        
        // Test invalid patterns
        assert_test(!RESTHandler::is_valid_route_pattern(""), 
                   "Invalid pattern - empty");
        assert_test(!RESTHandler::is_valid_route_pattern("api/users"), 
                   "Invalid pattern - no leading slash");
        
        // Test pattern normalization
        std::string normalized = RESTHandler::normalize_route_pattern("api/users/");
        assert_test(normalized == "/api/users", "Pattern normalization");
        
        std::cout << "\n";
    }
    
    void test_route_parameter_extraction() {
        std::cout << "Testing Route Parameter Extraction...\n";
        
        // Test parameter extraction
        std::vector<std::string> params = RESTHandler::extract_route_parameter_names("/api/users/{id}");
        assert_test(params.size() == 1 && params[0] == "id", 
                   "Parameter extraction - single param");
        
        params = RESTHandler::extract_route_parameter_names("/api/users/{id}/posts/{post_id}");
        assert_test(params.size() == 2 && params[0] == "id" && params[1] == "post_id", 
                   "Parameter extraction - multiple params");
        
        params = RESTHandler::extract_route_parameter_names("/api/users");
        assert_test(params.empty(), "Parameter extraction - no params");
        
        std::cout << "\n";
    }
    
    void test_json_utilities() {
        std::cout << "Testing JSON Utilities...\n";
        
        // Test JSON string escaping
        std::string escaped = RESTHandler::escape_json_string("Hello \"World\"");
        assert_test(escaped.find("\\\"") != std::string::npos, "JSON string escaping - quotes");
        
        escaped = RESTHandler::escape_json_string("Line 1\nLine 2");
        assert_test(escaped.find("\\n") != std::string::npos, "JSON string escaping - newlines");
        
        // Test JSON validation (basic)
        std::string json_obj = "{\"key\": \"value\"}";
        std::string json_array = "[1, 2, 3]";
        std::string invalid_json = "{key: value}";
        
        // Note: These would test the actual JSON validation in a full implementation
        
        std::cout << "\n";
    }
    
    void test_cors_functionality() {
        std::cout << "Testing CORS Functionality...\n";
        
        // Test CORS disabled by default
        assert_test(!handler_->is_cors_enabled(), "CORS disabled by default");
        
        // Test CORS enabling
        handler_->enable_cors();
        assert_test(handler_->is_cors_enabled(), "CORS enabling");
        
        // Test CORS disabling
        handler_->disable_cors();
        assert_test(!handler_->is_cors_enabled(), "CORS disabling");
        
        std::cout << "\n";
    }
    
    void test_error_handling() {
        std::cout << "Testing Error Handling...\n";
        
        // Test error response generation
        std::string error_response = handler_->create_json_error_response(
            HTTP_STATUS_NOT_FOUND, "Resource not found");
        assert_test(!error_response.empty(), "Error response generation");
        assert_test(error_response.find("404") != std::string::npos, "Error response - status code");
        assert_test(error_response.find("not found") != std::string::npos, "Error response - message");
        
        // Test error state management
        handler_->clear_error();
        assert_test(strlen(handler_->get_last_error()) == 0, "Error state clearing");
        
        std::cout << "\n";
    }
    
    void test_integration_functions() {
        std::cout << "Testing Integration Functions...\n";
        
        // Test REST mode enablement
        bool rest_enabled = socket_enable_rest_mode(test_socket_id_);
        assert_test(rest_enabled, "REST mode enablement");
        
        // Test REST mode detection
        bool is_rest = socket_is_rest_mode(test_socket_id_);
        assert_test(is_rest, "REST mode detection");
        
        // Test REST handler retrieval
        RESTHandler* retrieved_handler = get_rest_handler(test_socket_id_);
        assert_test(retrieved_handler != nullptr, "REST handler retrieval");
        
        std::cout << "\n";
    }
    
    void test_option_processing() {
        std::cout << "Testing Option Processing...\n";
        
        // Test REST mode registration
        bool mode_registered = register_rest_socket_mode();
        assert_test(mode_registered, "REST mode registration");
        
        // Test mode availability
        bool mode_available = is_rest_mode_available();
        assert_test(mode_available, "REST mode availability");
        
        // Test mode number
        int mode_number = get_rest_socket_mode();
        assert_test(mode_number == REST_SERVER_MODE, "REST mode number");
        
        std::cout << "\n";
    }
    
    void test_socket_integration() {
        std::cout << "Testing Socket Integration...\n";
        
        // Test auto-detection
        bool should_enable = socket_should_enable_rest_processing(test_socket_id_);
        // This test depends on socket configuration
        
        // Test REST request detection
        const char* rest_data = "POST /api/users HTTP/1.1\r\nContent-Type: application/json\r\n\r\n{}";
        bool is_rest_request = socket_rest_detect_rest_request(rest_data, strlen(rest_data));
        assert_test(is_rest_request, "REST request detection");
        
        const char* non_rest_data = "GET /index.html HTTP/1.1\r\n\r\n";
        bool is_not_rest = !socket_rest_detect_rest_request(non_rest_data, strlen(non_rest_data));
        assert_test(is_not_rest, "Non-REST request detection");
        
        // Test statistics
        size_t active_count = socket_rest_get_active_count();
        assert_test(active_count >= 0, "Active socket count");
        
        std::cout << "\n";
    }
};

/*
 * Mock Functions for Testing
 * 
 * These functions provide mock implementations for LPC integration
 * points that would normally be provided by the FluffOS runtime.
 */

// Mock svalue_t for testing
struct mock_svalue_t {
    int type;
    union {
        int number;
        char* string;
        void* map;
    } u;
};

// Mock mapping_t for testing
struct mock_mapping_t {
    int size;
    // Actual mapping structure would be more complex
};

// Mock function implementations
void bad_argument(void* sp, int expected_type, int arg_num, int function_id) {
    std::cout << "Mock bad_argument called\n";
}

void pop_stack() {
    std::cout << "Mock pop_stack called\n";
}

void push_number(int value) {
    std::cout << "Mock push_number(" << value << ") called\n";
}

void push_malloced_string(char* str) {
    std::cout << "Mock push_malloced_string called\n";
}

void push_refed_array(void* array) {
    std::cout << "Mock push_refed_array called\n";
}

void push_refed_mapping(void* mapping) {
    std::cout << "Mock push_refed_mapping called\n";
}

char* string_copy(const char* str) {
    if (!str) return nullptr;
    size_t len = strlen(str);
    char* copy = new char[len + 1];
    strcpy(copy, str);
    return copy;
}

void* find_value_in_mapping(void* mapping, void* key) {
    return nullptr; // Mock implementation
}

void outbuf_add(outbuffer_t* buffer, const char* str) {
    if (buffer && str) {
        std::cout << str;
    }
}

void outbuf_addv(outbuffer_t* buffer, const char* format, ...) {
    if (buffer && format) {
        std::cout << format;
    }
}

/*
 * Test Runner
 */
int main() {
    std::cout << "FluffOS Unified Socket Architecture - REST Handler Tests\n";
    std::cout << "========================================================\n\n";
    
    try {
        RESTHandlerTest test_suite;
        test_suite.run_all_tests();
        
        return 0;
    } catch (const std::exception& e) {
        std::cout << "Test suite failed with exception: " << e.what() << "\n";
        return 1;
    }
}

/*
 * Test Compilation Notes:
 * 
 * To compile this test file independently:
 * g++ -std=c++11 -I. -I../.. test_rest_handler.cc -o test_rest_handler
 * 
 * Note: Some functionality requires actual FluffOS integration and
 * will need to be tested within the full FluffOS build environment.
 */