/**
 * rest.h - REST package header for FluffOS
 *
 * Provides high-level REST API functionality built on the HTTP package
 *
 * -- REST Package for FluffOS --
 */

#ifndef PACKAGES_REST_H_
#define PACKAGES_REST_H_

#include "base/package_api.h"
#include <map>
#include <vector>
#include <regex>
#include <memory>

// REST route structure
struct rest_route {
    std::string method;
    std::string pattern;
    std::regex compiled_pattern;
    std::vector<std::string> param_names;
    svalue_t handler;
    mapping_t *metadata;  // General route metadata
    mapping_t *openapi_docs;  // OpenAPI documentation for this route
};

// REST router context
struct rest_router_context {
    int router_id;
    std::vector<std::shared_ptr<rest_route>> routes;
    mapping_t *middleware;
    mapping_t *config;
    mapping_t *openapi_info;  // OpenAPI specification info
    std::string docs_path;    // Path for serving documentation
    std::string ui_path;      // Path for serving UI
};

// Global REST management
extern std::map<int, std::shared_ptr<rest_router_context>> g_rest_routers;
extern int g_next_router_id;

// Function declarations
int rest_router_create_impl();
int rest_route_add_impl(int router_id, const char *method, const char *pattern, svalue_t *handler);
mapping_t *rest_route_process_impl(int router_id, mapping_t *request);
char *rest_jwt_create_impl(mapping_t *payload, const char *secret);
mapping_t *rest_jwt_verify_impl(const char *token, const char *secret);
mapping_t *rest_validate_impl(svalue_t *data, mapping_t *schema);
mapping_t *rest_parse_request_impl(mapping_t *http_request);
mapping_t *rest_format_response_impl(svalue_t *data, int status, mapping_t *headers);
mapping_t *rest_generate_openapi_impl(int router_id, mapping_t *api_info);
int rest_set_route_docs_impl(int router_id, const char *method, const char *pattern, mapping_t *docs);
int rest_serve_docs_impl(int router_id, const char *docs_path, const char *ui_path);

// Utility functions
std::string rest_pattern_to_regex(const std::string &pattern, std::vector<std::string> &param_names);
mapping_t *rest_extract_path_params(const std::string &pattern, const std::string &path);
mapping_t *rest_parse_query_string(const std::string &query);
void rest_cleanup_router(int router_id);

// Validation functions
bool rest_validate_string(svalue_t *value, mapping_t *schema);
bool rest_validate_number(svalue_t *value, mapping_t *schema);
bool rest_validate_array(svalue_t *value, mapping_t *schema);
bool rest_validate_mapping(svalue_t *value, mapping_t *schema);

// JWT functions
std::string rest_base64_encode(const std::string &input);
std::string rest_base64_decode(const std::string &input);
std::string rest_hmac_sha256(const std::string &data, const std::string &key);

#endif  // PACKAGES_REST_H_