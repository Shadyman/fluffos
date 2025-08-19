#ifndef REST_HANDLER_H_
#define REST_HANDLER_H_

#include "http_handler.h"
#include "socket_option_manager.h"
#include "base/package_api.h"
#include "packages/sockets/socket_efuns.h"
#include <unordered_map>
#include <vector>
#include <functional>
#include <regex>
#include <memory>

/*
 * RESTHandler - RESTful API Framework for Unified Socket Architecture
 * 
 * This class builds on HTTPHandler to provide comprehensive REST API support
 * for the FluffOS unified socket system. It extends HTTP/1.1 functionality
 * with RESTful routing, JSON processing, and API management.
 * 
 * Features:
 * - RESTful endpoint registration and routing
 * - JSON request/response processing with validation
 * - Route parameter extraction and validation
 * - REST API middleware support
 * - OpenAPI documentation generation
 * - Integration with REST_* socket options (no SO_ prefix)
 * - Built on Echo's HTTP handler foundation
 */

// Forward declarations
struct RouteHandler;
struct RouteMatch;
struct RestRequest;
struct RestResponse;

// REST-specific enumerations
enum rest_content_type {
    REST_CONTENT_JSON = 0,
    REST_CONTENT_XML = 1,
    REST_CONTENT_FORM = 2,
    REST_CONTENT_TEXT = 3,
    REST_CONTENT_BINARY = 4,
    REST_CONTENT_MULTIPART = 5,
    REST_CONTENT_UNKNOWN = 99
};

enum rest_validation_level {
    REST_VALIDATION_NONE = 0,
    REST_VALIDATION_BASIC = 1,
    REST_VALIDATION_STRICT = 2,
    REST_VALIDATION_SCHEMA = 3
};

enum rest_error_level {
    REST_ERROR_INFO = 0,
    REST_ERROR_WARNING = 1,
    REST_ERROR_ERROR = 2,
    REST_ERROR_CRITICAL = 3
};

// REST Route structure
struct RestRoute {
    int route_id;
    std::string method;  // GET, POST, PUT, DELETE, etc.
    std::string pattern; // /api/users/{id} format
    std::regex compiled_pattern;
    std::vector<std::string> param_names;
    std::string handler_object;    // LPC object to call
    std::string handler_function;  // Function name to call
    std::string description;
    bool requires_auth;
    std::vector<std::string> tags;
    std::unordered_map<std::string, std::string> response_schemas;
    
    RestRoute() : route_id(0), requires_auth(false) {}
};

// REST Request structure (extends HTTP)
struct RestRequest {
    HTTPRequest http_request;     // Base HTTP request from Echo's handler
    rest_content_type content_type;
    std::unordered_map<std::string, std::string> path_params;
    std::unordered_map<std::string, std::string> query_params;
    mapping_t* json_body;         // Parsed JSON body as LPC mapping
    std::string matched_route_pattern;
    int matched_route_id;
    std::vector<std::string> validation_errors;
    
    RestRequest() : content_type(REST_CONTENT_UNKNOWN), json_body(nullptr), 
                   matched_route_id(-1) {}
};

// REST Response structure (extends HTTP)
struct RestResponse {
    HTTPResponse http_response;   // Base HTTP response from Echo's handler
    rest_content_type content_type;
    mapping_t* json_body;         // JSON response as LPC mapping
    std::string error_message;
    std::vector<std::string> validation_errors;
    bool is_error_response;
    
    RestResponse() : content_type(REST_CONTENT_JSON), json_body(nullptr), 
                    is_error_response(false) {}
};

// Route matching result
struct RouteMatch {
    bool found;
    const RestRoute* route;
    std::unordered_map<std::string, std::string> params;
    
    RouteMatch() : found(false), route(nullptr) {}
};

// REST API Information structure
struct RestApiInfo {
    std::string title;
    std::string version;
    std::string description;
    std::string base_path;
    std::vector<std::string> schemes;  // http, https
    std::vector<std::string> consumes; // application/json, etc.
    std::vector<std::string> produces; // application/json, etc.
    std::unordered_map<std::string, std::string> contact;
    std::unordered_map<std::string, std::string> license;
    
    RestApiInfo() : title("FluffOS REST API"), version("1.0.0"), 
                   base_path("/api") {
        schemes.push_back("http");
        consumes.push_back("application/json");
        produces.push_back("application/json");
    }
};

class RESTHandler {
private:
    int socket_id_;
    std::unique_ptr<HTTPHandler> http_handler_;
    std::unique_ptr<SocketOptionManager> option_manager_;
    mutable std::string last_error_;
    
