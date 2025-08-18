/**
 * http_client.cc - HTTP client implementation
 *
 * HTTP client functionality using libwebsockets
 */

#include "http.h"
#include <nlohmann/json.hpp>

/**
 * Make HTTP client request implementation
 */
int http_request_impl(const char *url, mapping_t *options, svalue_t *callback) {
    if (!url || strlen(url) == 0) {
        return -1; // Invalid URL
    }
    
    // Create client context
    auto client_ctx = std::make_shared<http_client_context>();
    client_ctx->request_id = g_next_client_id++;
    client_ctx->url = url;
    client_ctx->method = "GET"; // Default method
    client_ctx->complete = false;
    client_ctx->response_status = 0;
    
    // Copy callback
    if (callback) {
        assign_svalue(&client_ctx->callback, callback);
    } else {
        client_ctx->callback.type = T_NUMBER;
        client_ctx->callback.u.number = 0;
    }
    
    // Parse options
    if (options) {
        svalue_t *method_val = find_string_in_mapping(options, "method");
        if (method_val && method_val->type == T_STRING) {
            client_ctx->method = method_val->u.string;
        }
        
        svalue_t *headers_val = find_string_in_mapping(options, "headers");
        if (headers_val && headers_val->type == T_MAPPING) {
            client_ctx->headers = mapping_to_http_headers(headers_val->u.map);
        }
        
        svalue_t *body_val = find_string_in_mapping(options, "body");
        if (body_val && body_val->type == T_STRING) {
            client_ctx->body = body_val->u.string;
        }
        
        // Handle JSON data
        svalue_t *json_val = find_string_in_mapping(options, "json");
        if (json_val) {
            // Convert to JSON string
            try {
                nlohmann::json j;
                
                // Convert svalue to JSON based on type
                switch (json_val->type) {
                    case T_STRING:
                        j = json_val->u.string;
                        break;
                    case T_NUMBER:
                        j = json_val->u.number;
                        break;
                    case T_MAPPING: {
                        // Convert mapping to JSON object
                        j = nlohmann::json::object();
                        for (int i = 0; i < json_val->u.map->table_size; i++) {
                            mapping_node_t *node = json_val->u.map->table[i];
                            while (node) {
                                if (node->values[0].type == T_STRING) {
                                    std::string key = node->values[0].u.string;
                                    if (node->values[1].type == T_STRING) {
                                        j[key] = node->values[1].u.string;
                                    } else if (node->values[1].type == T_NUMBER) {
                                        j[key] = node->values[1].u.number;
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
                
                client_ctx->body = j.dump();
                client_ctx->headers["Content-Type"] = "application/json";
                
            } catch (const std::exception &e) {
                // JSON encoding failed
                return -1;
            }
        }
    }
    
    // Parse URL components
    struct lws_client_connect_info ccinfo;
    memset(&ccinfo, 0, sizeof(ccinfo));
    
    // Simple URL parsing (assumes http:// or https://)
    std::string url_str = url;
    bool is_ssl = (url_str.substr(0, 8) == "https://");
    
    size_t proto_end = url_str.find("://");
    if (proto_end == std::string::npos) {
        return -1; // Invalid URL format
    }
    
    std::string remainder = url_str.substr(proto_end + 3);
    size_t path_start = remainder.find('/');
    
    std::string host_port;
    std::string path = "/";
    
    if (path_start != std::string::npos) {
        host_port = remainder.substr(0, path_start);
        path = remainder.substr(path_start);
    } else {
        host_port = remainder;
    }
    
    // Split host and port
    size_t port_start = host_port.find(':');
    std::string host;
    int port;
    
    if (port_start != std::string::npos) {
        host = host_port.substr(0, port_start);
        port = std::stoi(host_port.substr(port_start + 1));
    } else {
        host = host_port;
        port = is_ssl ? 443 : 80;
    }
    
    // Setup connection info
    ccinfo.context = nullptr; // Will create new context
    ccinfo.address = host.c_str();
    ccinfo.port = port;
    ccinfo.path = path.c_str();
    ccinfo.host = host.c_str();
    ccinfo.origin = host.c_str();
    ccinfo.protocol = nullptr;
    ccinfo.method = client_ctx->method.c_str();
    ccinfo.userdata = client_ctx.get();
    
    if (is_ssl) {
        ccinfo.ssl_connection = 1;
    }
    
    // Create libwebsockets context for client
    struct lws_context_creation_info info;
    memset(&info, 0, sizeof(info));
    
    info.port = CONTEXT_PORT_NO_LISTEN;
    info.protocols = nullptr;
    info.gid = -1;
    info.uid = -1;
    info.options = LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT | LWS_SERVER_OPTION_LIBEVENT;
    info.foreign_loops = reinterpret_cast<void**>(&g_event_base);
    
    // Set client callback
    static struct lws_protocols client_protocols[] = {
        {
            "http",
            http_lws_callback_client,
            sizeof(struct http_client_context),
            0,
            0, nullptr, 0
        },
        { nullptr, nullptr, 0, 0, 0, nullptr, 0 } // terminator
    };
    info.protocols = client_protocols;
    
    ccinfo.context = lws_create_context(&info);
    if (!ccinfo.context) {
        return -1;
    }
    
    // Connect
    client_ctx->wsi = lws_client_connect_via_info(&ccinfo);
    if (!client_ctx->wsi) {
        lws_context_destroy(ccinfo.context);
        return -1;
    }
    
    // Store client context
    g_http_clients[client_ctx->request_id] = client_ctx;
    
    return client_ctx->request_id;
}

/**
 * Cleanup client resources
 */
void http_cleanup_client(int client_id) {
    auto it = g_http_clients.find(client_id);
    if (it != g_http_clients.end()) {
        auto client_ctx = it->second;
        
        // Clean up callback
        free_svalue(&client_ctx->callback, "http_cleanup_client");
        
        // Remove from global map
        g_http_clients.erase(it);
    }
}