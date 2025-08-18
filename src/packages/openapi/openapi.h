/**
 * openapi.h - OpenAPI package header for FluffOS
 *
 * Provides OpenAPI 3.x documentation generation and serving
 *
 * -- OpenAPI Package for FluffOS --
 */

#ifndef PACKAGES_OPENAPI_H_
#define PACKAGES_OPENAPI_H_

#include "base/package_api.h"
#include "../rest/rest.h"
#include <map>
#include <string>

// OpenAPI documentation structure
struct openapi_route_docs {
    std::string summary;
    std::string description;
    std::string operation_id;
    mapping_t *parameters;
    mapping_t *responses;
    mapping_t *request_body;
    mapping_t *security;
    mapping_t *tags;
};

// OpenAPI server documentation
struct openapi_docs_context {
    int router_id;
    mapping_t *api_info;
    mapping_t *servers;
    mapping_t *security_schemes;
    std::map<std::string, std::shared_ptr<openapi_route_docs>> route_docs;
    mapping_t *components;
};

// Global OpenAPI management
extern std::map<int, std::shared_ptr<openapi_docs_context>> g_openapi_docs;

// Function declarations
mapping_t *rest_openapi_generate_impl(int router_id, mapping_t *api_info);
int rest_route_set_docs_impl(int router_id, const char *method, const char *pattern, mapping_t *docs);
int rest_docs_serve_impl(int router_id, const char *path, const char *ui_type);

// OpenAPI generation functions
mapping_t *openapi_generate_spec(std::shared_ptr<openapi_docs_context> docs_ctx);
mapping_t *openapi_generate_paths(std::shared_ptr<rest_router_context> router_ctx, 
                                 std::shared_ptr<openapi_docs_context> docs_ctx);
mapping_t *openapi_generate_components(std::shared_ptr<openapi_docs_context> docs_ctx);

// Documentation serving functions
std::string openapi_generate_swagger_html(const std::string &spec_url, const std::string &title);
std::string openapi_generate_redoc_html(const std::string &spec_url, const std::string &title);

// Utility functions
std::string openapi_route_key(const std::string &method, const std::string &pattern);
void openapi_cleanup_docs(int router_id);

#endif  // PACKAGES_OPENAPI_H_