/**
 * http_server.cc - HTTP server implementation
 *
 * HTTP server functionality using libwebsockets integrated with
 * FluffOS's libevent2 event system.
 */

#include "http.h"
#include <algorithm>

// External references
extern struct event_base *g_event_base;

/**
 * Start HTTP server implementation
 */
int http_server_start_impl(int port, svalue_t *callback, mapping_t *options) {
    if (port < 1 || port > 65535) {
        return -1; // Invalid port
    }
    
    // Create server context
    auto server_ctx = std::make_shared<http_server_context>();
    server_ctx->server_id = g_next_server_id++;
    server_ctx->port = port;
    server_ctx->event_base = g_event_base;
    server_ctx->next_request_id = 1;
    server_ctx->active = false;
    
    // Copy callback
    if (callback) {
        assign_svalue(&server_ctx->callback, callback);
    } else {
        server_ctx->callback.type = T_NUMBER;
        server_ctx->callback.u.number = 0;
    }
    
    // Setup libwebsockets context info
    struct lws_context_creation_info info;
    memset(&info, 0, sizeof(info));
    
    info.port = port;
    info.iface = nullptr; // Listen on all interfaces
    info.protocols = nullptr; // Use default HTTP protocol
    info.gid = -1;
    info.uid = -1;
    info.options = LWS_SERVER_OPTION_HTTP_HEADERS_SECURITY_BEST_PRACTICES_ENFORCE |
                   LWS_SERVER_OPTION_LIBEVENT;
    info.user = server_ctx.get();
    info.foreign_loops = reinterpret_cast<void**>(&g_event_base);
    
    // Set HTTP callback
    static struct lws_protocols protocols[] = {
        {
            "http",
            http_lws_callback_server,
            sizeof(struct http_request_context),
            0,
            0, nullptr, 0
        },
        { nullptr, nullptr, 0, 0, 0, nullptr, 0 } // terminator
    };
    info.protocols = protocols;
    
    // Parse options mapping
    if (options) {
        svalue_t *ssl_cert = find_string_in_mapping(options, "ssl_cert");
        svalue_t *ssl_key = find_string_in_mapping(options, "ssl_key");
        
        if (ssl_cert && ssl_cert->type == T_STRING && 
            ssl_key && ssl_key->type == T_STRING) {
            info.ssl_cert_filepath = ssl_cert->u.string;
            info.ssl_private_key_filepath = ssl_key->u.string;
            info.options |= LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;
        }
        
        svalue_t *interface_val = find_string_in_mapping(options, "interface");
        if (interface_val && interface_val->type == T_STRING) {
            info.iface = interface_val->u.string;
        }
    }
    
    // Create libwebsockets context
    server_ctx->lws_context = lws_create_context(&info);
    if (!server_ctx->lws_context) {
        return -1; // Failed to create context
    }
    
    server_ctx->active = true;
    
    // Store server context
    g_http_servers[server_ctx->server_id] = server_ctx;
    
    return server_ctx->server_id;
}

/**
 * Stop HTTP server implementation
 */
int http_server_stop_impl(int server_id) {
    auto it = g_http_servers.find(server_id);
    if (it == g_http_servers.end()) {
        return 0; // Server not found
    }
    
    auto server_ctx = it->second;
    if (server_ctx->lws_context) {
        lws_context_destroy(server_ctx->lws_context);
        server_ctx->lws_context = nullptr;
    }
    
    server_ctx->active = false;
    
    // Clean up callback
    free_svalue(&server_ctx->callback, "http_server_stop");
    
    // Remove from global map
    g_http_servers.erase(it);
    
    return 1; // Success
}

/**
 * Send HTTP response implementation
 */
int http_response_send_impl(int request_id, mapping_t *response) {
    if (!response) return 0;
    
    // Find the request context
    std::shared_ptr<http_request_context> req_ctx = nullptr;
    std::shared_ptr<http_server_context> server_ctx = nullptr;
    
    for (auto &server_pair : g_http_servers) {
        auto it = server_pair.second->pending_requests.find(request_id);
        if (it != server_pair.second->pending_requests.end()) {
            req_ctx = it->second;
            server_ctx = server_pair.second;
            break;
        }
    }
    
    if (!req_ctx || req_ctx->response_sent) {
        return 0; // Request not found or already responded
    }
    
    // Extract response data
    int status = 200;
    std::string body;
    std::map<std::string, std::string> headers;
    
    svalue_t *status_val = find_string_in_mapping(response, "status");
    if (status_val && status_val->type == T_NUMBER) {
        status = status_val->u.number;
    }
    
    svalue_t *body_val = find_string_in_mapping(response, "body");
    if (body_val && body_val->type == T_STRING) {
        body = body_val->u.string;
    }
    
    svalue_t *headers_val = find_string_in_mapping(response, "headers");
    if (headers_val && headers_val->type == T_MAPPING) {
        headers = mapping_to_http_headers(headers_val->u.map);
    }
    
    // Default content type if not specified
    if (headers.find("Content-Type") == headers.end()) {
        headers["Content-Type"] = "text/plain";
    }
    
    // Calculate response size
    size_t response_len = body.length();
    
    // Allocate response buffer with headers
    size_t header_space = 512; // Space for status line and headers
    char *response_buf = reinterpret_cast<char*>(malloc(LWS_PRE + header_space + response_len));
    if (!response_buf) {
        return 0;
    }
    
    char *response_start = response_buf + LWS_PRE;
    char *response_body = response_start + header_space;
    
    // Copy body
    memcpy(response_body, body.c_str(), response_len);
    
    // Build header string
    std::ostringstream header_stream;
    for (const auto &header : headers) {
        header_stream << header.first << ": " << header.second << "\r\n";
    }
    std::string header_string = header_stream.str();
    
    // Send response
    if (lws_add_http_header_status(req_ctx->wsi, status, 
                                   reinterpret_cast<unsigned char**>(&response_start), 
                                   reinterpret_cast<unsigned char*>(response_body))) {
        free(response_buf);
        return 0;
    }
    
    // Add headers
    for (const auto &header : headers) {
        if (lws_add_http_header_by_name(req_ctx->wsi, 
                                        reinterpret_cast<const unsigned char*>(header.first.c_str()),
                                        reinterpret_cast<const unsigned char*>(header.second.c_str()),
                                        header.second.length(),
                                        reinterpret_cast<unsigned char**>(&response_start),
                                        reinterpret_cast<unsigned char*>(response_body))) {
            free(response_buf);
            return 0;
        }
    }
    
    // Finalize headers
    if (lws_finalize_http_header(req_ctx->wsi, 
                                 reinterpret_cast<unsigned char**>(&response_start),
                                 reinterpret_cast<unsigned char*>(response_body))) {
        free(response_buf);
        return 0;
    }
    
    // Send response
    int written = lws_write(req_ctx->wsi, 
                           reinterpret_cast<unsigned char*>(response_body),
                           response_len, LWS_WRITE_HTTP_FINAL);
    
    free(response_buf);
    
    if (written < 0) {
        return 0;
    }
    
    req_ctx->response_sent = true;
    
    // Clean up request from pending list
    server_ctx->pending_requests.erase(request_id);
    
    return 1; // Success
}

/**
 * Cleanup server resources
 */
void http_cleanup_server(int server_id) {
    http_server_stop_impl(server_id);
}