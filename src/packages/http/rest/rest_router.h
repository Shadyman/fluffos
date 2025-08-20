#ifndef REST_ROUTER_H_
#define REST_ROUTER_H_

/*
 * REST Router - RESTful API Routing Framework
 * 
 * Provides advanced REST routing functionality including pattern matching,
 * parameter extraction, and middleware support. This component was moved
 * from the sockets package as part of architecture correction to proper
 * http/rest/ package structure.
 * 
 * Integration with HTTP Package:
 * - Built on top of HTTP parser and response components
 * - Uses HTTPRequest/HTTPResponse structures
 * - Integrates with socket option system (REST_* options without SO_ prefix)
 */

#include "../http_parser.h"
#include "packages/sockets/socket_option_manager.h"
#include <memory>
#include <vector>
#include <functional>
#include <regex>

// Forward declarations
struct RestRoute;
struct RouteMatch;
class RestRouter;

// REST-specific types moved from original rest_handler.h
enum rest_content_type {
    REST_CONTENT_JSON = 0,
    REST_CONTENT_XML = 1,
    REST_CONTENT_FORM = 2,
    REST_CONTENT_TEXT = 3,
    REST_CONTENT_BINARY = 4,
    REST_CONTENT_MULTIPART = 5,
    REST_CONTENT_UNKNOWN = 99
};

// REST validation levels (distinct from socket option values)
enum rest_validation_level {
    REST_VALIDATE_NONE = 0,
    REST_VALIDATE_BASIC = 1,
    REST_VALIDATE_STRICT = 2,
    REST_VALIDATE_SCHEMA = 3
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

// Route matching result
struct RouteMatch {
    bool found;
    const RestRoute* route;
    std::unordered_map<std::string, std::string> params;
    
    RouteMatch() : found(false), route(nullptr) {}
};

/*
 * RestRouter Class - Core REST Routing Engine
 */
class RestRouter {
private:
    std::vector<std::unique_ptr<RestRoute>> routes_;
    std::unordered_map<std::string, std::string> middleware_functions_;
    mutable std::string last_error_;
    int next_route_id_;
    
    // Route compilation and matching
    bool compile_route_pattern(RestRoute* route);
    bool extract_path_parameters(const RestRoute& route, const std::string& path, 
                                std::unordered_map<std::string, std::string>* params);
    
    // Pattern processing
    std::string convert_pattern_to_regex(const std::string& pattern);
    std::vector<std::string> extract_parameter_names(const std::string& pattern);
    
public:
    RestRouter();
    ~RestRouter();
    
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
    
    // Route matching
    RouteMatch find_matching_route(const std::string& method, const std::string& path);
    
    // Middleware management
    bool add_middleware(const std::string& name, const std::string& function);
    bool remove_middleware(const std::string& name);
    std::vector<std::string> get_middleware_chain() const;
    
    // Validation and utilities
    bool validate_route_pattern(const std::string& pattern);
    bool validate_method(const std::string& method);
    std::string normalize_route_pattern(const std::string& pattern);
    
    // Error handling
    const char* get_last_error() const { return last_error_.c_str(); }
    void clear_error() { last_error_.clear(); }
    
    // Debug and introspection
    void dump_routes(outbuffer_t* buffer) const;
    size_t get_route_count() const { return routes_.size(); }
    
    // Static utilities
    static bool is_valid_route_pattern(const std::string& pattern);
    static std::vector<std::string> extract_route_parameter_names(const std::string& pattern);
};

/*
 * REST Socket Options - Use values from socket_options.h
 * No redefinition needed, import via socket_options.h
 */
#include "../../sockets/socket_options.h"
// REST options now defined in socket_options.h:
// REST_ADD_ROUTE = 111, REST_OPENAPI_INFO = 112, REST_JWT_SECRET = 113, etc.

/*
 * REST Integration Functions
 */

// Initialize REST router for a socket
bool socket_enable_rest_mode(int socket_id, const mapping_t* options = nullptr);

// Get REST router for socket
RestRouter* get_rest_router(int socket_id);

// Route management from LPC
bool socket_rest_add_route(int socket_id, const mapping_t* route_config);
bool socket_rest_remove_route(int socket_id, int route_id);
array_t* socket_rest_get_routes(int socket_id);

/*
 * REST Constants
 */

// Common REST patterns
#define REST_PATTERN_ID_PARAM "{id}"
#define REST_PATTERN_WILDCARD "*"
#define REST_PATTERN_OPTIONAL_PARAM "{param?}"

// REST-specific headers
#define REST_HEADER_API_VERSION "X-API-Version"
#define REST_HEADER_REQUEST_ID "X-Request-ID"
#define REST_HEADER_RATE_LIMIT_REMAINING "X-RateLimit-Remaining"
#define REST_HEADER_RATE_LIMIT_RESET "X-RateLimit-Reset"

/*
 * Validation macros for REST
 */
#define IS_VALID_REST_METHOD(method) \
    ((method) == "GET" || (method) == "POST" || (method) == "PUT" || \
     (method) == "DELETE" || (method) == "PATCH" || (method) == "HEAD" || \
     (method) == "OPTIONS")

#endif  // REST_ROUTER_H_