#ifndef HTTP_H_
#define HTTP_H_

/*
 * HTTP Package - HTTP/1.1 Protocol Support for FluffOS Unified Socket Architecture
 * 
 * This is the main header for the HTTP package that provides comprehensive
 * HTTP/1.1 protocol support built on top of Phase 1's unified socket system.
 * 
 * Architecture:
 * - http_parser.cc/h: HTTP request/response parsing
 * - http_client.cc: HTTP client implementation  
 * - http_server.cc: HTTP server implementation
 * - http_response.cc: HTTP response generation
 * - rest/: RESTful API framework (separate subdirectory)
 * 
 * Integration with Phase 1:
 * - Uses SocketOptionManager for configuration
 * - Integrates with socket validation system
 * - Maintains SO_ prefix compliance for HTTP core options
 * - Removes SO_ prefix for REST-specific options per architecture guide
 */

#include "base/package_api.h"
#include "packages/sockets/socket_option_manager.h"
#include "packages/sockets/socket_efuns.h"
#include <string>
#include <unordered_map>
#include <memory>
#include <vector>

// HTTP Package version
#define HTTP_PACKAGE_VERSION "1.0.0"

// Forward declarations from restructured components
class HTTPParser;
class HTTPClient; 
class HTTPServer;
class HTTPResponse;

// Import core HTTP structures and enums from parser
#include "http_parser.h"

/*
 * Main HTTP Package Interface
 */

// Package initialization
void init_http_package();

// HTTP client functions (efuns)
void f_http_get();
void f_http_post();
void f_http_put();
void f_http_delete();
void f_http_request();

// HTTP server functions (efuns)
void f_http_server_start();
void f_http_server_stop();
void f_http_send_response();
void f_http_get_request_info();

// Socket mode integration
bool socket_enable_http_mode(int socket_id, const mapping_t* options = nullptr);
bool socket_is_http_mode(int socket_id);
int socket_process_http_data(int socket_id, const char* data, size_t length);
char* socket_generate_http_response(int socket_id, int status, const char* body, 
                                   const mapping_t* headers = nullptr);

// HTTP handler management
class HTTPHandler* get_http_handler(int socket_id);
bool register_http_handler(int socket_id, std::unique_ptr<HTTPHandler> handler);
void cleanup_http_handler(int socket_id);

/*
 * HTTP Package Constants
 */

// Socket modes (registered with Phase 1 system)
#define SOCKET_MODE_HTTP_CLIENT 15
#define SOCKET_MODE_HTTP_SERVER 16

// HTTP Socket Options (maintain SO_ prefix per architecture guide)
#define SO_HTTP_HEADERS      4020
#define SO_HTTP_METHOD       4021
#define SO_HTTP_TIMEOUT      4022
#define SO_HTTP_USER_AGENT   4023
#define SO_HTTP_FOLLOW_REDIRECTS 4024
#define SO_HTTP_MAX_REDIRECTS    4025
#define SO_HTTP_VERIFY_SSL       4026
#define SO_HTTP_SSL_CERT         4027
#define SO_HTTP_SSL_KEY          4028

// Default values - use values from socket_options.h to avoid conflicts
#define DEFAULT_HTTP_MAX_REDIRECTS 5

// Package integration with FluffOS build system
extern "C" {
    // Package registration function
    void register_http_package();
    
    // EFun specifications
    extern const efun_ptr_t http_efuns[];
    extern const int num_http_efuns;
}

#endif  // HTTP_H_