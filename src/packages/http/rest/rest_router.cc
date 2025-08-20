#include "rest_router.h"
#include <algorithm>
#include <sstream>

/*
 * RestRouter Implementation
 * 
 * Core REST routing functionality moved from original rest_handler.cc
 * as part of architecture correction to proper http/rest/ package structure.
 */

RestRouter::RestRouter() : next_route_id_(1) {
    clear_error();
}

RestRouter::~RestRouter() {
    // Cleanup handled by unique_ptr destructors
}

bool RestRouter::add_route(const std::string& method, const std::string& pattern,
                          const std::string& handler_object, const std::string& handler_function,
                          const std::string& description) {
    
    if (!validate_method(method)) {
        last_error_ = "Invalid HTTP method: " + method;
        return false;
    }
    
    if (!validate_route_pattern(pattern)) {
        last_error_ = "Invalid route pattern: " + pattern;
        return false;
    }
    
    auto route = std::make_unique<RestRoute>();
    route->route_id = next_route_id_++;
    route->method = method;
    route->pattern = normalize_route_pattern(pattern);
    route->handler_object = handler_object;
    route->handler_function = handler_function;
    route->description = description;
    route->param_names = extract_parameter_names(route->pattern);
    
    if (!compile_route_pattern(route.get())) {
        return false;
    }
    
    routes_.push_back(std::move(route));
    return true;
}

bool RestRouter::remove_route(int route_id) {
    auto it = std::remove_if(routes_.begin(), routes_.end(),
        [route_id](const std::unique_ptr<RestRoute>& route) {
            return route->route_id == route_id;
        });
    
    if (it != routes_.end()) {
        routes_.erase(it, routes_.end());
        return true;
    }
    
    last_error_ = "Route not found: " + std::to_string(route_id);
    return false;
}

bool RestRouter::remove_route(const std::string& method, const std::string& pattern) {
    std::string normalized_pattern = normalize_route_pattern(pattern);
    
    auto it = std::remove_if(routes_.begin(), routes_.end(),
        [&method, &normalized_pattern](const std::unique_ptr<RestRoute>& route) {
            return route->method == method && route->pattern == normalized_pattern;
        });
    
    if (it != routes_.end()) {
        routes_.erase(it, routes_.end());
        return true;
    }
    
    last_error_ = "Route not found: " + method + " " + pattern;
    return false;
}

void RestRouter::clear_all_routes() {
    routes_.clear();
    next_route_id_ = 1;
}

RouteMatch RestRouter::find_matching_route(const std::string& method, const std::string& path) {
    RouteMatch match;
    
    for (const auto& route : routes_) {
        if (route->method != method) {
            continue;
        }
        
        std::smatch regex_match;
        if (std::regex_match(path, regex_match, route->compiled_pattern)) {
            match.found = true;
            match.route = route.get();
            
            // Extract parameters
            for (size_t i = 1; i < regex_match.size() && i - 1 < route->param_names.size(); ++i) {
                match.params[route->param_names[i - 1]] = regex_match[i].str();
            }
            
            break;
        }
    }
    
    return match;
}

bool RestRouter::compile_route_pattern(RestRoute* route) {
    try {
        std::string regex_pattern = convert_pattern_to_regex(route->pattern);
        route->compiled_pattern = std::regex(regex_pattern);
        return true;
    } catch (const std::exception& e) {
        last_error_ = "Failed to compile route pattern: " + std::string(e.what());
        return false;
    }
}

