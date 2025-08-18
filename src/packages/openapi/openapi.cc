/**
 * openapi.cc - OpenAPI package implementation for FluffOS
 *
 * Implements OpenAPI 3.x documentation generation and serving
 *
 * Features:
 * - Automatic OpenAPI spec generation from REST routes
 * - Interactive documentation with Swagger UI and ReDoc
 * - Route documentation metadata management
 * - JSON schema support
 *
 * -- OpenAPI Package for FluffOS --
 */

#include "openapi.h"
#include <nlohmann/json.hpp>
#include <sstream>

// Global OpenAPI state
std::map<int, std::shared_ptr<openapi_docs_context>> g_openapi_docs;

/**
 * Generate route key for documentation lookup
 */
std::string openapi_route_key(const std::string &method, const std::string &pattern) {
    return method + ":" + pattern;
}

/**
 * Generate OpenAPI 3.x specification
 */
mapping_t *openapi_generate_spec(std::shared_ptr<openapi_docs_context> docs_ctx) {
    mapping_t *spec = allocate_mapping(10);
    svalue_t key, value;
    
    // OpenAPI version
    key.type = T_STRING; key.u.string = make_shared_string("openapi");
    value.type = T_STRING; value.u.string = make_shared_string("3.0.3");
    svalue_t *entry1 = find_for_insert(spec, &key, 0); *entry1 = value;
    free_string(key.u.string);
    free_string(value.u.string);
    
    // API info
    if (docs_ctx->api_info) {
        key.type = T_STRING; key.u.string = make_shared_string("info");
        value.type = T_MAPPING; value.u.map = docs_ctx->api_info;
        svalue_t *entry2 = find_for_insert(spec, &key, 0); *entry2 = value;
        free_string(key.u.string);
        // Don't free api_info - it's owned by docs_ctx
    } else {
        // Default info
        mapping_t *default_info = allocate_mapping(3);
        svalue_t info_key, info_value;
        
        info_key.type = T_STRING; info_key.u.string = make_shared_string("title");
        info_value.type = T_STRING; info_value.u.string = make_shared_string("FluffOS REST API");
        svalue_t *entry3 = find_for_insert(default_info, &info_key, 0); *entry3 = info_value;
        free_string(info_key.u.string);
        free_string(info_value.u.string);
        
        info_key.type = T_STRING; info_key.u.string = make_shared_string("version");
        info_value.type = T_STRING; info_value.u.string = make_shared_string("1.0.0");
        svalue_t *entry4 = find_for_insert(default_info, &info_key, 0); *entry4 = info_value;
        free_string(info_key.u.string);
        free_string(info_value.u.string);
        
        key.type = T_STRING; key.u.string = make_shared_string("info");
        value.type = T_MAPPING; value.u.map = default_info;
        svalue_t *entry5 = find_for_insert(spec, &key, 0); *entry5 = value;
        free_string(key.u.string);
        free_mapping(default_info);
    }
    
    // Servers
    if (docs_ctx->servers) {
        key.type = T_STRING; key.u.string = make_shared_string("servers");
        value.type = T_MAPPING; value.u.map = docs_ctx->servers;
        svalue_t *entry6 = find_for_insert(spec, &key, 0); *entry6 = value;
        free_string(key.u.string);
    } else {
        // Default server
        mapping_t *default_servers = allocate_mapping(1);
        mapping_t *server = allocate_mapping(2);
        svalue_t server_key, server_value;
        
        server_key.type = T_STRING; server_key.u.string = make_shared_string("url");
        server_value.type = T_STRING; server_value.u.string = make_shared_string("/");
        svalue_t *entry7 = find_for_insert(server, &server_key, 0); *entry7 = server_value;
        free_string(server_key.u.string);
        free_string(server_value.u.string);
        
        server_key.type = T_STRING; server_key.u.string = make_shared_string("description");
        server_value.type = T_STRING; server_value.u.string = make_shared_string("Local server");
        svalue_t *entry8 = find_for_insert(server, &server_key, 0); *entry8 = server_value;
        free_string(server_key.u.string);
        free_string(server_value.u.string);
        
        svalue_t servers_key, servers_value;
        servers_key.type = T_STRING; servers_key.u.string = make_shared_string("0");
        servers_value.type = T_MAPPING; servers_value.u.map = server;
        svalue_t *entry9 = find_for_insert(default_servers, &servers_key, 0); *entry9 = servers_value;
        free_string(servers_key.u.string);
        free_mapping(server);
        
        key.type = T_STRING; key.u.string = make_shared_string("servers");
        value.type = T_MAPPING; value.u.map = default_servers;
        svalue_t *entry10 = find_for_insert(spec, &key, 0); *entry10 = value;
        free_string(key.u.string);
        free_mapping(default_servers);
    }
    
    // Generate paths from router
    auto router_it = g_rest_routers.find(docs_ctx->router_id);
    if (router_it != g_rest_routers.end()) {
        mapping_t *paths = openapi_generate_paths(router_it->second, docs_ctx);
        key.type = T_STRING; key.u.string = make_shared_string("paths");
        value.type = T_MAPPING; value.u.map = paths;
        svalue_t *entry11 = find_for_insert(spec, &key, 0); *entry11 = value;
        free_string(key.u.string);
        free_mapping(paths);
    }
    
    // Components (schemas, security schemes, etc.)
    if (docs_ctx->components) {
        key.type = T_STRING; key.u.string = make_shared_string("components");
        value.type = T_MAPPING; value.u.map = docs_ctx->components;
        svalue_t *entry12 = find_for_insert(spec, &key, 0); *entry12 = value;
        free_string(key.u.string);
    }
    
    return spec;
}

