#include "http_handler.h"
#include "socket_option_manager.h"
#include <cassert>
#include <iostream>

/*
 * HTTP Handler Test Suite
 * 
 * This file contains comprehensive tests for the HTTP handler implementation
 * to ensure proper HTTP/1.1 protocol support and integration with the
 * unified socket architecture.
 */

void test_http_request_parsing() {
    std::cout << "Testing HTTP request parsing..." << std::endl;
    
    HTTPHandler handler(1);
    
    // Test basic GET request
    const char* get_request = 
        "GET /test?param=value HTTP/1.1\r\n"
        "Host: localhost:8080\r\n"
        "User-Agent: TestClient/1.0\r\n"
        "Connection: keep-alive\r\n"
        "\r\n";
    
    assert(handler.process_incoming_data(get_request, strlen(get_request)));
    assert(handler.is_request_complete());
    
    const HTTPRequest& request = handler.get_current_request();
    assert(request.method == HTTP_GET);
    assert(request.path == "/test");
    assert(request.query_string == "param=value");
    assert(request.version == HTTP_VERSION_1_1);
    assert(request.keep_alive == true);
    assert(request.headers.at("host") == "localhost:8080");
    assert(request.headers.at("user-agent") == "TestClient/1.0");
    
    std::cout << "✓ Basic GET request parsing passed" << std::endl;
}

void test_http_post_request() {
    std::cout << "Testing HTTP POST request..." << std::endl;
    
    HTTPHandler handler(2);
    
    const char* post_request = 
        "POST /api/data HTTP/1.1\r\n"
        "Host: api.example.com\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: 25\r\n"
        "Connection: close\r\n"
        "\r\n"
        "{\"name\":\"test\",\"id\":123}";
    
    assert(handler.process_incoming_data(post_request, strlen(post_request)));
    assert(handler.is_request_complete());
    
    const HTTPRequest& request = handler.get_current_request();
    assert(request.method == HTTP_POST);
    assert(request.path == "/api/data");
    assert(request.version == HTTP_VERSION_1_1);
    assert(request.keep_alive == false);  // Connection: close
    assert(request.content_length == 25);
    assert(request.body == "{\"name\":\"test\",\"id\":123}");
    assert(request.headers.at("content-type") == "application/json");
    
    std::cout << "✓ POST request parsing passed" << std::endl;
}

void test_http_response_generation() {
    std::cout << "Testing HTTP response generation..." << std::endl;
    
    HTTPHandler handler(3);
    
    // Test basic success response
    std::string response = handler.create_success_response("<h1>Hello World</h1>", "text/html");
    
    assert(response.find("HTTP/1.1 200 OK") == 0);
    assert(response.find("Content-Type: text/html; charset=utf-8") != std::string::npos);
    assert(response.find("Content-Length: 20") != std::string::npos);
    assert(response.find("<h1>Hello World</h1>") != std::string::npos);
    assert(response.find("Server: FluffOS") != std::string::npos);
    
    std::cout << "✓ Success response generation passed" << std::endl;
    
    // Test JSON response
    std::string json_response = handler.create_json_response("{\"status\":\"ok\"}", HTTP_STATUS_CREATED);
    
    assert(json_response.find("HTTP/1.1 201 Created") == 0);
    assert(json_response.find("Content-Type: application/json; charset=utf-8") != std::string::npos);
    assert(json_response.find("{\"status\":\"ok\"}") != std::string::npos);
    
    std::cout << "✓ JSON response generation passed" << std::endl;
    
    // Test error response
    std::string error_response = handler.create_error_response(HTTP_STATUS_NOT_FOUND, "Page not found");
    
    assert(error_response.find("HTTP/1.1 404 Not Found") == 0);
    assert(error_response.find("Content-Type: text/html; charset=utf-8") != std::string::npos);
    assert(error_response.find("Page not found") != std::string::npos);
    
    std::cout << "✓ Error response generation passed" << std::endl;
}

void test_uri_parsing() {
    std::cout << "Testing URI parsing..." << std::endl;
    
    HTTPHandler handler(4);
    
    // Test URI with query parameters
    const char* complex_request = 
        "GET /path/to/resource?param1=value1&param2=value%202 HTTP/1.1\r\n"
        "Host: test.com\r\n"
        "\r\n";
    
    assert(handler.process_incoming_data(complex_request, strlen(complex_request)));
    assert(handler.is_request_complete());
    
    const HTTPRequest& request = handler.get_current_request();
    assert(request.path == "/path/to/resource");
    assert(request.query_string == "param1=value1&param2=value%202");
    
    std::cout << "✓ URI parsing passed" << std::endl;
}

void test_keep_alive_handling() {
    std::cout << "Testing Keep-Alive handling..." << std::endl;
    
    HTTPHandler handler(5);
    
    // Test HTTP/1.1 default keep-alive
    const char* http11_request = 
        "GET / HTTP/1.1\r\n"
        "Host: test.com\r\n"
        "\r\n";
    
    assert(handler.process_incoming_data(http11_request, strlen(http11_request)));
    assert(handler.is_request_complete());
    assert(handler.should_keep_alive());  // Default for HTTP/1.1
    
    handler.reset_request_state();
    
    // Test explicit Connection: close
    const char* close_request = 
        "GET / HTTP/1.1\r\n"
        "Host: test.com\r\n"
        "Connection: close\r\n"
        "\r\n";
    
    assert(handler.process_incoming_data(close_request, strlen(close_request)));
    assert(handler.is_request_complete());
    assert(!handler.should_keep_alive());  // Explicit close
    
    std::cout << "✓ Keep-Alive handling passed" << std::endl;
}