std::string RestRouter::convert_pattern_to_regex(const std::string& pattern) {
    std::string regex_pattern = pattern;
    
    // Enhanced pattern to regex conversion with advanced features
    
    // First, escape regex special characters (except our parameter syntax)
    std::string escaped;
    for (size_t i = 0; i < regex_pattern.size(); ++i) {
        char c = regex_pattern[i];
        if (c == '.' || c == '*' || c == '+' || c == '?' || c == '(' || c == ')' || 
            c == '[' || c == ']' || c == '^' || c == '$' || c == '|' || c == '\\') {
            // Don't escape if it's part of our parameter syntax
            if (!(c == '*' && (i == 0 || regex_pattern[i-1] == '/' || i == regex_pattern.size()-1))) {
                escaped += '\\';
            }
        }
        escaped += c;
    }
    regex_pattern = escaped;
    
    // Advanced pattern replacements
    size_t pos = 0;
    
    // Handle wildcard patterns: /* -> /.*
    pos = 0;
    while ((pos = regex_pattern.find("/*", pos)) != std::string::npos) {
        regex_pattern.replace(pos, 2, "/.*");
        pos += 3;
    }
    
    // Handle optional parameters: {param?} -> ([^/]*)?
    pos = 0;
    while ((pos = regex_pattern.find('{', pos)) != std::string::npos) {
        size_t end_pos = regex_pattern.find('}', pos);
        if (end_pos != std::string::npos) {
            std::string param_content = regex_pattern.substr(pos + 1, end_pos - pos - 1);
            
            if (!param_content.empty() && param_content.back() == '?') {
                // Optional parameter
                regex_pattern.replace(pos, end_pos - pos + 1, "([^/]*)?");
                pos += 8; // Length of "([^/]*)?"
            } else if (param_content.find(':') != std::string::npos) {
                // Typed parameter: {id:int} -> ([0-9]+)
                size_t colon_pos = param_content.find(':');
                std::string param_type = param_content.substr(colon_pos + 1);
                
                if (param_type == "int" || param_type == "integer") {
                    regex_pattern.replace(pos, end_pos - pos + 1, "([0-9]+)");
                    pos += 8; // Length of "([0-9]+)"
                } else if (param_type == "uuid") {
                    regex_pattern.replace(pos, end_pos - pos + 1, 
                        "([0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12})");
                    pos += 76; // Length of UUID pattern
                } else if (param_type == "alpha") {
                    regex_pattern.replace(pos, end_pos - pos + 1, "([a-zA-Z]+)");
                    pos += 11; // Length of "([a-zA-Z]+)"
                } else {
                    // Default to string pattern
                    regex_pattern.replace(pos, end_pos - pos + 1, "([^/]+)");
                    pos += 7; // Length of "([^/]+)"
                }
            } else {
                // Regular parameter: {param} -> ([^/]+)
                regex_pattern.replace(pos, end_pos - pos + 1, "([^/]+)");
                pos += 7; // Length of "([^/]+)"
            }
        } else {
            break;
        }
    }
    
    // Handle trailing slash normalization
    if (regex_pattern.length() > 1 && regex_pattern.back() == '/') {
        regex_pattern += "?"; // Make trailing slash optional
    }
    
    // Anchor pattern
    if (regex_pattern.front() != '^') {
        regex_pattern = "^" + regex_pattern;
    }
    if (regex_pattern.back() != '$') {
        regex_pattern += "$";
    }
    
    return regex_pattern;
}

std::vector<std::string> RestRouter::extract_parameter_names(const std::string& pattern) {
    std::vector<std::string> param_names;
    
    size_t pos = 0;
    while ((pos = pattern.find('{', pos)) != std::string::npos) {
        size_t end_pos = pattern.find('}', pos);
        if (end_pos != std::string::npos) {
            std::string param_name = pattern.substr(pos + 1, end_pos - pos - 1);
            // Remove optional marker if present
            if (!param_name.empty() && param_name.back() == '?') {
                param_name.pop_back();
            }
            param_names.push_back(param_name);
            pos = end_pos + 1;
        } else {
            break;
        }
    }
    
    return param_names;
}

