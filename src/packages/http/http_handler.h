#ifndef HTTP_HANDLER_H_
#define HTTP_HANDLER_H_

/*
 * HTTP Handler - HTTP Request/Response Processing for Unified Socket Architecture
 * 
 * This component manages HTTP request processing, response generation, and
 * socket integration. It bridges HTTP protocol handling with the unified
 * socket architecture from Phase 1.
 * 
 * Responsibilities:
 * - HTTP request/response lifecycle management
 * - Socket integration via SocketOptionManager
 * - HTTP protocol compliance and validation
 * - Connection state management
 * - HTTP option processing for REST and HTTP socket options
 */

#include "base/package_api.h"
#include "http_parser.h"
#include "packages/sockets/socket_option_manager.h"
#include <memory>
#include <unordered_map>

// Forward declarations
class SocketOptionManager;

// Default HTTP constants - use values from socket_options.h to avoid conflicts
#define DEFAULT_HTTP_MAX_HEADERS 100
#define DEFAULT_HTTP_MAX_BODY_SIZE 1048576  // 1MB
#define MAX_HEADER_SIZE 8192
#define MAX_BODY_SIZE DEFAULT_HTTP_MAX_BODY_SIZE

/*
 * HTTPHandler Class - Core HTTP Processing Engine
 * 
 * This class handles the complete HTTP request/response lifecycle for a socket.
 * It integrates with Phase 1's unified socket architecture through the 
 * SocketOptionManager and provides HTTP-specific functionality.
 */
class HTTPHandler {
private:
    int socket_id_;
    std::unique_ptr<SocketOptionManager> option_manager_;
    std::unique_ptr<HTTPConnection> connection_;
    HTTPParser parser_;
    std::string buffer_;
    
    // Request processing state
    bool parsing_headers_;
    bool request_complete_;
    size_t content_bytes_needed_;
    
    // Connection management
    bool keep_alive_;
    bool connection_closed_;
    
    // Error tracking
    std::string last_error_;
    
    // Internal helper methods
    bool parse_headers();
    bool parse_body();
    bool parse_request_line(const std::string& line, HTTPRequest* request);
    bool parse_header_line(const std::string& line, HTTPRequest* request);
    http_method string_to_method(const std::string& method_str);
    http_version string_to_version(const std::string& version_str);
    bool parse_uri(const std::string& uri, std::string* path, std::string* query);
    std::string decode_uri_component(const std::string& encoded);
    
public:
    // Response generation helpers (moved to public for socket integration)
    std::string generate_response(http_status status, const std::string& body,
                                const std::unordered_map<std::string, std::string>& headers = {});
private:
    std::string format_response(const HTTPResponse& response);
    std::string get_status_text(http_status status);
    void add_default_headers(HTTPResponse* response);
    
    // Option processing helpers
    void apply_socket_options_to_request(HTTPRequest* request);
    void apply_socket_options_to_response(HTTPResponse* response);
    void process_connection_headers(HTTPRequest* request);
    void set_headers_from_mapping(HTTPResponse* response, const mapping_t* headers);
    
    // Validation helpers
    bool is_valid_header_name(const std::string& name);
    bool is_valid_header_value(const std::string& value);
    std::string get_mime_type(const std::string& extension);
    std::string get_content_type_with_charset(const std::string& mime_type, 
                                            const std::string& charset = "utf-8");
    
public:
    HTTPHandler(int socket_id);
    ~HTTPHandler();
    
    // Core request processing
    bool process_incoming_data(const char* data, size_t length);
    bool is_request_complete() const;
    const HTTPRequest& get_current_request() const;
    void reset_request_state();
    
    // Response generation
    std::string create_error_response(http_status status, const std::string& message = "");
    std::string create_success_response(const std::string& body, const std::string& content_type = "text/html");
    std::string create_json_response(const std::string& json_body, http_status status = HTTP_STATUS_OK);
    std::string create_redirect_response(const std::string& location, http_status status = HTTP_STATUS_FOUND);
    
    // Connection management
    bool should_keep_alive() const;
    void close_connection();
    size_t get_buffer_size() const;
    void clear_buffer();
    
    // Socket option integration
    bool set_http_option(int option, const svalue_t* value, object_t* caller);
    bool get_http_option(int option, svalue_t* result, object_t* caller);
    mapping_t* get_all_http_options(object_t* caller) const;
    
    // Request data access
    mapping_t* get_request_headers() const;
    const char* get_method_string(http_method method) const;
    const char* get_version_string(http_version version) const;
    
    // Utility methods
    bool is_valid_method_for_request(http_method method) const;
    size_t get_max_request_size() const;
    static std::string url_encode(const std::string& input);
    static std::string url_decode(const std::string& input);
    static std::string html_escape(const std::string& input);
    
    // Debug and monitoring
    void dump_request_state(outbuffer_t* buffer) const;
    void dump_connection_state(outbuffer_t* buffer) const;
    
    // HTTP Socket Option Processing (Phase 2 Golf Implementation)
    bool process_rest_add_route_option(const svalue_t* value, object_t* caller);
    bool process_rest_openapi_info_option(const svalue_t* value, object_t* caller);
    bool process_rest_jwt_secret_option(const svalue_t* value, object_t* caller);
    bool process_http_headers_option(const svalue_t* value, object_t* caller);
    
    // Get socket ID for external access
    int get_socket_id() const { return socket_id_; }
};

/*
 * HTTP Handler Management Functions
 * 
 * These functions manage the HTTPHandler instances associated with sockets
 * and integrate with the unified socket architecture.
 */

// Socket integration functions
bool socket_enable_http_mode(int socket_id, const mapping_t* options = nullptr);
bool socket_is_http_mode(int socket_id);
int socket_process_http_data(int socket_id, const char* data, size_t length);
char* socket_generate_http_response(int socket_id, int status, const char* body, 
                                   const mapping_t* headers = nullptr);

// Handler registry management
HTTPHandler* get_http_handler(int socket_id);
bool register_http_handler(int socket_id, std::unique_ptr<HTTPHandler> handler);
void cleanup_http_handler(int socket_id);

// HTTP Package initialization
void init_http_handler_registry();
void cleanup_http_handler_registry();

#endif  // HTTP_HANDLER_H_