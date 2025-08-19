#include "rest_handler.h"
#include "socket_option_manager.h"
#include "base/package_api.h"
#include "packages/sockets/socket_efuns.h"

/*
 * REST Socket External Functions (efuns)
 * 
 * This file provides LPC-callable functions for REST API functionality
 * built on top of the unified socket architecture and HTTP handlers.
 */

// Function declarations for LPC integration
void f_socket_enable_rest_mode();
void f_socket_is_rest_mode();
void f_socket_rest_add_route();
void f_socket_rest_remove_route();
void f_socket_rest_get_routes();
void f_socket_rest_set_api_info();
void f_socket_rest_get_api_info();
void f_socket_rest_generate_docs();
void f_socket_rest_enable_cors();
void f_socket_rest_disable_cors();
void f_socket_rest_parse_json();
void f_socket_rest_serialize_json();

/*
 * socket_enable_rest_mode()
 * 
 * Enable REST API mode for a socket.
 * 
 * Syntax: int socket_enable_rest_mode(int socket_id, mapping options)
 * 
 * Parameters:
 *   socket_id - Socket file descriptor
 *   options - Optional mapping with REST configuration
 * 
 * Returns: 1 for success, 0 for failure
 */
void f_socket_enable_rest_mode() {
    int socket_id;
    mapping_t* options = nullptr;
    
    // Get parameters from LPC stack
    if (st_num_arg == 2) {
        if (sp->type != T_MAPPING) {
            bad_argument(sp, T_MAPPING, 2, F_SOCKET_ENABLE_REST_MODE);
        }
        options = sp->u.map;
        pop_stack();
    }
    
    if (sp->type != T_NUMBER) {
        bad_argument(sp, T_NUMBER, 1, F_SOCKET_ENABLE_REST_MODE);
    }
    socket_id = sp->u.number;
    pop_stack();
    
    // Enable REST mode
    bool result = socket_enable_rest_mode(socket_id, options);
    
    push_number(result ? 1 : 0);
}

/*
 * socket_is_rest_mode()
 * 
 * Check if socket is in REST mode.
 * 
 * Syntax: int socket_is_rest_mode(int socket_id)
 * 
 * Parameters:
 *   socket_id - Socket file descriptor
 * 
 * Returns: 1 if REST mode is enabled, 0 otherwise
 */
void f_socket_is_rest_mode() {
    int socket_id;
    
    if (sp->type != T_NUMBER) {
        bad_argument(sp, T_NUMBER, 1, F_SOCKET_IS_REST_MODE);
    }
    socket_id = sp->u.number;
    pop_stack();
    
    bool result = socket_is_rest_mode(socket_id);
    
    push_number(result ? 1 : 0);
}

/*
 * socket_rest_add_route()
 * 
 * Add a REST route to the socket.
 * 
 * Syntax: int socket_rest_add_route(int socket_id, mapping route_config)
 * 
 * Parameters:
 *   socket_id - Socket file descriptor
 *   route_config - Mapping with route configuration:
 *     "method" : string - HTTP method (GET, POST, etc.)
 *     "pattern" : string - URL pattern (/api/users/{id})
 *     "handler_object" : string - LPC object file
 *     "handler_function" : string - Function to call
 *     "description" : string - Optional description
 *     "requires_auth" : int - Whether authentication is required
 * 
 * Returns: Route ID for success, 0 for failure
 */
void f_socket_rest_add_route() {
    int socket_id;
    mapping_t* route_config;
    
    // Get route configuration mapping
    if (sp->type != T_MAPPING) {
        bad_argument(sp, T_MAPPING, 2, F_SOCKET_REST_ADD_ROUTE);
    }
    route_config = sp->u.map;
    pop_stack();
    
    // Get socket ID
    if (sp->type != T_NUMBER) {
        bad_argument(sp, T_NUMBER, 1, F_SOCKET_REST_ADD_ROUTE);
    }
    socket_id = sp->u.number;
    pop_stack();
    
    // Get REST handler
    RESTHandler* handler = get_rest_handler(socket_id);
    if (!handler) {
        push_number(0);
        return;
    }
    
    // Extract route parameters from mapping
    svalue_t* method_val = find_value_in_mapping(route_config, const_cast<svalue_t*>(&const0));
    svalue_t* pattern_val = find_value_in_mapping(route_config, const_cast<svalue_t*>(&const1));
    svalue_t* handler_obj_val = find_value_in_mapping(route_config, const_cast<svalue_t*>(&const2));
    svalue_t* handler_func_val = find_value_in_mapping(route_config, const_cast<svalue_t*>(&const3));
    svalue_t* description_val = find_value_in_mapping(route_config, const_cast<svalue_t*>(&const4));
    
    // Validate required parameters
    if (!method_val || method_val->type != T_STRING ||
        !pattern_val || pattern_val->type != T_STRING ||
        !handler_obj_val || handler_obj_val->type != T_STRING ||
        !handler_func_val || handler_func_val->type != T_STRING) {
        push_number(0);
        return;
    }
    
    std::string method(method_val->u.string);
    std::string pattern(pattern_val->u.string);
    std::string handler_object(handler_obj_val->u.string);
    std::string handler_function(handler_func_val->u.string);
    std::string description = (description_val && description_val->type == T_STRING) ? 
                             std::string(description_val->u.string) : "";
    
    // Add route
    bool result = handler->add_route(method, pattern, handler_object, handler_function, description);
    
    // For now, return 1 for success, 0 for failure
    // In a full implementation, this would return the actual route ID
    push_number(result ? 1 : 0);
}