bool RestRouter::validate_route_pattern(const std::string& pattern) {
    if (pattern.empty() || pattern.front() != '/') {
        return false;
    }
    
    // Check for balanced braces
    int brace_count = 0;
    for (char c : pattern) {
        if (c == '{') {
            brace_count++;
        } else if (c == '}') {
            brace_count--;
            if (brace_count < 0) {
                return false;
            }
        }
    }
    
    return brace_count == 0;
}

bool RestRouter::validate_method(const std::string& method) {
    return IS_VALID_REST_METHOD(method);
}

std::string RestRouter::normalize_route_pattern(const std::string& pattern) {
    std::string normalized = pattern;
    
    // Remove trailing slash unless it's the root
    if (normalized.length() > 1 && normalized.back() == '/') {
        normalized.pop_back();
    }
    
    return normalized;
}

bool RestRouter::add_middleware(const std::string& name, const std::string& function) {
    if (name.empty() || function.empty()) {
        last_error_ = "Middleware name and function cannot be empty";
        return false;
    }
    
    middleware_functions_[name] = function;
    return true;
}

bool RestRouter::remove_middleware(const std::string& name) {
    auto it = middleware_functions_.find(name);
    if (it != middleware_functions_.end()) {
        middleware_functions_.erase(it);
        return true;
    }
    
    last_error_ = "Middleware not found: " + name;
    return false;
}

std::vector<std::string> RestRouter::get_middleware_chain() const {
    std::vector<std::string> chain;
    for (const auto& middleware : middleware_functions_) {
        chain.push_back(middleware.first);
    }
    return chain;
}

void RestRouter::dump_routes(outbuffer_t* buffer) const {
    outbuf_addv(buffer, "REST Router - %zu routes:\n", routes_.size());
    
    for (const auto& route : routes_) {
        outbuf_addv(buffer, "  [%d] %s %s -> %s::%s\n",
                   route->route_id,
                   route->method.c_str(),
                   route->pattern.c_str(),
                   route->handler_object.c_str(),
                   route->handler_function.c_str());
        
        if (!route->param_names.empty()) {
            outbuf_add(buffer, "      Parameters: ");
            for (size_t i = 0; i < route->param_names.size(); ++i) {
                if (i > 0) outbuf_add(buffer, ", ");
                outbuf_add(buffer, route->param_names[i].c_str());
            }
            outbuf_add(buffer, "\n");
        }
    }
    
    if (!middleware_functions_.empty()) {
        outbuf_addv(buffer, "Middleware chain (%zu items):\n", middleware_functions_.size());
        for (const auto& middleware : middleware_functions_) {
            outbuf_addv(buffer, "  %s -> %s\n", 
                       middleware.first.c_str(), 
                       middleware.second.c_str());
        }
    }
}

// Static utility implementations
bool RestRouter::is_valid_route_pattern(const std::string& pattern) {
    RestRouter temp_router;
    return temp_router.validate_route_pattern(pattern);
}

std::vector<std::string> RestRouter::extract_route_parameter_names(const std::string& pattern) {
    RestRouter temp_router;
    return temp_router.extract_parameter_names(pattern);
}

// Global router registry for socket integration
static std::unordered_map<int, std::unique_ptr<RestRouter>> rest_routers_;

bool socket_enable_rest_mode(int socket_id, const mapping_t* options) {
    auto router = std::make_unique<RestRouter>();
    rest_routers_[socket_id] = std::move(router);
    return true;
}

RestRouter* get_rest_router(int socket_id) {
    auto it = rest_routers_.find(socket_id);
    return (it != rest_routers_.end()) ? it->second.get() : nullptr;
}

bool socket_rest_add_route(int socket_id, const mapping_t* route_config) {
    RestRouter* router = get_rest_router(socket_id);
    if (!router) {
        return false;
    }
    
    return router->register_route_from_mapping(route_config);
}

bool socket_rest_remove_route(int socket_id, int route_id) {
    RestRouter* router = get_rest_router(socket_id);
    if (!router) {
        return false;
    }
    
    return router->remove_route(route_id);
}

