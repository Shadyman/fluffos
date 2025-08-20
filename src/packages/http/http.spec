/*
 * HTTP efuns - HTTP/1.1 Protocol Support for FluffOS Unified Socket Architecture
 * 
 * These functions provide HTTP client and server capabilities through the
 * unified socket architecture. This package was restructured from the
 * sockets package as part of architecture correction.
 */

// HTTP client functions
int http_get(string, mapping | void, function | void);
int http_post(string, mixed, mapping | void, function | void);
int http_put(string, mixed, mapping | void, function | void);
int http_delete(string, mapping | void, function | void);
int http_request(string, string, mixed | void, mapping | void, function | void);

// HTTP server functions  
int http_server_start(int, mapping | void, function | void);
int http_server_stop(int);
int http_send_response(int, int, string | void, mapping | void);
mapping http_get_request_info(int);

// REST API functions
int rest_add_route(string, string, string, string);
int rest_remove_route(int);
mixed *rest_get_routes();
string rest_generate_openapi_spec();
int rest_set_api_info(mapping);
int rest_enable_cors(mapping | void);
int rest_add_middleware(string, string);