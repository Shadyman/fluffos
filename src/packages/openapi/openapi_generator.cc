/**
 * openapi_generator.cc - OpenAPI specification generation
 *
 * Implements the core OpenAPI generation logic
 */

#include "openapi.h"

/**
 * Generate OpenAPI specification implementation
 */
mapping_t *rest_openapi_generate_impl(int router_id, mapping_t *api_info) {
    // Find or create docs context
    auto docs_it = g_openapi_docs.find(router_id);
    std::shared_ptr<openapi_docs_context> docs_ctx;
    
    if (docs_it == g_openapi_docs.end()) {
        // Create new docs context
        docs_ctx = std::make_shared<openapi_docs_context>();
        docs_ctx->router_id = router_id;
        docs_ctx->api_info = nullptr;
        docs_ctx->servers = nullptr;
        docs_ctx->security_schemes = nullptr;
        docs_ctx->components = nullptr;
        
        g_openapi_docs[router_id] = docs_ctx;
    } else {
        docs_ctx = docs_it->second;
    }
    
    // Update API info if provided
    if (api_info) {
        if (docs_ctx->api_info) {
            free_mapping(docs_ctx->api_info);
        }
        
        // Deep copy the mapping
        docs_ctx->api_info = allocate_mapping(api_info->count);
        for (int i = 0; i < api_info->table_size; i++) {
            mapping_node_t *node = api_info->table[i];
            while (node) {
                svalue_t *entry1 = find_for_insert(docs_ctx->api_info, &node->values[0], 0); *entry1 = node->values[1];
                node = node->next;
            }
        }
    }
    
    // Generate and return OpenAPI specification
    return openapi_generate_spec(docs_ctx);
}

/**
 * Set route documentation implementation
 */
int rest_route_set_docs_impl(int router_id, const char *method, const char *pattern, mapping_t *docs) {
    if (!method || !pattern || !docs) {
        return 0;
    }
    
    // Find or create docs context
    auto docs_it = g_openapi_docs.find(router_id);
    std::shared_ptr<openapi_docs_context> docs_ctx;
    
    if (docs_it == g_openapi_docs.end()) {
        // Create new docs context
        docs_ctx = std::make_shared<openapi_docs_context>();
        docs_ctx->router_id = router_id;
        docs_ctx->api_info = nullptr;
        docs_ctx->servers = nullptr;
        docs_ctx->security_schemes = nullptr;
        docs_ctx->components = nullptr;
        
        g_openapi_docs[router_id] = docs_ctx;
    } else {
        docs_ctx = docs_it->second;
    }
    
    // Create route docs
    auto route_docs = std::make_shared<openapi_route_docs>();
    
    // Extract documentation fields from mapping
    svalue_t *summary_val = find_string_in_mapping(docs, "summary");
    if (summary_val && summary_val->type == T_STRING) {
        route_docs->summary = summary_val->u.string;
    }
    
    svalue_t *description_val = find_string_in_mapping(docs, "description");
    if (description_val && description_val->type == T_STRING) {
        route_docs->description = description_val->u.string;
    }
    
    svalue_t *operation_id_val = find_string_in_mapping(docs, "operationId");
    if (operation_id_val && operation_id_val->type == T_STRING) {
        route_docs->operation_id = operation_id_val->u.string;
    }
    
    // Copy complex fields
    svalue_t *parameters_val = find_string_in_mapping(docs, "parameters");
    if (parameters_val && parameters_val->type == T_MAPPING) {
        route_docs->parameters = allocate_mapping(parameters_val->u.map->count);
        for (int i = 0; i < parameters_val->u.map->table_size; i++) {
            mapping_node_t *node = parameters_val->u.map->table[i];
            while (node) {
                svalue_t *entry2 = find_for_insert(route_docs->parameters, &node->values[0], 0); *entry2 = node->values[1];
                node = node->next;
            }
        }
    } else {
        route_docs->parameters = nullptr;
    }
    
    svalue_t *responses_val = find_string_in_mapping(docs, "responses");
    if (responses_val && responses_val->type == T_MAPPING) {
        route_docs->responses = allocate_mapping(responses_val->u.map->count);
        for (int i = 0; i < responses_val->u.map->table_size; i++) {
            mapping_node_t *node = responses_val->u.map->table[i];
            while (node) {
                svalue_t *entry3 = find_for_insert(route_docs->responses, &node->values[0], 0); *entry3 = node->values[1];
                node = node->next;
            }
        }
    } else {
        route_docs->responses = nullptr;
    }
    
    svalue_t *request_body_val = find_string_in_mapping(docs, "requestBody");
    if (request_body_val && request_body_val->type == T_MAPPING) {
        route_docs->request_body = allocate_mapping(request_body_val->u.map->count);
        for (int i = 0; i < request_body_val->u.map->table_size; i++) {
            mapping_node_t *node = request_body_val->u.map->table[i];
            while (node) {
                svalue_t *entry4 = find_for_insert(route_docs->request_body, &node->values[0], 0); *entry4 = node->values[1];
                node = node->next;
            }
        }
    } else {
        route_docs->request_body = nullptr;
    }
    
    svalue_t *security_val = find_string_in_mapping(docs, "security");
    if (security_val && security_val->type == T_MAPPING) {
        route_docs->security = allocate_mapping(security_val->u.map->count);
        for (int i = 0; i < security_val->u.map->table_size; i++) {
            mapping_node_t *node = security_val->u.map->table[i];
            while (node) {
                svalue_t *entry5 = find_for_insert(route_docs->security, &node->values[0], 0); *entry5 = node->values[1];
                node = node->next;
            }
        }
    } else {
        route_docs->security = nullptr;
    }
    
    svalue_t *tags_val = find_string_in_mapping(docs, "tags");
    if (tags_val && tags_val->type == T_MAPPING) {
        route_docs->tags = allocate_mapping(tags_val->u.map->count);
        for (int i = 0; i < tags_val->u.map->table_size; i++) {
            mapping_node_t *node = tags_val->u.map->table[i];
            while (node) {
                svalue_t *entry6 = find_for_insert(route_docs->tags, &node->values[0], 0); *entry6 = node->values[1];
                node = node->next;
            }
        }
    } else {
        route_docs->tags = nullptr;
    }
    
    // Store route docs
    std::string route_key = openapi_route_key(method, pattern);
    docs_ctx->route_docs[route_key] = route_docs;
    
    return 1; // Success
}