    // REST configuration
    RestApiInfo api_info_;
    std::vector<std::unique_ptr<RestRoute>> routes_;
    std::unordered_map<std::string, std::string> middleware_functions_;
    std::string docs_path_;
    bool cors_enabled_;
    std::unordered_map<std::string, std::string> cors_headers_;
    
    // JSON processing
    rest_validation_level validation_level_;
    size_t max_json_size_;
    
    // Route processing
    bool compile_route_pattern(RestRoute* route);
    RouteMatch find_matching_route(const std::string& method, const std::string& path);
    bool extract_path_parameters(const RestRoute& route, const std::string& path, 
                                std::unordered_map<std::string, std::string>* params);
    
    // Parameter processing
    bool parse_query_parameters(const std::string& query_string, RestRequest* request);
    bool parse_json_body(const std::string& body, RestRequest* request);
    bool validate_request_parameters(const RestRoute& route, const RestRequest& request);
    
    // Response generation
    std::string serialize_json_response(const RestResponse& response);
    std::string create_error_json(http_status status, const std::string& message, 
                                 const std::vector<std::string>& details = {});
    void add_cors_headers(RestResponse* response);
    void add_api_headers(RestResponse* response);
    
    // Content type handling
    rest_content_type detect_content_type(const HTTPRequest& request);
    std::string get_content_type_header(rest_content_type type);
    
    // Validation helpers
    bool is_valid_json(const std::string& json_str);
    bool validate_route_pattern(const std::string& pattern);
    bool validate_method(const std::string& method);
    
    // Option integration helpers
    void apply_rest_options_to_request(RestRequest* request);
    void apply_rest_options_to_response(RestResponse* response);
    
public:
    RESTHandler(int socket_id);
    ~RESTHandler();
    
    // Core REST processing
    bool process_rest_request(const char* data, size_t length);
    std::string generate_rest_response(const RestResponse& response);
    
    // Route management
    bool add_route(const std::string& method, const std::string& pattern,
                  const std::string& handler_object, const std::string& handler_function,
                  const std::string& description = "");
    bool remove_route(int route_id);
    bool remove_route(const std::string& method, const std::string& pattern);
    void clear_all_routes();
    
    // Route registration from LPC
    bool register_route_from_mapping(const mapping_t* route_config);
    array_t* get_all_routes() const;
    mapping_t* get_route_info(int route_id) const;
    
    // Request processing
    bool is_rest_request_complete() const;
    const RestRequest& get_current_request() const;
    void reset_request_state();
    
    // Response generation helpers
    std::string create_json_success_response(const mapping_t* data, 
                                           http_status status = HTTP_STATUS_OK);
    std::string create_json_error_response(http_status status, const std::string& message,
                                         const mapping_t* details = nullptr);
    std::string create_validation_error_response(const std::vector<std::string>& errors);
    
    // API documentation
    std::string generate_openapi_spec();
    std::string generate_api_docs_html();
    bool set_api_info_from_mapping(const mapping_t* api_info);
    mapping_t* get_api_info() const;
    
    // Middleware support
    bool add_middleware(const std::string& name, const std::string& function);
    bool remove_middleware(const std::string& name);
    std::vector<std::string> get_middleware_chain() const;
    
    // CORS support
    void enable_cors(const mapping_t* cors_config = nullptr);
    void disable_cors();
    bool is_cors_enabled() const;
    std::string handle_cors_preflight(const HTTPRequest& request);
    
    // Option integration (REST_* options without SO_ prefix)
    bool set_rest_option(int option, const svalue_t* value, object_t* caller = nullptr);
    bool get_rest_option(int option, svalue_t* result, object_t* caller = nullptr);
    mapping_t* get_all_rest_options(object_t* caller = nullptr) const;
    
    // JSON utilities
    mapping_t* parse_json_to_mapping(const std::string& json_str);
    std::string serialize_mapping_to_json(const mapping_t* mapping);
    bool is_valid_json_schema(const mapping_t* schema);
    
    // Validation
    bool validate_json_against_schema(const mapping_t* data, const mapping_t* schema);
    std::vector<std::string> get_validation_errors() const;
    void set_validation_level(rest_validation_level level);
    rest_validation_level get_validation_level() const;
    
    // Connection management (delegates to HTTP handler)
    bool should_keep_alive() const;
    void close_connection();
    size_t get_buffer_size() const;
    void clear_buffer();
    
    // Error handling
    const char* get_last_error() const { return last_error_.c_str(); }
    void clear_error() { last_error_.clear(); }
    
