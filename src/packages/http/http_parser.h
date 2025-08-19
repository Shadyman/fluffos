#ifndef HTTP_PARSER_H_
#define HTTP_PARSER_H_

/*
 * HTTP Parser - HTTP/1.1 Request/Response Parsing
 * 
 * This component handles HTTP message parsing, validation, and structure
 * management. Moved from original http_handler.h as part of architecture
 * correction to proper package structure.
 * 
 * Responsibilities:
 * - HTTP request line parsing (method, URI, version)
 * - Header parsing and validation
 * - Content body processing
 * - URI decoding and parameter extraction
 * - HTTP message validation
 */

#include "base/package_api.h"
#include <string>
#include <unordered_map>
#include <vector>
#include <regex>

// HTTP Method enumeration
enum http_method {
    HTTP_GET = 0,
    HTTP_POST = 1,
    HTTP_PUT = 2,
    HTTP_DELETE = 3,
    HTTP_HEAD = 4,
    HTTP_OPTIONS = 5,
    HTTP_PATCH = 6,
    HTTP_TRACE = 7,
    HTTP_CONNECT = 8,
    HTTP_UNKNOWN = 99
};

// HTTP Version enumeration
enum http_version {
    HTTP_VERSION_1_0 = 10,
    HTTP_VERSION_1_1 = 11,
    HTTP_VERSION_2_0 = 20,
    HTTP_VERSION_UNKNOWN = 0
};

// HTTP Status codes
enum http_status {
    HTTP_STATUS_CONTINUE = 100,
    HTTP_STATUS_OK = 200,
    HTTP_STATUS_CREATED = 201,
    HTTP_STATUS_ACCEPTED = 202,
    HTTP_STATUS_NO_CONTENT = 204,
    HTTP_STATUS_MOVED_PERMANENTLY = 301,
    HTTP_STATUS_FOUND = 302,
    HTTP_STATUS_NOT_MODIFIED = 304,
    HTTP_STATUS_BAD_REQUEST = 400,
    HTTP_STATUS_UNAUTHORIZED = 401,
    HTTP_STATUS_FORBIDDEN = 403,
    HTTP_STATUS_NOT_FOUND = 404,
    HTTP_STATUS_METHOD_NOT_ALLOWED = 405,
    HTTP_STATUS_CONFLICT = 409,
    HTTP_STATUS_INTERNAL_SERVER_ERROR = 500,
    HTTP_STATUS_NOT_IMPLEMENTED = 501,
    HTTP_STATUS_BAD_GATEWAY = 502,
    HTTP_STATUS_SERVICE_UNAVAILABLE = 503
};

// HTTP Request structure
struct HTTPRequest {
    http_method method;
    std::string uri;
    std::string path;
    std::string query_string;
    http_version version;
    std::unordered_map<std::string, std::string> headers;
    std::string body;
    size_t content_length;
    bool keep_alive;
    bool is_complete;
    
    HTTPRequest() : method(HTTP_UNKNOWN), version(HTTP_VERSION_UNKNOWN), 
                   content_length(0), keep_alive(false), is_complete(false) {}
};

// HTTP Response structure
struct HTTPResponse {
    http_version version;
    http_status status;
    std::string status_text;
    std::unordered_map<std::string, std::string> headers;
    std::string body;
    bool keep_alive;
    
    HTTPResponse() : version(HTTP_VERSION_1_1), status(HTTP_STATUS_OK), 
                    status_text("OK"), keep_alive(false) {}
};

// HTTP Connection state
struct HTTPConnection {
    int socket_id;
    bool keep_alive;
    std::string buffer;
    HTTPRequest current_request;
    size_t bytes_needed;
    bool parsing_headers;
    
    HTTPConnection(int sock_id) : socket_id(sock_id), keep_alive(false), 
                                 bytes_needed(0), parsing_headers(true) {}
};

/*
 * HTTPParser Class - Core HTTP Message Processing
 */
class HTTPParser {
private:
    mutable std::string last_error_;
    
    // HTTP parsing limits
    static const size_t MAX_HEADER_SIZE = 8192;
    static const size_t MAX_BODY_SIZE = 1048576;  // 1MB default
    
    // Parsing helper methods
    bool parse_request_line(const std::string& line, HTTPRequest* request);
    bool parse_header_line(const std::string& line, HTTPRequest* request);
    http_method string_to_method(const std::string& method_str);
    http_version string_to_version(const std::string& version_str);
    
