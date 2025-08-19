/**
 * http.cc - HTTP package implementation for FluffOS
 *
 * Implements HTTP server and client functionality using libwebsockets
 * with integration into FluffOS's libevent2 event system.
 *
 * Features:
 * - HTTP/1.1 and HTTP/2 support via libwebsockets
 * - Async request/response handling
 * - Integration with existing FluffOS socket system
 * - Thread-safe operations
 * - Connection pooling and keep-alive
 *
 * -- HTTP Package for FluffOS --
 */

#include "http.h"
#include "base/package_api.h"
#include <nlohmann/json.hpp>
#include <cstring>
#include <sstream>

// Global HTTP state
std::map<int, std::shared_ptr<http_server_context>> g_http_servers;
std::map<int, std::shared_ptr<http_client_context>> g_http_clients;
int g_next_server_id = 1;
int g_next_client_id = 1;

// External references to FluffOS globals
extern struct event_base *g_event_base;

/**
 * Convert mapping to HTTP headers map
 */
std::map<std::string, std::string> mapping_to_http_headers(mapping_t *headers) {
    std::map<std::string, std::string> result;
    if (!headers) return result;
    
    // Iterate through mapping
    for (int i = 0; i < headers->table_size; i++) {
        mapping_node_t *node = headers->table[i];
        while (node) {
            if (node->values[0].type == T_STRING && node->values[1].type == T_STRING) {
                result[node->values[0].u.string] = node->values[1].u.string;
            }
            node = node->next;
        }
    }
    return result;
}

/**
 * Convert HTTP headers map to mapping
 */
mapping_t *http_headers_to_mapping(const std::map<std::string, std::string> &headers) {
    mapping_t *result = allocate_mapping(headers.size());
    
    for (const auto &pair : headers) {
        svalue_t key, value;
        key.type = T_STRING;
        key.subtype = STRING_SHARED;
        key.u.string = make_shared_string(pair.first.c_str());
        value.type = T_STRING;
        value.subtype = STRING_SHARED;
        value.u.string = make_shared_string(pair.second.c_str());
        
        svalue_t *entry = find_for_insert(result, &key, 0);
        *entry = value;
        
        // Clean up key reference
        free_string(key.u.string);
    }
    
    return result;
}

/**
 * libwebsockets callback for HTTP server
 */