    // Debug and introspection
    void dump_rest_state(outbuffer_t* buffer) const;
    void dump_routes(outbuffer_t* buffer) const;
    void dump_api_stats(outbuffer_t* buffer) const;
    
    // Utility methods
    static std::string escape_json_string(const std::string& input);
    static std::string unescape_json_string(const std::string& input);
    static bool is_valid_route_pattern(const std::string& pattern);
    static std::string normalize_route_pattern(const std::string& pattern);
    static std::vector<std::string> extract_route_parameter_names(const std::string& pattern);
    
    // HTTP handler access
    HTTPHandler* get_http_handler() const { return http_handler_.get(); }
};

/*
 * REST Integration with Socket System
 */

// Initialize REST mode for a socket
bool socket_enable_rest_mode(int socket_id, const mapping_t* options = nullptr);

// Check if socket is in REST mode
bool socket_is_rest_mode(int socket_id);

// Process REST data for socket
int socket_process_rest_data(int socket_id, const char* data, size_t length);

// Generate REST response for socket
string_t* socket_generate_rest_response(int socket_id, const mapping_t* response_data, 
                                       int status = HTTP_STATUS_OK);

// Get REST handler for socket
RESTHandler* get_rest_handler(int socket_id);

// Route management from LPC
bool socket_rest_add_route(int socket_id, const mapping_t* route_config);
bool socket_rest_remove_route(int socket_id, int route_id);
array_t* socket_rest_get_routes(int socket_id);

/*
 * REST Constants and Utilities
 */

// Common REST patterns
#define REST_PATTERN_ID_PARAM "{id}"
#define REST_PATTERN_WILDCARD "*"
#define REST_PATTERN_OPTIONAL_PARAM "{param?}"

// JSON content types
#define REST_CONTENT_TYPE_JSON "application/json"
#define REST_CONTENT_TYPE_JSON_UTF8 "application/json; charset=utf-8"

// Common REST response patterns
#define REST_SUCCESS_MESSAGE "success"
#define REST_ERROR_MESSAGE "error"
#define REST_VALIDATION_ERROR_MESSAGE "validation_error"

// REST-specific headers
#define REST_HEADER_API_VERSION "X-API-Version"
#define REST_HEADER_REQUEST_ID "X-Request-ID"
#define REST_HEADER_RATE_LIMIT_REMAINING "X-RateLimit-Remaining"
#define REST_HEADER_RATE_LIMIT_RESET "X-RateLimit-Reset"

// CORS headers
#define CORS_HEADER_ALLOW_ORIGIN "Access-Control-Allow-Origin"
#define CORS_HEADER_ALLOW_METHODS "Access-Control-Allow-Methods"
#define CORS_HEADER_ALLOW_HEADERS "Access-Control-Allow-Headers"
#define CORS_HEADER_EXPOSE_HEADERS "Access-Control-Expose-Headers"
#define CORS_HEADER_MAX_AGE "Access-Control-Max-Age"
#define CORS_HEADER_ALLOW_CREDENTIALS "Access-Control-Allow-Credentials"

// OpenAPI specification constants
#define OPENAPI_VERSION "3.0.0"
#define OPENAPI_MEDIA_TYPE_JSON "application/json"

/*
 * Validation macros for REST
 */
#define IS_VALID_REST_METHOD(method) \
    ((method) == "GET" || (method) == "POST" || (method) == "PUT" || \
     (method) == "DELETE" || (method) == "PATCH" || (method) == "HEAD" || \
     (method) == "OPTIONS")

#define IS_JSON_CONTENT_TYPE(content_type) \
    ((content_type) == REST_CONTENT_JSON)

#define IS_REST_SUCCESS_STATUS(status) \
    ((status) >= 200 && (status) < 300)

#define IS_REST_ERROR_STATUS(status) \
    ((status) >= 400)

/*
 * REST Error Codes
 */
#define REST_ERROR_INVALID_JSON 4001
#define REST_ERROR_VALIDATION_FAILED 4002
#define REST_ERROR_ROUTE_NOT_FOUND 4003
#define REST_ERROR_METHOD_NOT_ALLOWED 4004
#define REST_ERROR_MISSING_PARAMETERS 4005
#define REST_ERROR_INVALID_PARAMETERS 4006
#define REST_ERROR_AUTHENTICATION_REQUIRED 4007
#define REST_ERROR_AUTHORIZATION_FAILED 4008
#define REST_ERROR_RATE_LIMIT_EXCEEDED 4009
#define REST_ERROR_INTERNAL_SERVER_ERROR 5001

#endif  // REST_HANDLER_H_