array_t* socket_rest_get_routes(int socket_id) {
    RestRouter* router = get_rest_router(socket_id);
    if (!router) {
        return nullptr;
    }
    
    return router->get_all_routes();
}

bool RestRouter::register_route_from_mapping(const mapping_t* route_config) {
    if (!route_config) {
        last_error_ = "Route configuration mapping is null";
        return false;
    }
    
    // Extract required fields
    svalue_t* method_sv = find_string_in_mapping(const_cast<mapping_t*>(route_config), "method");
    svalue_t* pattern_sv = find_string_in_mapping(const_cast<mapping_t*>(route_config), "pattern");
    svalue_t* handler_obj_sv = find_string_in_mapping(const_cast<mapping_t*>(route_config), "handler_object");
    svalue_t* handler_func_sv = find_string_in_mapping(const_cast<mapping_t*>(route_config), "handler_function");
    
    if (!method_sv || method_sv->type != T_STRING ||
        !pattern_sv || pattern_sv->type != T_STRING ||
        !handler_obj_sv || handler_obj_sv->type != T_STRING ||
        !handler_func_sv || handler_func_sv->type != T_STRING) {
        last_error_ = "Missing required route configuration fields";
        return false;
    }
    
    std::string method = method_sv->u.string;
    std::string pattern = pattern_sv->u.string;
    std::string handler_object = handler_obj_sv->u.string;
    std::string handler_function = handler_func_sv->u.string;
    
    // Extract optional description
    std::string description = "";
    svalue_t* desc_sv = find_string_in_mapping(const_cast<mapping_t*>(route_config), "description");
    if (desc_sv && desc_sv->type == T_STRING) {
        description = desc_sv->u.string;
    }
    
    return add_route(method, pattern, handler_object, handler_function, description);
}

array_t* RestRouter::get_all_routes() const {
    array_t* result = allocate_empty_array(routes_.size());
    
    for (size_t i = 0; i < routes_.size(); ++i) {
        const RestRoute& route = *routes_[i];
        mapping_t* route_info = allocate_mapping(6);
        
        // Add route information to mapping
        add_mapping_string(route_info, "method", route.method.c_str());
        add_mapping_string(route_info, "pattern", route.pattern.c_str());
        add_mapping_string(route_info, "handler_object", route.handler_object.c_str());
        add_mapping_string(route_info, "handler_function", route.handler_function.c_str());
        add_mapping_string(route_info, "description", route.description.c_str());
        add_mapping_pair(route_info, "route_id", route.route_id);
        
        result->item[i].type = T_MAPPING;
        result->item[i].u.map = route_info;
    }
    
    return result;
}

mapping_t* RestRouter::get_route_info(int route_id) const {
    for (const auto& route : routes_) {
        if (route->route_id == route_id) {
            mapping_t* route_info = allocate_mapping(8);
            
            add_mapping_string(route_info, "method", route->method.c_str());
            add_mapping_string(route_info, "pattern", route->pattern.c_str());
            add_mapping_string(route_info, "handler_object", route->handler_object.c_str());
            add_mapping_string(route_info, "handler_function", route->handler_function.c_str());
            add_mapping_string(route_info, "description", route->description.c_str());
            add_mapping_pair(route_info, "route_id", route->route_id);
            add_mapping_pair(route_info, "requires_auth", route->requires_auth ? 1 : 0);
            
            // Add parameter names array
            if (!route->param_names.empty()) {
                array_t* param_array = allocate_empty_array(route->param_names.size());
                for (size_t i = 0; i < route->param_names.size(); ++i) {
                    param_array->item[i].type = T_STRING;
                    param_array->item[i].subtype = STRING_MALLOC;
                    param_array->item[i].u.string = string_copy(route->param_names[i].c_str(), "get_route_info");
                }
                add_mapping_array(route_info, "param_names", param_array);
            }
            
            return route_info;
        }
    }
    
    return nullptr;
}