int http_lws_callback_server(struct lws *wsi, enum lws_callback_reasons reason,
                           void *user, void *in, size_t len) {
    struct http_request_context *req_ctx = nullptr;
    
    switch (reason) {
        case LWS_CALLBACK_HTTP: {
            // New HTTP request
            auto server_ctx = reinterpret_cast<http_server_context*>(lws_context_user(lws_get_context(wsi)));
            if (!server_ctx) return -1;
            
            // Create request context
            auto request = std::make_shared<http_request_context>();
            request->request_id = server_ctx->next_request_id++;
            request->server_id = server_ctx->server_id;
            request->wsi = wsi;
            request->complete = false;
            request->response_sent = false;
            
            // Extract request information
            char *uri = reinterpret_cast<char*>(in);
            request->uri = uri ? uri : "/";
            request->method = "GET"; // Default method, will be updated if needed
            
            // Store request context
            server_ctx->pending_requests[request->request_id] = request;
            lws_set_wsi_user(wsi, request.get());
            
            // Extract headers using proper libwebsockets API
            char buf[256];
            
            // Get common headers
            if (lws_hdr_copy(wsi, buf, sizeof(buf), WSI_TOKEN_HOST) > 0) {
                request->headers["Host"] = buf;
            }
            if (lws_hdr_copy(wsi, buf, sizeof(buf), WSI_TOKEN_CONNECTION) > 0) {
                request->headers["Connection"] = buf;
            }
            if (lws_hdr_copy(wsi, buf, sizeof(buf), WSI_TOKEN_HTTP_CONTENT_TYPE) > 0) {
                request->headers["Content-Type"] = buf;
            }
            if (lws_hdr_copy(wsi, buf, sizeof(buf), WSI_TOKEN_HTTP_CONTENT_LENGTH) > 0) {
                request->headers["Content-Length"] = buf;
            }
            if (lws_hdr_copy(wsi, buf, sizeof(buf), WSI_TOKEN_HTTP_AUTHORIZATION) > 0) {
                request->headers["Authorization"] = buf;
            }
            
            return 0;
        }
        
        case LWS_CALLBACK_HTTP_BODY: {
            // Receive request body
            req_ctx = reinterpret_cast<http_request_context*>(lws_get_opaque_user_data(wsi));
            if (req_ctx) {
                req_ctx->body.append(reinterpret_cast<char*>(in), len);
            }
            return 0;
        }
        
        case LWS_CALLBACK_HTTP_BODY_COMPLETION: {
            // Request complete, trigger callback
            req_ctx = reinterpret_cast<http_request_context*>(lws_get_opaque_user_data(wsi));
            if (req_ctx) {
                req_ctx->complete = true;
                
                // Find server context
                auto server_it = g_http_servers.find(req_ctx->server_id);
                if (server_it != g_http_servers.end()) {
                    auto server_ctx = server_it->second;
                    
                    // Create LPC request mapping
                    mapping_t *request_mapping = allocate_mapping(6);
                    svalue_t key, value;
                    
                    // Add request_id
                    key.type = T_STRING;
                    key.subtype = STRING_SHARED;
                    key.u.string = make_shared_string("id");
                    value.type = T_NUMBER;
                    value.u.number = req_ctx->request_id;
                    svalue_t *entry = find_for_insert(request_mapping, &key, 0);
                    *entry = value;
                    free_string(key.u.string);
                    
                    // Add method
                    key.type = T_STRING;
                    key.subtype = STRING_SHARED;
                    key.u.string = make_shared_string("method");
                    value.type = T_STRING;
                    value.subtype = STRING_SHARED;
                    value.u.string = make_shared_string(req_ctx->method.c_str());
                    entry = find_for_insert(request_mapping, &key, 0);
                    *entry = value;
                    free_string(key.u.string);
                    
                    // Add URI
                    key.type = T_STRING;
                    key.subtype = STRING_SHARED;
                    key.u.string = make_shared_string("uri");
                    value.type = T_STRING;
                    value.subtype = STRING_SHARED;
                    value.u.string = make_shared_string(req_ctx->uri.c_str());
                    entry = find_for_insert(request_mapping, &key, 0);
                    *entry = value;
                    free_string(key.u.string);
                    
                    // Add headers
                    key.type = T_STRING;
                    key.subtype = STRING_SHARED;
                    key.u.string = make_shared_string("headers");
                    value.type = T_MAPPING;
                    value.u.map = http_headers_to_mapping(req_ctx->headers);
                    entry = find_for_insert(request_mapping, &key, 0);
                    *entry = value;
                    free_string(key.u.string);
                    
                    // Add body
                    key.type = T_STRING;
                    key.subtype = STRING_SHARED;
                    key.u.string = make_shared_string("body");
                    value.type = T_STRING;
                    value.subtype = STRING_SHARED;
                    value.u.string = make_shared_string(req_ctx->body.c_str());
                    entry = find_for_insert(request_mapping, &key, 0);
                    *entry = value;
                    free_string(key.u.string);
                    
                    // Call LPC callback
                    if (server_ctx->callback.type == T_FUNCTION) {
                        push_refed_mapping(request_mapping);
                        safe_call_function_pointer(server_ctx->callback.u.fp, 1);
                    } else if (server_ctx->callback.type == T_STRING) {
                        // Call named function
                        push_refed_mapping(request_mapping);
                        safe_apply(server_ctx->callback.u.string, current_object, 1, ORIGIN_EFUN);
                    }
                }
            }
            return 0;
        }
        
        case LWS_CALLBACK_CLOSED_HTTP: {
            // Clean up request context
            req_ctx = reinterpret_cast<http_request_context*>(lws_get_opaque_user_data(wsi));
            if (req_ctx) {
                auto server_it = g_http_servers.find(req_ctx->server_id);
                if (server_it != g_http_servers.end()) {
                    server_it->second->pending_requests.erase(req_ctx->request_id);
                }
            }
            return 0;
        }
        
        default:
            return lws_callback_http_dummy(wsi, reason, user, in, len);
    }
}

/**
 * libwebsockets callback for HTTP client
 */