/**
 * Generate OpenAPI paths from REST routes
 */
mapping_t *openapi_generate_paths(std::shared_ptr<rest_router_context> router_ctx, 
                                 std::shared_ptr<openapi_docs_context> docs_ctx) {
    mapping_t *paths = allocate_mapping(router_ctx->routes.size());
    
    // Group routes by path pattern
    std::map<std::string, std::map<std::string, std::shared_ptr<rest_route>>> grouped_routes;
    
    for (const auto &route : router_ctx->routes) {
        grouped_routes[route->pattern][route->method] = route;
    }
    
    // Generate OpenAPI for each path
    for (const auto &path_group : grouped_routes) {
        std::string path_pattern = path_group.first;
        
        // Convert REST pattern to OpenAPI pattern
        // {param} stays as {param} in OpenAPI
        std::string openapi_path = path_pattern;
        
        mapping_t *path_item = allocate_mapping(path_group.second.size());
        
        // Add operations for each method
        for (const auto &method_route : path_group.second) {
            std::string method = method_route.first;
            auto route = method_route.second;
            
            // Convert method to lowercase for OpenAPI
            std::transform(method.begin(), method.end(), method.begin(), ::tolower);
            
            mapping_t *operation = allocate_mapping(5);
            svalue_t op_key, op_value;
            
            // Look for documentation for this route
            std::string route_key = openapi_route_key(method_route.first, path_pattern);
            auto docs_it = docs_ctx->route_docs.find(route_key);
            
            if (docs_it != docs_ctx->route_docs.end()) {
                auto route_docs = docs_it->second;
                
                // Add summary
                if (!route_docs->summary.empty()) {
                    op_key.type = T_STRING; op_key.u.string = make_shared_string("summary");
                    op_value.type = T_STRING; op_value.u.string = make_shared_string(route_docs->summary.c_str());
                    svalue_t *entry13 = find_for_insert(operation, &op_key, 0); *entry13 = op_value;
                    free_string(op_key.u.string);
                    free_string(op_value.u.string);
                }
                
                // Add description
                if (!route_docs->description.empty()) {
                    op_key.type = T_STRING; op_key.u.string = make_shared_string("description");
                    op_value.type = T_STRING; op_value.u.string = make_shared_string(route_docs->description.c_str());
                    svalue_t *entry14 = find_for_insert(operation, &op_key, 0); *entry14 = op_value;
                    free_string(op_key.u.string);
                    free_string(op_value.u.string);
                }
                
                // Add operation ID
                if (!route_docs->operation_id.empty()) {
                    op_key.type = T_STRING; op_key.u.string = make_shared_string("operationId");
                    op_value.type = T_STRING; op_value.u.string = make_shared_string(route_docs->operation_id.c_str());
                    svalue_t *entry15 = find_for_insert(operation, &op_key, 0); *entry15 = op_value;
                    free_string(op_key.u.string);
                    free_string(op_value.u.string);
                }
                
                // Add parameters
                if (route_docs->parameters) {
                    op_key.type = T_STRING; op_key.u.string = make_shared_string("parameters");
                    op_value.type = T_MAPPING; op_value.u.map = route_docs->parameters;
                    svalue_t *entry16 = find_for_insert(operation, &op_key, 0); *entry16 = op_value;
                    free_string(op_key.u.string);
                }
                
                // Add responses
                if (route_docs->responses) {
                    op_key.type = T_STRING; op_key.u.string = make_shared_string("responses");
                    op_value.type = T_MAPPING; op_value.u.map = route_docs->responses;
                    svalue_t *entry17 = find_for_insert(operation, &op_key, 0); *entry17 = op_value;
                    free_string(op_key.u.string);
                }
            } else {
                // Generate basic documentation
                op_key.type = T_STRING; op_key.u.string = make_shared_string("summary");
                std::string default_summary = method_route.first + " " + path_pattern;
                op_value.type = T_STRING; op_value.u.string = make_shared_string(default_summary.c_str());
                svalue_t *entry18 = find_for_insert(operation, &op_key, 0); *entry18 = op_value;
                free_string(op_key.u.string);
                free_string(op_value.u.string);
                
                // Default responses
                mapping_t *responses = allocate_mapping(1);
                mapping_t *response_200 = allocate_mapping(1);
                svalue_t resp_key, resp_value;
                
                resp_key.type = T_STRING; resp_key.u.string = make_shared_string("description");
                resp_value.type = T_STRING; resp_value.u.string = make_shared_string("Success");
                svalue_t *entry19 = find_for_insert(response_200, &resp_key, 0); *entry19 = resp_value;
                free_string(resp_key.u.string);
                free_string(resp_value.u.string);
                
                resp_key.type = T_STRING; resp_key.u.string = make_shared_string("200");
                resp_value.type = T_MAPPING; resp_value.u.map = response_200;
                svalue_t *entry20 = find_for_insert(responses, &resp_key, 0); *entry20 = resp_value;
                free_string(resp_key.u.string);
                free_mapping(response_200);
                
                op_key.type = T_STRING; op_key.u.string = make_shared_string("responses");
                op_value.type = T_MAPPING; op_value.u.map = responses;
                svalue_t *entry21 = find_for_insert(operation, &op_key, 0); *entry21 = op_value;
                free_string(op_key.u.string);
                free_mapping(responses);
            }
            
            // Add operation to path item
            svalue_t method_key, method_value;
            method_key.type = T_STRING; method_key.u.string = make_shared_string(method.c_str());
            method_value.type = T_MAPPING; method_value.u.map = operation;
            svalue_t *entry22 = find_for_insert(path_item, &method_key, 0); *entry22 = method_value;
            free_string(method_key.u.string);
            free_mapping(operation);
        }
        
        // Add path item to paths
        svalue_t path_key, path_value;
        path_key.type = T_STRING; path_key.u.string = make_shared_string(openapi_path.c_str());
        path_value.type = T_MAPPING; path_value.u.map = path_item;
        svalue_t *entry23 = find_for_insert(paths, &path_key, 0); *entry23 = path_value;
        free_string(path_key.u.string);
        free_mapping(path_item);
    }
    
    return paths;
}

