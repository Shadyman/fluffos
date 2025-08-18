/**
 * rest.cc - REST package implementation for FluffOS
 *
 * Implements high-level REST API functionality on top of the HTTP package
 *
 * Features:
 * - URL routing with parameter extraction
 * - JWT authentication
 * - Request validation
 * - Response formatting
 * - JSON request/response handling
 *
 * -- REST Package for FluffOS --
 */

#include "rest.h"
#include <nlohmann/json.hpp>
#include <sstream>
#include <algorithm>

// Global REST state
std::map<int, std::shared_ptr<rest_router_context>> g_rest_routers;
int g_next_router_id = 1;

/**
 * Convert REST route pattern to regex with parameter extraction
 * Example: "/users/{id}/posts/{post_id}" -> "^/users/([^/]+)/posts/([^/]+)$"
 */
std::string rest_pattern_to_regex(const std::string &pattern, std::vector<std::string> &param_names) {
    std::string regex_pattern = "^";
    std::string current_param;
    bool in_param = false;
    
    for (char c : pattern) {
        if (c == '{') {
            in_param = true;
            current_param.clear();
            regex_pattern += "([^/]+)"; // Match parameter value
        } else if (c == '}') {
            in_param = false;
            param_names.push_back(current_param);
        } else if (in_param) {
            current_param += c;
        } else {
            // Escape regex special characters
            if (c == '.' || c == '*' || c == '+' || c == '?' || c == '^' || 
                c == '$' || c == '(' || c == ')' || c == '[' || c == ']' ||
                c == '|' || c == '\\') {
                regex_pattern += '\\';
            }
            regex_pattern += c;
        }
    }
    
    regex_pattern += "$";
    return regex_pattern;
}

/**
 * Extract path parameters from URL using route pattern
 */
mapping_t *rest_extract_path_params(const std::string &pattern, const std::string &path) {
    std::vector<std::string> param_names;
    std::string regex_str = rest_pattern_to_regex(pattern, param_names);
    
    try {
        std::regex pattern_regex(regex_str);
        std::smatch matches;
        
        if (!std::regex_match(path, matches, pattern_regex)) {
            return allocate_mapping(0); // No match
        }
        
        mapping_t *params = allocate_mapping(param_names.size());
        
        for (size_t i = 0; i < param_names.size() && i + 1 < matches.size(); i++) {
            svalue_t key, value;
            key.type = T_STRING;
            key.u.string = make_shared_string(param_names[i].c_str());
            value.type = T_STRING;
            value.u.string = make_shared_string(matches[i + 1].str().c_str());
            
            svalue_t *entry = find_for_insert(params, &key, 0);
            *entry = value;
            
            free_string(key.u.string);
        }
        
        return params;
        
    } catch (const std::exception &e) {
        return allocate_mapping(0); // Regex error
    }
}

/**
 * Parse query string into mapping
 */
mapping_t *rest_parse_query_string(const std::string &query) {
    mapping_t *params = allocate_mapping(10); // Initial size
    
    if (query.empty()) {
        return params;
    }
    
    std::istringstream stream(query);
    std::string pair;
    
    while (std::getline(stream, pair, '&')) {
        size_t eq_pos = pair.find('=');
        if (eq_pos != std::string::npos) {
            std::string key = pair.substr(0, eq_pos);
            std::string value = pair.substr(eq_pos + 1);
            
            // Simple URL decode (just handle %20 for spaces)
            size_t pos = 0;
            while ((pos = value.find("%20", pos)) != std::string::npos) {
                value.replace(pos, 3, " ");
                pos += 1;
            }
            
            svalue_t key_sv, value_sv;
            key_sv.type = T_STRING;
            key_sv.u.string = make_shared_string(key.c_str());
            value_sv.type = T_STRING;
            value_sv.u.string = make_shared_string(value.c_str());
            
            svalue_t *entry = find_for_insert(params, &key_sv, 0);
            *entry = value_sv;
            
            free_string(key_sv.u.string);
        }
    }
    
    return params;
}