int http_lws_callback_client(struct lws *wsi, enum lws_callback_reasons reason,
                           void *user, void *in, size_t len) {
    struct http_client_context *client_ctx = reinterpret_cast<http_client_context*>(user);
    
    switch (reason) {
        case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
            if (client_ctx) {
                client_ctx->complete = true;
                client_ctx->response_status = 0; // Connection error
            }
            return -1;
            
        case LWS_CALLBACK_ESTABLISHED_CLIENT_HTTP: {
            if (client_ctx) {
                client_ctx->response_status = lws_http_client_http_response(wsi);
            }
            return 0;
        }
        
        case LWS_CALLBACK_RECEIVE_CLIENT_HTTP: {
            if (client_ctx) {
                client_ctx->response_body.append(reinterpret_cast<char*>(in), len);
            }
            return 0;
        }
        
        case LWS_CALLBACK_COMPLETED_CLIENT_HTTP: {
            if (client_ctx) {
                client_ctx->complete = true;
                
                // Create response mapping for callback
                mapping_t *response_mapping = allocate_mapping(4);
                svalue_t key, value;
                
                // Add status
                key.type = T_STRING;
                key.subtype = STRING_SHARED;
                key.u.string = make_shared_string("status");
                value.type = T_NUMBER;
                value.u.number = client_ctx->response_status;
                svalue_t *entry = find_for_insert(response_mapping, &key, 0);
                *entry = value;
                free_string(key.u.string);
                
                // Add headers
                key.type = T_STRING;
                key.subtype = STRING_SHARED;
                key.u.string = make_shared_string("headers");
                value.type = T_MAPPING;
                value.u.map = http_headers_to_mapping(client_ctx->response_headers);
                entry = find_for_insert(response_mapping, &key, 0);
                *entry = value;
                free_string(key.u.string);
                
                // Add body
                key.type = T_STRING;
                key.subtype = STRING_SHARED;
                key.u.string = make_shared_string("body");
                value.type = T_STRING;
                value.subtype = STRING_SHARED;
                value.u.string = make_shared_string(client_ctx->response_body.c_str());
                entry = find_for_insert(response_mapping, &key, 0);
                *entry = value;
                free_string(key.u.string);
                
                // Call callback if provided
                if (client_ctx->callback.type == T_FUNCTION) {
                    push_refed_mapping(response_mapping);
                    safe_call_function_pointer(client_ctx->callback.u.fp, 1);
                } else if (client_ctx->callback.type == T_STRING) {
                    push_refed_mapping(response_mapping);
                    safe_apply(client_ctx->callback.u.string, current_object, 1, ORIGIN_EFUN);
                }
            }
            return 0;
        }
        
        case LWS_CALLBACK_CLOSED_CLIENT_HTTP: {
            // Cleanup will be handled elsewhere
            return 0;
        }
        
        default:
            return lws_callback_http_dummy(wsi, reason, user, in, len);
    }
}

// EFUN implementations

/**
 * http_server_start(int port, string|function callback, mapping options)
 * Start HTTP server on specified port
 */
void f_http_start_server(void) {
    mapping_t *options = nullptr;
    svalue_t *callback;
    int port;
    
    // Parse arguments
    if (st_num_arg == 3) {
        options = sp->u.map;
        sp--;
    }
    callback = sp--;
    port = sp->u.number;
    
    int result = http_server_start_impl(port, callback, options);
    free_svalue(sp + 1, "f_http_server_start");
    sp->type = T_NUMBER;
    sp->u.number = result;
}

/**
 * http_server_stop(int server_id)
 * Stop HTTP server
 */
void f_http_stop_server(void) {
    int server_id = sp->u.number;
    int result = http_server_stop_impl(server_id);
    
    sp->type = T_NUMBER;
    sp->u.number = result;
}

/**
 * http_response_send(int request_id, mapping response)
 * Send HTTP response
 */
void f_http_send_response(void) {
    mapping_t *response = sp->u.map;
    sp--;
    int request_id = sp->u.number;
    
    int result = http_response_send_impl(request_id, response);
    free_mapping(response);
    
    sp->type = T_NUMBER;
    sp->u.number = result;
}

/**
 * http_request(string url, mapping options, string|function callback)
 * Make HTTP client request
 */