/**
 * Generate OpenAPI components section
 */
mapping_t *openapi_generate_components(std::shared_ptr<openapi_docs_context> docs_ctx) {
    mapping_t *components = allocate_mapping(3);
    svalue_t key, value;
    
    // Schemas section
    mapping_t *schemas = allocate_mapping(0); // Empty for now
    key.type = T_STRING; key.u.string = make_shared_string("schemas");
    value.type = T_MAPPING; value.u.map = schemas;
    svalue_t *entry7 = find_for_insert(components, &key, 0); *entry7 = value;
    free_string(key.u.string);
    free_mapping(schemas);
    
    // Security schemes
    if (docs_ctx->security_schemes) {
        key.type = T_STRING; key.u.string = make_shared_string("securitySchemes");
        value.type = T_MAPPING; value.u.map = docs_ctx->security_schemes;
        svalue_t *entry8 = find_for_insert(components, &key, 0); *entry8 = value;
        free_string(key.u.string);
    } else {
        // Default JWT scheme
        mapping_t *security_schemes = allocate_mapping(1);
        mapping_t *jwt_scheme = allocate_mapping(3);
        svalue_t scheme_key, scheme_value;
        
        scheme_key.type = T_STRING; scheme_key.u.string = make_shared_string("type");
        scheme_value.type = T_STRING; scheme_value.u.string = make_shared_string("http");
        svalue_t *entry9 = find_for_insert(jwt_scheme, &scheme_key, 0); *entry9 = scheme_value;
        free_string(scheme_key.u.string);
        free_string(scheme_value.u.string);
        
        scheme_key.type = T_STRING; scheme_key.u.string = make_shared_string("scheme");
        scheme_value.type = T_STRING; scheme_value.u.string = make_shared_string("bearer");
        svalue_t *entry10 = find_for_insert(jwt_scheme, &scheme_key, 0); *entry10 = scheme_value;
        free_string(scheme_key.u.string);
        free_string(scheme_value.u.string);
        
        scheme_key.type = T_STRING; scheme_key.u.string = make_shared_string("bearerFormat");
        scheme_value.type = T_STRING; scheme_value.u.string = make_shared_string("JWT");
        svalue_t *entry11 = find_for_insert(jwt_scheme, &scheme_key, 0); *entry11 = scheme_value;
        free_string(scheme_key.u.string);
        free_string(scheme_value.u.string);
        
        scheme_key.type = T_STRING; scheme_key.u.string = make_shared_string("bearerAuth");
        scheme_value.type = T_MAPPING; scheme_value.u.map = jwt_scheme;
        svalue_t *entry12 = find_for_insert(security_schemes, &scheme_key, 0); *entry12 = scheme_value;
        free_string(scheme_key.u.string);
        free_mapping(jwt_scheme);
        
        key.type = T_STRING; key.u.string = make_shared_string("securitySchemes");
        value.type = T_MAPPING; value.u.map = security_schemes;
        svalue_t *entry13 = find_for_insert(components, &key, 0); *entry13 = value;
        free_string(key.u.string);
        free_mapping(security_schemes);
    }
    
    return components;
}

/**
 * Cleanup OpenAPI docs
 */
void openapi_cleanup_docs(int router_id) {
    auto it = g_openapi_docs.find(router_id);
    if (it != g_openapi_docs.end()) {
        auto docs_ctx = it->second;
        
        // Clean up mappings
        if (docs_ctx->api_info) free_mapping(docs_ctx->api_info);
        if (docs_ctx->servers) free_mapping(docs_ctx->servers);
        if (docs_ctx->security_schemes) free_mapping(docs_ctx->security_schemes);
        if (docs_ctx->components) free_mapping(docs_ctx->components);
        
        // Clean up route docs
        for (auto &route_pair : docs_ctx->route_docs) {
            auto route_docs = route_pair.second;
            if (route_docs->parameters) free_mapping(route_docs->parameters);
            if (route_docs->responses) free_mapping(route_docs->responses);
            if (route_docs->request_body) free_mapping(route_docs->request_body);
            if (route_docs->security) free_mapping(route_docs->security);
            if (route_docs->tags) free_mapping(route_docs->tags);
        }
        
        // Remove from global map
        g_openapi_docs.erase(it);
    }
}