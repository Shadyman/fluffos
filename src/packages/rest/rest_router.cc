/**
 * rest_router.cc - REST routing implementation
 *
 * URL routing and request processing
 */

#include "rest.h"
#include <nlohmann/json.hpp>

/**
 * Create REST router implementation
 */
int rest_router_create_impl() {
    auto router_ctx = std::make_shared<rest_router_context>();
    router_ctx->router_id = g_next_router_id++;
    router_ctx->middleware = allocate_mapping(0);
    router_ctx->config = allocate_mapping(0);
    
    g_rest_routers[router_ctx->router_id] = router_ctx;
    
    return router_ctx->router_id;
}

/**
 * Add route to router implementation
 */
int rest_route_add_impl(int router_id, const char *method, const char *pattern, svalue_t *handler) {
    auto it = g_rest_routers.find(router_id);
    if (it == g_rest_routers.end()) {
        return 0; // Router not found
    }
    
    auto router_ctx = it->second;
    
    // Create new route
    auto route = std::make_shared<rest_route>();
    route->method = method;
    route->pattern = pattern;
    
    // Compile pattern to regex
    try {
        std::string regex_str = rest_pattern_to_regex(pattern, route->param_names);
        route->compiled_pattern = std::regex(regex_str);
    } catch (const std::exception &e) {
        return 0; // Invalid pattern
    }
    
    // Copy handler
    assign_svalue(&route->handler, handler);
    
    // Initialize metadata
    route->metadata = allocate_mapping(0);
    
    // Add to router
    router_ctx->routes.push_back(route);
    
    return 1; // Success
}

/**
 * Process request through router implementation
 */
mapping_t *rest_route_process_impl(int router_id, mapping_t *request) {
    auto it = g_rest_routers.find(router_id);
    if (it == g_rest_routers.end()) {
        return allocate_mapping(0); // Router not found
    }
    
    auto router_ctx = it->second;
    
    // Extract method and path from request
    svalue_t *method_val = find_string_in_mapping(request, "method");
    svalue_t *uri_val = find_string_in_mapping(request, "uri");
    
    if (!method_val || method_val->type != T_STRING ||
        !uri_val || uri_val->type != T_STRING) {
        return allocate_mapping(0); // Invalid request
    }
    
    std::string method = method_val->u.string;
    std::string uri = uri_val->u.string;
    
    // Split URI into path and query
    std::string path = uri;
    std::string query;
    
    size_t query_pos = uri.find('?');
    if (query_pos != std::string::npos) {
        path = uri.substr(0, query_pos);
        query = uri.substr(query_pos + 1);
    }
    
    // Find matching route
    for (const auto &route : router_ctx->routes) {
        if (route->method != method && route->method != "*") {
            continue; // Method doesn't match
        }
        
        std::smatch matches;
        if (std::regex_match(path, matches, route->compiled_pattern)) {
            // Route matches! Create result mapping
            mapping_t *result = allocate_mapping(4);
            svalue_t key, value;
            
            // Add handler
            key.type = T_STRING; key.u.string = make_shared_string("handler");
            assign_svalue(&value, &route->handler);
            svalue_t *entry1 = find_for_insert(result, &key, 0);
            *entry1 = value;
            free_string(key.u.string);
            
            // Add path parameters
            mapping_t *path_params = rest_extract_path_params(route->pattern, path);
            key.type = T_STRING; key.u.string = make_shared_string("path_params");
            value.type = T_MAPPING; value.u.map = path_params;
            svalue_t *entry2 = find_for_insert(result, &key, 0);
            *entry2 = value;
            free_string(key.u.string);
            
            // Add query parameters
            mapping_t *query_params = rest_parse_query_string(query);
            key.type = T_STRING; key.u.string = make_shared_string("query_params");
            value.type = T_MAPPING; value.u.map = query_params;
            svalue_t *entry3 = find_for_insert(result, &key, 0);
            *entry3 = value;
            free_string(key.u.string);
            
            // Add route metadata
            key.type = T_STRING; key.u.string = make_shared_string("metadata");
            value.type = T_MAPPING; value.u.map = route->metadata;
            svalue_t *entry4 = find_for_insert(result, &key, 0);
            *entry4 = value;
            free_string(key.u.string);
            // Don't free metadata - it's owned by the route
            
            return result;
        }
    }
    
    // No route found
    return allocate_mapping(0);
}

/**
 * Parse HTTP request into REST format
 */