/*
 * socket_rest_remove_route()
 * 
 * Remove a REST route from the socket.
 * 
 * Syntax: int socket_rest_remove_route(int socket_id, int route_id)
 *     or: int socket_rest_remove_route(int socket_id, string method, string pattern)
 * 
 * Parameters:
 *   socket_id - Socket file descriptor
 *   route_id - Route ID to remove (from socket_rest_add_route)
 *   method - HTTP method
 *   pattern - URL pattern
 * 
 * Returns: 1 for success, 0 for failure
 */
void f_socket_rest_remove_route() {
    int socket_id;
    
    // Get REST handler first
    if (st_num_arg < 2) {
        error("Too few arguments to socket_rest_remove_route()");
    }
    
    // Get socket ID (always first parameter)
    svalue_t* socket_arg = sp - (st_num_arg - 1);
    if (socket_arg->type != T_NUMBER) {
        bad_argument(socket_arg, T_NUMBER, 1, F_SOCKET_REST_REMOVE_ROUTE);
    }
    socket_id = socket_arg->u.number;
    
    RESTHandler* handler = get_rest_handler(socket_id);
    if (!handler) {
        // Pop all arguments and return failure
        while (st_num_arg > 0) {
            pop_stack();
        }
        push_number(0);
        return;
    }
    
    bool result = false;
    
    if (st_num_arg == 2) {
        // Remove by route ID
        if (sp->type != T_NUMBER) {
            bad_argument(sp, T_NUMBER, 2, F_SOCKET_REST_REMOVE_ROUTE);
        }
        int route_id = sp->u.number;
        result = handler->remove_route(route_id);
        
    } else if (st_num_arg == 3) {
        // Remove by method and pattern
        if (sp->type != T_STRING) {
            bad_argument(sp, T_STRING, 3, F_SOCKET_REST_REMOVE_ROUTE);
        }
        std::string pattern(sp->u.string);
        pop_stack();
        
        if (sp->type != T_STRING) {
            bad_argument(sp, T_STRING, 2, F_SOCKET_REST_REMOVE_ROUTE);
        }
        std::string method(sp->u.string);
        
        result = handler->remove_route(method, pattern);
    }
    
    // Clean up stack
    while (st_num_arg > 0) {
        pop_stack();
    }
    
    push_number(result ? 1 : 0);
}

/*
 * socket_rest_get_routes()
 * 
 * Get all REST routes for a socket.
 * 
 * Syntax: array socket_rest_get_routes(int socket_id)
 * 
 * Parameters:
 *   socket_id - Socket file descriptor
 * 
 * Returns: Array of route mappings
 */
void f_socket_rest_get_routes() {
    int socket_id;
    
    if (sp->type != T_NUMBER) {
        bad_argument(sp, T_NUMBER, 1, F_SOCKET_REST_GET_ROUTES);
    }
    socket_id = sp->u.number;
    pop_stack();
    
    RESTHandler* handler = get_rest_handler(socket_id);
    if (!handler) {
        push_number(0);
        return;
    }
    
    // Get routes array
    array_t* routes = handler->get_all_routes();
    if (routes) {
        push_refed_array(routes);
    } else {
        push_number(0);
    }
}

/*
 * socket_rest_set_api_info()
 * 
 * Set API information for OpenAPI documentation.
 * 
 * Syntax: int socket_rest_set_api_info(int socket_id, mapping api_info)
 * 
 * Parameters:
 *   socket_id - Socket file descriptor
 *   api_info - Mapping with API information:
 *     "title" : string - API title
 *     "version" : string - API version
 *     "description" : string - API description
 *     "base_path" : string - Base URL path
 * 
 * Returns: 1 for success, 0 for failure
 */