// EFUN implementations

/**
 * rest_openapi_generate(int router_id, mapping api_info)
 * Generate OpenAPI specification
 */
void f_rest_generate_openapi(void) {
    mapping_t *api_info = nullptr;
    
    if (st_num_arg >= 2) {
        api_info = sp->u.map;
        sp--;
    }
    int router_id = sp->u.number;
    
    mapping_t *result = rest_openapi_generate_impl(router_id, api_info);
    
    if (api_info) free_mapping(api_info);
    
    sp->type = T_MAPPING;
    sp->u.map = result;
}

/**
 * rest_route_set_docs(int router_id, string method, string pattern, mapping docs)
 * Set documentation for a route
 */
void f_rest_set_route_docs(void) {
    mapping_t *docs = sp->u.map;
    sp--;
    const char *pattern = sp->u.string;
    sp--;
    const char *method = sp->u.string;
    sp--;
    int router_id = sp->u.number;
    
    int result = rest_route_set_docs_impl(router_id, method, pattern, docs);
    
    // Clean up
    free_mapping(docs);
    free_string((sp + 1)->u.string); // pattern
    free_string((sp + 2)->u.string); // method
    
    sp->type = T_NUMBER;
    sp->u.number = result;
}

/**
 * rest_docs_serve(int router_id, string path, string ui_type)
 * Serve interactive documentation
 */
void f_rest_serve_docs(void) {
    const char *ui_type = "swagger"; // default
    
    if (st_num_arg >= 3) {
        ui_type = sp->u.string;
        sp--;
    }
    const char *path = sp->u.string;
    sp--;
    int router_id = sp->u.number;
    
    int result = rest_docs_serve_impl(router_id, path, ui_type);
    
    // Clean up
    if (st_num_arg >= 3) {
        free_string((sp + 2)->u.string); // ui_type
    }
    free_string((sp + 1)->u.string); // path
    
    sp->type = T_NUMBER;
    sp->u.number = result;
}