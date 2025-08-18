/**
 * http.h - HTTP package header for FluffOS
 *
 * Provides HTTP server and client functionality using libwebsockets
 * integration with FluffOS's existing event system.
 *
 * -- HTTP Package for FluffOS --
 */

#ifndef PACKAGES_HTTP_H_
#define PACKAGES_HTTP_H_

#include "base/package_api.h"
#include <libwebsockets.h>
#include <event2/event.h>
#include <map>
#include <memory>

// HTTP server context structure
struct http_server_context {
    int server_id;
    int port;
    struct lws_context *lws_context;
    struct event_base *event_base;
    svalue_t callback;
    std::map<int, std::shared_ptr<struct http_request_context>> pending_requests;
    int next_request_id;
    bool active;
};

// HTTP request context
struct http_request_context {
    int request_id;
    int server_id;
    struct lws *wsi;
    std::string method;
    std::string uri;
    std::string protocol;
    std::map<std::string, std::string> headers;
    std::string body;
    bool complete;
    svalue_t response_data;
    bool response_sent;
};

// HTTP client request context
struct http_client_context {
    int request_id;
    std::string url;
    std::string method;
    std::map<std::string, std::string> headers;
    std::string body;
    svalue_t callback;
    struct lws *wsi;
    std::string response_body;
    int response_status;
    std::map<std::string, std::string> response_headers;
    bool complete;
};

// Global HTTP management
extern std::map<int, std::shared_ptr<http_server_context>> g_http_servers;
extern std::map<int, std::shared_ptr<http_client_context>> g_http_clients;
extern int g_next_server_id;
extern int g_next_client_id;

// Function declarations
int http_server_start_impl(int port, svalue_t *callback, mapping_t *options);
int http_server_stop_impl(int server_id);
int http_response_send_impl(int request_id, mapping_t *response);
int http_request_impl(const char *url, mapping_t *options, svalue_t *callback);

// Utility functions
mapping_t *http_headers_to_mapping(const std::map<std::string, std::string> &headers);
std::map<std::string, std::string> mapping_to_http_headers(mapping_t *headers);
void http_cleanup_server(int server_id);
void http_cleanup_client(int client_id);

// libwebsockets callbacks
int http_lws_callback_server(struct lws *wsi, enum lws_callback_reasons reason,
                           void *user, void *in, size_t len);
int http_lws_callback_client(struct lws *wsi, enum lws_callback_reasons reason,
                           void *user, void *in, size_t len);

#endif  // PACKAGES_HTTP_H_