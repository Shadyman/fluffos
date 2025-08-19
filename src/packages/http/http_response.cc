#include "http.h"
#include "http_parser.h"

/*
 * HTTP Response Generation
 * 
 * Utilities for generating HTTP responses in proper format.
 * This functionality was extracted from the original http_handler.cc
 * as part of architecture correction to proper package structure.
 */

#include <sstream>
#include <ctime>
#include <iomanip>

class HTTPResponseGenerator {
private:
    HTTPParser parser_;
    
public:
    HTTPResponseGenerator() = default;
    ~HTTPResponseGenerator() = default;
    
    // Generate complete HTTP response
    std::string generate_response(const HTTPResponse& response) {
        std::ostringstream oss;
        
        // Status line
        oss << parser_.get_version_string(response.version) << " "
            << static_cast<int>(response.status) << " "
            << parser_.get_status_text(response.status) << "\r\n";
        
        // Headers
        for (const auto& header : response.headers) {
            oss << header.first << ": " << header.second << "\r\n";
        }
        
        // Content-Length if not already set and body exists
        if (response.headers.find("content-length") == response.headers.end() && 
            !response.body.empty()) {
            oss << "Content-Length: " << response.body.length() << "\r\n";
        }
        
        // Date header if not set
        if (response.headers.find("date") == response.headers.end()) {
            std::time_t now = std::time(nullptr);
            std::tm* gmt = std::gmtime(&now);
            oss << "Date: " << std::put_time(gmt, "%a, %d %b %Y %H:%M:%S GMT") << "\r\n";
        }
        
        // Server header if not set
        if (response.headers.find("server") == response.headers.end()) {
            oss << "Server: FluffOS HTTP Server v1.0\r\n";
        }
        
        // End of headers
        oss << "\r\n";
        
        // Body
        if (!response.body.empty()) {
            oss << response.body;
        }
        
        return oss.str();
    }
    
    // Create error response
    std::string create_error_response(http_status status, const std::string& message = "") {
        HTTPResponse response;
        response.status = status;
        response.version = HTTP_VERSION_1_1;
        
        std::string body_message = message;
        if (body_message.empty()) {
            body_message = parser_.get_status_text(status);
        }
        
        std::ostringstream body_oss;
        body_oss << "<!DOCTYPE html>\n"
                 << "<html><head><title>" << static_cast<int>(status) << " " 
                 << parser_.get_status_text(status) << "</title></head>\n"
                 << "<body><h1>" << static_cast<int>(status) << " " 
                 << parser_.get_status_text(status) << "</h1>\n"
                 << "<p>" << body_message << "</p>\n"
                 << "</body></html>";
        
        response.body = body_oss.str();
        response.headers["content-type"] = "text/html; charset=utf-8";
        
        return generate_response(response);
    }
    
    // Create JSON response
    std::string create_json_response(const std::string& json_body, 
                                   http_status status = HTTP_STATUS_OK) {
        HTTPResponse response;
        response.status = status;
        response.version = HTTP_VERSION_1_1;
        response.body = json_body;
        response.headers["content-type"] = "application/json; charset=utf-8";
        
        return generate_response(response);
    }
    
    // Create redirect response
    std::string create_redirect_response(const std::string& location, 
                                       http_status status = HTTP_STATUS_FOUND) {
        HTTPResponse response;
        response.status = status;
        response.version = HTTP_VERSION_1_1;
        response.headers["location"] = location;
        response.headers["content-length"] = "0";
        
        return generate_response(response);
    }
};

// Global response generator instance
static HTTPResponseGenerator response_generator;