void test_option_integration() {
    std::cout << "Testing socket option integration..." << std::endl;
    
    HTTPHandler handler(6);
    
    // Test setting HTTP timeout option
    svalue_t timeout_val;
    timeout_val.type = T_NUMBER;
    timeout_val.u.number = 60000;
    
    assert(handler.set_http_option(SO_HTTP_TIMEOUT, &timeout_val));
    
    // Test getting the option back
    svalue_t retrieved_val;
    assert(handler.get_http_option(SO_HTTP_TIMEOUT, &retrieved_val));
    assert(retrieved_val.type == T_NUMBER);
    assert(retrieved_val.u.number == 60000);
    
    // Test setting User-Agent
    svalue_t user_agent_val;
    user_agent_val.type = T_STRING;
    user_agent_val.u.string = "CustomAgent/1.0";
    
    assert(handler.set_http_option(SO_HTTP_USER_AGENT, &user_agent_val));
    
    std::cout << "✓ Socket option integration passed" << std::endl;
}

void test_partial_request_handling() {
    std::cout << "Testing partial request handling..." << std::endl;
    
    HTTPHandler handler(7);
    
    // Send request in chunks
    const char* chunk1 = "GET /test HTTP/1.1\r\n";
    const char* chunk2 = "Host: localhost\r\n";
    const char* chunk3 = "Content-Length: 5\r\n";
    const char* chunk4 = "\r\n";
    const char* chunk5 = "hello";
    
    assert(handler.process_incoming_data(chunk1, strlen(chunk1)));
    assert(!handler.is_request_complete());
    
    assert(handler.process_incoming_data(chunk2, strlen(chunk2)));
    assert(!handler.is_request_complete());
    
    assert(handler.process_incoming_data(chunk3, strlen(chunk3)));
    assert(!handler.is_request_complete());
    
    assert(handler.process_incoming_data(chunk4, strlen(chunk4)));
    assert(!handler.is_request_complete());  // Still need body
    
    assert(handler.process_incoming_data(chunk5, strlen(chunk5)));
    assert(handler.is_request_complete());
    
    const HTTPRequest& request = handler.get_current_request();
    assert(request.body == "hello");
    assert(request.content_length == 5);
    
    std::cout << "✓ Partial request handling passed" << std::endl;
}

void test_url_encoding() {
    std::cout << "Testing URL encoding/decoding..." << std::endl;
    
    // Test URL encoding
    std::string encoded = HTTPHandler::url_encode("hello world!@#$%^&*()");
    assert(encoded == "hello%20world%21%40%23%24%25%5E%26%2A%28%29");
    
    // Test URL decoding
    std::string decoded = HTTPHandler::url_decode("hello%20world%21");
    assert(decoded == "hello world!");
    
    // Test plus to space conversion
    std::string plus_decoded = HTTPHandler::url_decode("hello+world");
    assert(plus_decoded == "hello world");
    
    std::cout << "✓ URL encoding/decoding passed" << std::endl;
}

void test_html_escaping() {
    std::cout << "Testing HTML escaping..." << std::endl;
    
    std::string escaped = HTTPHandler::html_escape("<script>alert('xss')</script>");
    assert(escaped == "&lt;script&gt;alert(&#39;xss&#39;)&lt;/script&gt;");
    
    std::string ampersand_escaped = HTTPHandler::html_escape("Tom & Jerry");
    assert(ampersand_escaped == "Tom &amp; Jerry");
    
    std::cout << "✓ HTML escaping passed" << std::endl;
}

void test_mime_type_detection() {
    std::cout << "Testing MIME type detection..." << std::endl;
    
    assert(HTTPHandler::get_mime_type(".html") == "text/html");
    assert(HTTPHandler::get_mime_type(".json") == "application/json");
    assert(HTTPHandler::get_mime_type(".css") == "text/css");
    assert(HTTPHandler::get_mime_type(".js") == "application/javascript");
    assert(HTTPHandler::get_mime_type(".png") == "image/png");
    assert(HTTPHandler::get_mime_type(".unknown") == "application/octet-stream");
    
    std::cout << "✓ MIME type detection passed" << std::endl;
}

void test_error_handling() {
    std::cout << "Testing error handling..." << std::endl;
    
    HTTPHandler handler(8);
    
    // Test invalid request line
    const char* invalid_request = "INVALID REQUEST LINE\r\n\r\n";
    assert(!handler.process_incoming_data(invalid_request, strlen(invalid_request)));
    
    handler.reset_request_state();
    
    // Test oversized headers
    std::string large_header = "GET / HTTP/1.1\r\n";
    large_header += "Large-Header: ";
    large_header += std::string(10000, 'x');  // Very large header
    large_header += "\r\n\r\n";
    
    assert(!handler.process_incoming_data(large_header.c_str(), large_header.length()));
    
    std::cout << "✓ Error handling passed" << std::endl;
}

int main() {
    std::cout << "Running HTTP Handler Test Suite..." << std::endl;
    std::cout << "========================================" << std::endl;
    
    try {
        test_http_request_parsing();
        test_http_post_request();
        test_http_response_generation();
        test_uri_parsing();
        test_keep_alive_handling();
        test_option_integration();
        test_partial_request_handling();
        test_url_encoding();
        test_html_escaping();
        test_mime_type_detection();
        test_error_handling();
        
        std::cout << "========================================" << std::endl;
        std::cout << "✅ All HTTP Handler tests passed!" << std::endl;
        
        return 0;
    } catch (const std::exception& e) {
        std::cout << "❌ Test failed with exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cout << "❌ Test failed with unknown exception" << std::endl;
        return 1;
    }
}