    // URI processing
    bool parse_uri(const std::string& uri, std::string* path, std::string* query);
    std::string decode_uri_component(const std::string& encoded);
    
    // Content processing
    bool validate_content_length(const HTTPRequest& request);
    void process_connection_headers(HTTPRequest* request);
    
public:
    HTTPParser();
    ~HTTPParser();
    
    // Core parsing methods
    bool parse_http_request(const char* data, size_t length, HTTPRequest* request);
    bool parse_http_response(const char* data, size_t length, HTTPResponse* response);
    
    // Request processing
    bool is_request_complete(const HTTPRequest& request) const;
    bool validate_request(const HTTPRequest& request) const;
    
    // Response processing  
    bool is_response_complete(const HTTPResponse& response) const;
    bool validate_response(const HTTPResponse& response) const;
    
    // URI utilities
    std::string encode_uri_component(const std::string& input);
    std::string decode_uri_component(const std::string& encoded);
    bool parse_query_string(const std::string& query, 
                           std::unordered_map<std::string, std::string>* params);
    
    // Header utilities
    std::string normalize_header_name(const std::string& name);
    bool is_valid_header_name(const std::string& name);
    bool is_valid_header_value(const std::string& value);
    
    // Method/version utilities
    const char* get_method_string(http_method method) const;
    const char* get_version_string(http_version version) const;
    const char* get_status_text(http_status status) const;
    
    // Validation utilities
    bool is_valid_method_for_request(http_method method) const;
    size_t get_max_request_size() const;
    size_t get_max_header_size() const;
    
    // Error handling
    const char* get_last_error() const { return last_error_.c_str(); }
    void clear_error() { last_error_.clear(); }
    
    // Static utility functions
    static std::string url_encode(const std::string& input);
    static std::string url_decode(const std::string& input);
    static std::string html_escape(const std::string& input);
};

/*
 * HTTP Constants and Utilities
 */

// Common MIME types
#define MIME_TYPE_TEXT_HTML "text/html"
#define MIME_TYPE_TEXT_PLAIN "text/plain"
#define MIME_TYPE_APPLICATION_JSON "application/json"
#define MIME_TYPE_APPLICATION_XML "application/xml"
#define MIME_TYPE_APPLICATION_FORM_URLENCODED "application/x-www-form-urlencoded"
#define MIME_TYPE_MULTIPART_FORM_DATA "multipart/form-data"
#define MIME_TYPE_IMAGE_PNG "image/png"
#define MIME_TYPE_IMAGE_JPEG "image/jpeg"
#define MIME_TYPE_TEXT_CSS "text/css"
#define MIME_TYPE_APPLICATION_JAVASCRIPT "application/javascript"

// Common headers
#define HTTP_HEADER_CONTENT_TYPE "Content-Type"
#define HTTP_HEADER_CONTENT_LENGTH "Content-Length"
#define HTTP_HEADER_CONNECTION "Connection"
#define HTTP_HEADER_HOST "Host"
#define HTTP_HEADER_USER_AGENT "User-Agent"
#define HTTP_HEADER_ACCEPT "Accept"
#define HTTP_HEADER_AUTHORIZATION "Authorization"
#define HTTP_HEADER_CACHE_CONTROL "Cache-Control"
#define HTTP_HEADER_DATE "Date"
#define HTTP_HEADER_EXPIRES "Expires"
#define HTTP_HEADER_LAST_MODIFIED "Last-Modified"
#define HTTP_HEADER_LOCATION "Location"
#define HTTP_HEADER_SERVER "Server"
#define HTTP_HEADER_SET_COOKIE "Set-Cookie"
#define HTTP_HEADER_COOKIE "Cookie"

// HTTP parsing limits
#define MAX_HTTP_HEADER_COUNT 100
#define MAX_HTTP_HEADER_NAME_LENGTH 100
#define MAX_HTTP_HEADER_VALUE_LENGTH 4096
#define MAX_HTTP_URI_LENGTH 4096
#define MAX_HTTP_METHOD_LENGTH 32

/*
 * Validation macros
 */
#define IS_VALID_HTTP_STATUS(status) ((status) >= 100 && (status) <= 599)
#define IS_SUCCESS_STATUS(status) ((status) >= 200 && (status) < 300)
#define IS_ERROR_STATUS(status) ((status) >= 400)
#define IS_REDIRECT_STATUS(status) ((status) >= 300 && (status) < 400)

#endif  // HTTP_PARSER_H_