// EFUN implementations

/**
 * rest_router_create()
 * Create new REST router
 */
void f_rest_create_router(void) {
    int result = rest_router_create_impl();
    
    push_number(result);
}

/**
 * rest_route_add(int router_id, string method, string pattern, string|function handler)
 * Add route to router
 */
void f_rest_add_route(void) {
    svalue_t *handler = sp--;
    const char *pattern = sp->u.string;
    sp--;
    const char *method = sp->u.string;
    sp--;
    int router_id = sp->u.number;
    
    int result = rest_route_add_impl(router_id, method, pattern, handler);
    
    // Clean up
    free_string((sp + 1)->u.string); // pattern
    free_string((sp + 2)->u.string); // method
    free_svalue(handler, "f_rest_route_add");
    
    sp->type = T_NUMBER;
    sp->u.number = result;
}

/**
 * rest_route_process(int router_id, mapping request)
 * Process request through router
 */
void f_rest_process_route(void) {
    mapping_t *request = sp->u.map;
    sp--;
    int router_id = sp->u.number;
    
    mapping_t *result = rest_route_process_impl(router_id, request);
    free_mapping(request);
    
    sp->type = T_MAPPING;
    sp->u.map = result;
}

/**
 * rest_jwt_create(mapping payload, string secret)
 * Create JWT token
 */
void f_rest_create_jwt(void) {
    const char *secret = sp->u.string;
    sp--;
    mapping_t *payload = sp->u.map;
    
    char *token = rest_jwt_create_impl(payload, secret);
    
    // Clean up
    free_mapping(payload);
    free_string((sp + 1)->u.string);
    
    if (token) {
        sp->type = T_STRING;
        sp->u.string = make_shared_string(token);
        free(token);
    } else {
        sp->type = T_NUMBER;
        sp->u.number = 0;
    }
}

/**
 * rest_jwt_verify(string token, string secret)
 * Verify JWT token
 */
void f_rest_verify_jwt(void) {
    const char *secret = sp->u.string;
    sp--;
    const char *token = sp->u.string;
    
    mapping_t *result = rest_jwt_verify_impl(token, secret);
    
    // Clean up
    free_string(sp->u.string);
    free_string((sp + 1)->u.string);
    
    sp->type = T_MAPPING;
    sp->u.map = result ? result : allocate_mapping(0);
}

/**
 * rest_validate(mixed data, mapping schema)
 * Validate data against schema
 */
void f_rest_validate_schema(void) {
    mapping_t *schema = sp->u.map;
    sp--;
    svalue_t *data = sp;
    
    mapping_t *result = rest_validate_impl(data, schema);
    
    // Clean up
    free_svalue(data, "f_rest_validate");
    free_mapping(schema);
    
    sp->type = T_MAPPING;
    sp->u.map = result;
}

/**
 * rest_parse_request(mapping http_request)
 * Parse HTTP request into REST format
 */
void f_rest_parse_request(void) {
    mapping_t *http_request = sp->u.map;
    
    mapping_t *result = rest_parse_request_impl(http_request);
    free_mapping(http_request);
    
    sp->type = T_MAPPING;
    sp->u.map = result;
}

/**
 * rest_format_response(mixed data, int status, mapping headers)
 * Format REST response
 */
void f_rest_format_response(void) {
    mapping_t *headers = nullptr;
    int status = 200;
    svalue_t *data;
    
    // Parse arguments
    if (st_num_arg >= 3) {
        headers = sp->u.map;
        sp--;
    }
    if (st_num_arg >= 2) {
        status = sp->u.number;
        sp--;
    }
    data = sp;
    
    mapping_t *result = rest_format_response_impl(data, status, headers);
    
    // Clean up
    free_svalue(data, "f_rest_format_response");
    if (headers) free_mapping(headers);
    
    sp->type = T_MAPPING;
    sp->u.map = result;
}