mapping_t *rest_parse_request_impl(mapping_t *http_request) {
    mapping_t *rest_request = allocate_mapping(8);
    svalue_t key, value;
    
    // Copy basic HTTP fields
    const char *copy_fields[] = {
        "id", "method", "uri", "headers", "body", nullptr
    };
    
    for (int i = 0; copy_fields[i]; i++) {
        svalue_t *field_val = find_string_in_mapping(http_request, copy_fields[i]);
        if (field_val) {
            key.type = T_STRING;
            key.u.string = make_shared_string(copy_fields[i]);
            assign_svalue(&value, field_val);
            svalue_t *entry = find_for_insert(rest_request, &key, 0); *entry = value;
            free_string(key.u.string);
            free_svalue(&value, "rest_parse_request_impl");
        }
    }
    
    // Parse JSON body if present
    svalue_t *body_val = find_string_in_mapping(http_request, "body");
    svalue_t *headers_val = find_string_in_mapping(http_request, "headers");
    
    if (body_val && body_val->type == T_STRING && headers_val && headers_val->type == T_MAPPING) {
        svalue_t *content_type_val = find_string_in_mapping(headers_val->u.map, "Content-Type");
        if (content_type_val && content_type_val->type == T_STRING) {
            std::string content_type = content_type_val->u.string;
            
            if (content_type.find("application/json") != std::string::npos) {
                try {
                    nlohmann::json j = nlohmann::json::parse(body_val->u.string);
                    
                    // Convert JSON to LPC mapping
                    mapping_t *json_data = allocate_mapping(j.size());
                    
                    for (auto it = j.begin(); it != j.end(); ++it) {
                        svalue_t json_key, json_value;
                        json_key.type = T_STRING;
                        json_key.u.string = make_shared_string(it.key().c_str());
                        
                        if (it.value().is_string()) {
                            json_value.type = T_STRING;
                            json_value.u.string = make_shared_string(it.value().get<std::string>().c_str());
                        } else if (it.value().is_number_integer()) {
                            json_value.type = T_NUMBER;
                            json_value.u.number = it.value().get<int>();
                        } else if (it.value().is_number_float()) {
                            json_value.type = T_REAL;
                            json_value.u.real = it.value().get<double>();
                        } else {
                            json_value.type = T_NUMBER;
                            json_value.u.number = 0; // Default for unsupported types
                        }
                        
                        svalue_t *entry = find_for_insert(json_data, &json_key, 0); *entry = json_value;
                        free_string(json_key.u.string);
                        if (json_value.type == T_STRING) {
                            free_string(json_value.u.string);
                        }
                    }
                    
                    // Add parsed JSON to request
                    key.type = T_STRING; key.u.string = make_shared_string("json");
                    value.type = T_MAPPING; value.u.map = json_data;
                    svalue_t *entry = find_for_insert(rest_request, &key, 0); *entry = value;
                    free_string(key.u.string);
                    free_mapping(json_data);
                    
                } catch (const std::exception &e) {
                    // JSON parsing failed - ignore
                }
            }
        }
    }
    
    return rest_request;
}

/**
 * Format REST response
 */
mapping_t *rest_format_response_impl(svalue_t *data, int status, mapping_t *headers) {
    mapping_t *response = allocate_mapping(3);
    svalue_t key, value;
    
    // Add status
    key.type = T_STRING; key.u.string = make_shared_string("status");
    value.type = T_NUMBER; value.u.number = status;
    svalue_t *entry = find_for_insert(response, &key, 0); *entry = value;
    free_string(key.u.string);
    
    // Add headers (copy or create default)
    mapping_t *response_headers;
    if (headers) {
        response_headers = allocate_mapping(headers->count + 1);
        // Copy existing headers
        for (int i = 0; i < headers->table_size; i++) {
            mapping_node_t *node = headers->table[i];
            while (node) {
                svalue_t *entry = find_for_insert(response_headers, &node->values[0], 0); *entry = node->values[1];
                node = node->next;
            }
        }
    } else {
        response_headers = allocate_mapping(1);
    }
    
    // Set default content type if not present
    svalue_t *content_type_val = find_string_in_mapping(response_headers, "Content-Type");
    if (!content_type_val) {
        svalue_t ct_key, ct_value;
        ct_key.type = T_STRING; ct_key.u.string = make_shared_string("Content-Type");
        ct_value.type = T_STRING; ct_value.u.string = make_shared_string("application/json");
        svalue_t *entry = find_for_insert(response_headers, &ct_key, 0); *entry = ct_value;
        free_string(ct_key.u.string);
        free_string(ct_value.u.string);
    }
    
    key.type = T_STRING; key.u.string = make_shared_string("headers");
    value.type = T_MAPPING; value.u.map = response_headers;
    svalue_t *entry_headers = find_for_insert(response, &key, 0); *entry_headers = value;
    free_string(key.u.string);
    free_mapping(response_headers);
    
    // Add body (convert data to JSON string)
    std::string body_str;
    
    try {
        nlohmann::json j;
        
        // Convert svalue to JSON
        switch (data->type) {
            case T_STRING:
                j = data->u.string;
                break;
            case T_NUMBER:
                j = data->u.number;
                break;
            case T_REAL:
                j = data->u.real;
                break;
            case T_MAPPING: {
                j = nlohmann::json::object();
                for (int i = 0; i < data->u.map->table_size; i++) {
                    mapping_node_t *node = data->u.map->table[i];
                    while (node) {
                        if (node->values[0].type == T_STRING) {
                            std::string key_str = node->values[0].u.string;
                            if (node->values[1].type == T_STRING) {
                                j[key_str] = node->values[1].u.string;
                            } else if (node->values[1].type == T_NUMBER) {
                                j[key_str] = node->values[1].u.number;
                            } else if (node->values[1].type == T_REAL) {
                                j[key_str] = node->values[1].u.real;
                            }
                        }
                        node = node->next;
                    }
                }
                break;
            }
            default:
                j = nullptr;
                break;
        }
        
        body_str = j.dump();
        
    } catch (const std::exception &e) {
        body_str = "{}"; // Default empty JSON
    }
    
    key.type = T_STRING; key.u.string = make_shared_string("body");
    value.type = T_STRING; value.u.string = make_shared_string(body_str.c_str());
    svalue_t *entry_body = find_for_insert(response, &key, 0); *entry_body = value;
    free_string(key.u.string);
    free_string(value.u.string);
    
    return response;
}

/**
 * Cleanup router resources
 */
void rest_cleanup_router(int router_id) {
    auto it = g_rest_routers.find(router_id);
    if (it != g_rest_routers.end()) {
        auto router_ctx = it->second;
        
        // Clean up routes
        for (auto &route : router_ctx->routes) {
            free_svalue(&route->handler, "rest_cleanup_router");
            free_mapping(route->metadata);
        }
        
        // Clean up router context
        free_mapping(router_ctx->middleware);
        free_mapping(router_ctx->config);
        
        // Remove from global map
        g_rest_routers.erase(it);
    }
}