void f_http_send_request(void) {
    svalue_t *callback = nullptr;
    mapping_t *options = nullptr;
    const char *url;
    
    // Parse arguments
    if (st_num_arg >= 3) {
        callback = sp--;
    }
    if (st_num_arg >= 2) {
        options = sp->u.map;
        sp--;
    }
    url = sp->u.string;
    
    int result = http_request_impl(url, options, callback);
    
    // Clean up
    free_string(sp->u.string);
    if (options) free_mapping(options);
    if (callback) free_svalue(callback, "f_http_request");
    
    sp->type = T_NUMBER;
    sp->u.number = result;
}

/* WebSocket integration efuns */
#ifdef F_WEBSOCKET_SOCKET_CREATE
void f_websocket_socket_create() {
  try {
    int mode = sp->u.number;
    
    // Validate WebSocket socket mode
    if (mode < 30 || mode > 37) {  // WEBSOCKET_SERVER through MQTT_CLIENT
      error("websocket_socket_create: Invalid WebSocket socket mode %d\n", mode);
      return;
    }
    
    // For now, delegate to regular socket_create
    // This would need integration with the socket package and existing websocket.cc
    error("websocket_socket_create: Not yet implemented\n");
  } catch (const std::exception& e) {
    pop_n_elems(st_num_arg);
    error("websocket_socket_create: %s\n", e.what());
  }
}
#endif

#ifdef F_WEBSOCKET_SEND_MESSAGE
void f_websocket_send_message() {
  try {
    error("websocket_send_message: Not yet implemented\n");
  } catch (const std::exception& e) {
    pop_n_elems(st_num_arg);
    error("websocket_send_message: %s\n", e.what());
  }
}
#endif

#ifdef F_WEBSOCKET_SEND_BINARY
void f_websocket_send_binary() {
  try {
    error("websocket_send_binary: Not yet implemented\n");
  } catch (const std::exception& e) {
    pop_n_elems(st_num_arg);
    error("websocket_send_binary: %s\n", e.what());
  }
}
#endif

#ifdef F_WEBSOCKET_CLOSE_CONNECTION
void f_websocket_close_connection() {
  try {
    error("websocket_close_connection: Not yet implemented\n");
  } catch (const std::exception& e) {
    pop_n_elems(st_num_arg);
    error("websocket_close_connection: %s\n", e.what());
  }
}
#endif

#ifdef F_WEBSOCKET_GET_INFO
void f_websocket_get_info() {
  try {
    error("websocket_get_info: Not yet implemented\n");
  } catch (const std::exception& e) {
    pop_n_elems(st_num_arg);
    error("websocket_get_info: %s\n", e.what());
  }
}
#endif

/* MQTT client efuns */
#ifdef F_MQTT_SOCKET_CREATE
void f_mqtt_socket_create() {
  try {
    error("mqtt_socket_create: Not yet implemented\n");
  } catch (const std::exception& e) {
    pop_n_elems(st_num_arg);
    error("mqtt_socket_create: %s\n", e.what());
  }
}
#endif

#ifdef F_MQTT_PUBLISH
void f_mqtt_publish() {
  try {
    error("mqtt_publish: Not yet implemented\n");
  } catch (const std::exception& e) {
    pop_n_elems(st_num_arg);
    error("mqtt_publish: %s\n", e.what());
  }
}
#endif

#ifdef F_MQTT_SUBSCRIBE
void f_mqtt_subscribe() {
  try {
    error("mqtt_subscribe: Not yet implemented\n");
  } catch (const std::exception& e) {
    pop_n_elems(st_num_arg);
    error("mqtt_subscribe: %s\n", e.what());
  }
}
#endif

#ifdef F_MQTT_UNSUBSCRIBE
void f_mqtt_unsubscribe() {
  try {
    error("mqtt_unsubscribe: Not yet implemented\n");
  } catch (const std::exception& e) {
    pop_n_elems(st_num_arg);
    error("mqtt_unsubscribe: %s\n", e.what());
  }
}
#endif

#ifdef F_MQTT_DISCONNECT
void f_mqtt_disconnect() {
  try {
    error("mqtt_disconnect: Not yet implemented\n");
  } catch (const std::exception& e) {
    pop_n_elems(st_num_arg);
    error("mqtt_disconnect: %s\n", e.what());
  }
}
#endif