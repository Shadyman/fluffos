# HTTP Package Specification
# HTTP/1.1 Protocol Support for FluffOS Unified Socket Architecture

name: http
type: socket_protocol
version: 1.0.0
description: "HTTP/1.1 protocol implementation with RESTful API framework"

dependencies:
  - sockets  # Phase 1 unified socket architecture

socket_modes:
  HTTP_CLIENT: 15
  HTTP_SERVER: 16
  REST_API: 17

socket_options:
  # Core HTTP options (keep SO_ prefix)
  SO_HTTP_HEADERS: 4020
  SO_HTTP_METHOD: 4021
  SO_HTTP_TIMEOUT: 4022
  SO_HTTP_USER_AGENT: 4023
  SO_HTTP_FOLLOW_REDIRECTS: 4024
  SO_HTTP_MAX_REDIRECTS: 4025
  SO_HTTP_VERIFY_SSL: 4026
  SO_HTTP_SSL_CERT: 4027
  SO_HTTP_SSL_KEY: 4028
  
  # REST options (NO SO_ prefix per architecture guide)
  REST_ADD_ROUTE: 4100
  REST_OPENAPI_INFO: 4101
  REST_JWT_SECRET: 4102
  REST_DOCS_PATH: 4103
  REST_ROUTER_CONFIG: 4104
  REST_VALIDATION_SCHEMA: 4105
  REST_CORS_CONFIG: 4106
  REST_MIDDLEWARE: 4107
  REST_AUTH_CONFIG: 4108
  REST_RATE_LIMIT: 4109

efuns:
  # HTTP client functions
  - http_get(string url, mapping headers, function callback)
  - http_post(string url, mixed data, mapping headers, function callback)
  - http_put(string url, mixed data, mapping headers, function callback)
  - http_delete(string url, mapping headers, function callback)
  - http_request(string method, string url, mixed data, mapping headers, function callback)
  
  # HTTP server functions  
  - http_server_start(int port, mapping config, function request_handler)
  - http_server_stop(int server_id)
  - http_send_response(int connection_id, int status, string body, mapping headers)
  - http_get_request_info(int connection_id)
  
  # REST API functions
  - rest_add_route(string method, string pattern, string handler_obj, string handler_func)
  - rest_remove_route(int route_id)
  - rest_get_routes()
  - rest_generate_openapi_spec()
  - rest_set_api_info(mapping api_info)
  - rest_enable_cors(mapping cors_config)
  - rest_add_middleware(string name, string function)

author: "FluffOS Unified Socket Architecture - Phase 2"
maintainer: "Subagents Echo & Foxtrot (Corrected by Architecture Subagent)"