void f_socket_rest_set_api_info() {
    int socket_id;
    mapping_t* api_info;
    
    // Get API info mapping
    if (sp->type != T_MAPPING) {
        bad_argument(sp, T_MAPPING, 2, F_SOCKET_REST_SET_API_INFO);
    }
    api_info = sp->u.map;
    pop_stack();
    
    // Get socket ID
    if (sp->type != T_NUMBER) {
        bad_argument(sp, T_NUMBER, 1, F_SOCKET_REST_SET_API_INFO);
    }
    socket_id = sp->u.number;
    pop_stack();
    
    RESTHandler* handler = get_rest_handler(socket_id);
    if (!handler) {
        push_number(0);
        return;
    }
    
    bool result = handler->set_api_info_from_mapping(api_info);
    push_number(result ? 1 : 0);
}

/*
 * socket_rest_get_api_info()
 * 
 * Get API information for a socket.
 * 
 * Syntax: mapping socket_rest_get_api_info(int socket_id)
 * 
 * Parameters:
 *   socket_id - Socket file descriptor
 * 
 * Returns: Mapping with API information
 */
void f_socket_rest_get_api_info() {
    int socket_id;
    
    if (sp->type != T_NUMBER) {
        bad_argument(sp, T_NUMBER, 1, F_SOCKET_REST_GET_API_INFO);
    }
    socket_id = sp->u.number;
    pop_stack();
    
    RESTHandler* handler = get_rest_handler(socket_id);
    if (!handler) {
        push_number(0);
        return;
    }
    
    mapping_t* api_info = handler->get_api_info();
    if (api_info) {
        push_refed_mapping(api_info);
    } else {
        push_number(0);
    }
}

/*
 * socket_rest_generate_docs()
 * 
 * Generate OpenAPI documentation for REST API.
 * 
 * Syntax: string socket_rest_generate_docs(int socket_id, string format)
 * 
 * Parameters:
 *   socket_id - Socket file descriptor
 *   format - Documentation format ("json", "yaml", "html")
 * 
 * Returns: Generated documentation string
 */
void f_socket_rest_generate_docs() {
    int socket_id;
    std::string format = "json";  // default format
    
    // Get format if provided
    if (st_num_arg == 2) {
        if (sp->type != T_STRING) {
            bad_argument(sp, T_STRING, 2, F_SOCKET_REST_GENERATE_DOCS);
        }
        format = std::string(sp->u.string);
        pop_stack();
    }
    
    // Get socket ID
    if (sp->type != T_NUMBER) {
        bad_argument(sp, T_NUMBER, 1, F_SOCKET_REST_GENERATE_DOCS);
    }
    socket_id = sp->u.number;
    pop_stack();
    
    RESTHandler* handler = get_rest_handler(socket_id);
    if (!handler) {
        push_number(0);
        return;
    }
    
    std::string docs;
    if (format == "html") {
        docs = handler->generate_api_docs_html();
    } else {
        // Default to JSON OpenAPI spec
        docs = handler->generate_openapi_spec();
    }
    
    if (!docs.empty()) {
        push_malloced_string(string_copy(docs.c_str()));
    } else {
        push_number(0);
    }
}

/*
 * socket_rest_enable_cors()
 * 
 * Enable CORS (Cross-Origin Resource Sharing) for REST API.
 * 
 * Syntax: int socket_rest_enable_cors(int socket_id, mapping cors_config)
 * 
 * Parameters:
 *   socket_id - Socket file descriptor
 *   cors_config - Optional CORS configuration mapping
 * 
 * Returns: 1 for success, 0 for failure
 */
void f_socket_rest_enable_cors() {
    int socket_id;
    mapping_t* cors_config = nullptr;
    
    // Get CORS config if provided
    if (st_num_arg == 2) {
        if (sp->type != T_MAPPING) {
            bad_argument(sp, T_MAPPING, 2, F_SOCKET_REST_ENABLE_CORS);
        }
        cors_config = sp->u.map;
        pop_stack();
    }
    
    // Get socket ID
    if (sp->type != T_NUMBER) {
        bad_argument(sp, T_NUMBER, 1, F_SOCKET_REST_ENABLE_CORS);
    }
    socket_id = sp->u.number;
    pop_stack();
    
    RESTHandler* handler = get_rest_handler(socket_id);
    if (!handler) {
        push_number(0);
        return;
    }
    
    handler->enable_cors(cors_config);
    push_number(1);
}

/*
 * socket_rest_disable_cors()
 * 
 * Disable CORS for REST API.
 * 
 * Syntax: int socket_rest_disable_cors(int socket_id)
 * 
 * Parameters:
 *   socket_id - Socket file descriptor
 * 
 * Returns: 1 for success, 0 for failure
 */
void f_socket_rest_disable_cors() {
    int socket_id;
    
    if (sp->type != T_NUMBER) {
        bad_argument(sp, T_NUMBER, 1, F_SOCKET_REST_DISABLE_CORS);
    }
    socket_id = sp->u.number;
    pop_stack();
    
    RESTHandler* handler = get_rest_handler(socket_id);
    if (!handler) {
        push_number(0);
        return;
    }
    
    handler->disable_cors();
    push_number(1);
}

/*
 * socket_rest_parse_json()
 * 
 * Parse JSON string to LPC mapping.
 * 
 * Syntax: mapping socket_rest_parse_json(string json_string)
 * 
 * Parameters:
 *   json_string - JSON string to parse
 * 
 * Returns: LPC mapping or 0 on error
 */
void f_socket_rest_parse_json() {
    if (sp->type != T_STRING) {
        bad_argument(sp, T_STRING, 1, F_SOCKET_REST_PARSE_JSON);
    }
    
    std::string json_str(sp->u.string);
    pop_stack();
    
    // Use any available REST handler for JSON parsing
    // (This functionality doesn't need a specific socket)
    if (!rest_handlers_.empty()) {
        RESTHandler* handler = rest_handlers_.begin()->second.get();
        mapping_t* result = handler->parse_json_to_mapping(json_str);
        
        if (result) {
            push_refed_mapping(result);
        } else {
            push_number(0);
        }
    } else {
        push_number(0);
    }
}

/*
 * socket_rest_serialize_json()
 * 
 * Serialize LPC mapping to JSON string.
 * 
 * Syntax: string socket_rest_serialize_json(mapping data)
 * 
 * Parameters:
 *   data - LPC mapping to serialize
 * 
 * Returns: JSON string or 0 on error
 */
void f_socket_rest_serialize_json() {
    if (sp->type != T_MAPPING) {
        bad_argument(sp, T_MAPPING, 1, F_SOCKET_REST_SERIALIZE_JSON);
    }
    
    mapping_t* data = sp->u.map;
    pop_stack();
    
    // Use any available REST handler for JSON serialization
    if (!rest_handlers_.empty()) {
        RESTHandler* handler = rest_handlers_.begin()->second.get();
        std::string result = handler->serialize_mapping_to_json(data);
        
        if (!result.empty()) {
            push_malloced_string(string_copy(result.c_str()));
        } else {
            push_number(0);
        }
    } else {
        push_number(0);
    }
}

/*
 * Function table for package registration
 */
static function_t rest_efuns[] = {
    {"socket_enable_rest_mode", f_socket_enable_rest_mode, TYPE_NUMBER, TYPE_NUMBER | TYPE_MAPPING},
    {"socket_is_rest_mode", f_socket_is_rest_mode, TYPE_NUMBER, TYPE_NUMBER},
    {"socket_rest_add_route", f_socket_rest_add_route, TYPE_NUMBER, TYPE_NUMBER | TYPE_MAPPING},
    {"socket_rest_remove_route", f_socket_rest_remove_route, TYPE_NUMBER, TYPE_NUMBER | TYPE_NUMBER | TYPE_STRING | TYPE_STRING},
    {"socket_rest_get_routes", f_socket_rest_get_routes, TYPE_ARRAY, TYPE_NUMBER},
    {"socket_rest_set_api_info", f_socket_rest_set_api_info, TYPE_NUMBER, TYPE_NUMBER | TYPE_MAPPING},
    {"socket_rest_get_api_info", f_socket_rest_get_api_info, TYPE_MAPPING, TYPE_NUMBER},
    {"socket_rest_generate_docs", f_socket_rest_generate_docs, TYPE_STRING, TYPE_NUMBER | TYPE_STRING},
    {"socket_rest_enable_cors", f_socket_rest_enable_cors, TYPE_NUMBER, TYPE_NUMBER | TYPE_MAPPING},
    {"socket_rest_disable_cors", f_socket_rest_disable_cors, TYPE_NUMBER, TYPE_NUMBER},
    {"socket_rest_parse_json", f_socket_rest_parse_json, TYPE_MAPPING, TYPE_STRING},
    {"socket_rest_serialize_json", f_socket_rest_serialize_json, TYPE_STRING, TYPE_MAPPING},
    {nullptr, nullptr, 0, 0}  // End marker
};

// Package initialization function
void init_rest_efuns() {
    // Register REST efuns with the FluffOS function table
    // This would be called during package initialization
}

// Note: The actual function constants (F_SOCKET_ENABLE_REST_MODE, etc.) 
// would need to be defined in the main function table or generated 
// automatically by the build system.