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

#ifdef PACKAGE_SOCKETS
#include "packages/sockets/socket_efuns.h"
// Forward declarations for socket integration
static int http_socket_create_handler(enum socket_mode mode, svalue_t *read_callback, svalue_t *close_callback);
static int websocket_socket_create_handler(enum socket_mode mode, svalue_t *read_callback, svalue_t *close_callback);
static void init_http_socket_handlers();
#endif

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
#ifdef PACKAGE_SOCKETS
    // Initialize handlers if not done already
    init_http_socket_handlers();
#endif
    
    int mode = sp->u.number;
    
    // Validate WebSocket socket mode
    if (mode < 30 || mode > 37) {  // WEBSOCKET_SERVER through MQTT_CLIENT
      error("websocket_socket_create: Invalid WebSocket socket mode %d\n", mode);
      return;
    }
    
    // Get the callback parameters
    svalue_t *close_callback = (st_num_arg >= 3) ? sp - 1 : nullptr;
    svalue_t *read_callback = sp - (st_num_arg >= 3 ? 2 : 1);
    
#ifdef PACKAGE_SOCKETS
    // Call the WebSocket handler directly
    int result = websocket_socket_create_handler(static_cast<enum socket_mode>(mode), read_callback, close_callback);
    
    // Clean up stack and return result
    pop_n_elems(st_num_arg);
    push_number(result);
#else
    pop_n_elems(st_num_arg);
    error("websocket_socket_create: PACKAGE_SOCKETS not available\n");
#endif
    
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
#ifdef PACKAGE_SOCKETS
    // Initialize handlers if not done already
    init_http_socket_handlers();
#endif
    
    // Get broker parameter
    const char *broker = sp->u.string;
    svalue_t *close_callback = (st_num_arg >= 3) ? sp - 1 : nullptr;
    svalue_t *read_callback = sp - (st_num_arg >= 3 ? 2 : 1);
    
#ifdef PACKAGE_SOCKETS
    // Create MQTT client socket (mode 37)
    int result = websocket_socket_create_handler(MQTT_CLIENT, read_callback, close_callback);
    
    if (result >= 0) {
      // Store broker information in the client context
      auto client_it = g_http_clients.find(result);
      if (client_it != g_http_clients.end()) {
        client_it->second->url = broker;
      }
    }
    
    // Clean up stack and return result
    pop_n_elems(st_num_arg);
    push_number(result);
#else
    pop_n_elems(st_num_arg);
    error("mqtt_socket_create: PACKAGE_SOCKETS not available\n");
#endif
    
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

/* HTTP Socket Integration */

#ifdef PACKAGE_SOCKETS

// HTTP socket handlers
static int http_socket_create_handler(enum socket_mode mode, svalue_t *read_callback, svalue_t *close_callback) {
  try {
    // Create HTTP context based on mode
    switch (mode) {
      case HTTP_SERVER:
      case HTTPS_SERVER: {
        // Create HTTP server using existing infrastructure
        // For now, create a placeholder socket that will be handled by http_start_server
        int virtual_fd = g_next_server_id++;
        
        // Store callback for later use
        auto server_ctx = std::make_shared<http_server_context>();
        server_ctx->server_id = virtual_fd;
        server_ctx->callback = *read_callback;
        server_ctx->active = false;  // Not started yet
        g_http_servers[virtual_fd] = server_ctx;
        
        return virtual_fd;
      }
      
      case HTTP_CLIENT:
      case HTTPS_CLIENT: {
        // Create HTTP client context
        int virtual_fd = g_next_client_id++;
        
        // Store callback for later use
        auto client_ctx = std::make_shared<http_client_context>();
        client_ctx->request_id = virtual_fd;
        client_ctx->callback = *read_callback;
        client_ctx->complete = false;
        g_http_clients[virtual_fd] = client_ctx;
        
        return virtual_fd;
      }
      
      case REST_SERVER:
      case REST_CLIENT: {
        // REST modes use HTTP infrastructure with additional features
        // Delegate to HTTP modes for now
        enum socket_mode http_mode = (mode == REST_SERVER) ? HTTP_SERVER : HTTP_CLIENT;
        return http_socket_create_handler(http_mode, read_callback, close_callback);
      }
      
      default:
        return -1;  // Invalid mode
    }
  } catch (const std::exception& e) {
    // debug(http, "HTTP socket create error: %s\n", e.what());
    return -1;
  }
}

// WebSocket socket handlers
static int websocket_socket_create_handler(enum socket_mode mode, svalue_t *read_callback, svalue_t *close_callback) {
  try {
    // WebSocket modes use libwebsockets WebSocket functionality
    switch (mode) {
      case WEBSOCKET_SERVER:
      case WEBSOCKET_SECURE_SERVER: {
        // Create WebSocket server context
        int virtual_fd = g_next_server_id++;
        
        auto server_ctx = std::make_shared<http_server_context>();
        server_ctx->server_id = virtual_fd;
        server_ctx->callback = *read_callback;
        server_ctx->active = false;
        // Mark as WebSocket mode for different protocol handling
        g_http_servers[virtual_fd] = server_ctx;
        
        return virtual_fd;
      }
      
      case WEBSOCKET_CLIENT:
      case WEBSOCKET_SECURE_CLIENT:
      case WEBSOCKET_FILE_STREAM:
      case WEBSOCKET_BINARY_STREAM:
      case WEBSOCKET_COMPRESSED_NATIVE: {
        // Create WebSocket client context
        int virtual_fd = g_next_client_id++;
        
        auto client_ctx = std::make_shared<http_client_context>();
        client_ctx->request_id = virtual_fd;
        client_ctx->callback = *read_callback;
        client_ctx->complete = false;
        g_http_clients[virtual_fd] = client_ctx;
        
        return virtual_fd;
      }
      
      case MQTT_CLIENT: {
        // MQTT client using libwebsockets
        int virtual_fd = g_next_client_id++;
        
        auto client_ctx = std::make_shared<http_client_context>();
        client_ctx->request_id = virtual_fd;
        client_ctx->callback = *read_callback;
        client_ctx->method = "MQTT";  // Special marker
        client_ctx->complete = false;
        g_http_clients[virtual_fd] = client_ctx;
        
        return virtual_fd;
      }
      
      default:
        return -1;  // Invalid mode
    }
  } catch (const std::exception& e) {
    // debug(http, "WebSocket socket create error: %s\n", e.what());
    return -1;
  }
}

// Initialize HTTP/WebSocket socket handlers
static void init_http_socket_handlers() {
  static int initialized = 0;
  if (initialized) return;
  
  // Register handlers for HTTP modes (20-25)
  register_socket_create_handler(HTTP_SERVER, http_socket_create_handler);
  register_socket_create_handler(HTTPS_SERVER, http_socket_create_handler);
  register_socket_create_handler(HTTP_CLIENT, http_socket_create_handler);
  register_socket_create_handler(HTTPS_CLIENT, http_socket_create_handler);
  register_socket_create_handler(REST_SERVER, http_socket_create_handler);
  register_socket_create_handler(REST_CLIENT, http_socket_create_handler);
  
  // Register handlers for WebSocket modes (30-37)
  register_socket_create_handler(WEBSOCKET_SERVER, websocket_socket_create_handler);
  register_socket_create_handler(WEBSOCKET_CLIENT, websocket_socket_create_handler);
  register_socket_create_handler(WEBSOCKET_SECURE_SERVER, websocket_socket_create_handler);
  register_socket_create_handler(WEBSOCKET_SECURE_CLIENT, websocket_socket_create_handler);
  register_socket_create_handler(WEBSOCKET_FILE_STREAM, websocket_socket_create_handler);
  register_socket_create_handler(WEBSOCKET_BINARY_STREAM, websocket_socket_create_handler);
  register_socket_create_handler(WEBSOCKET_COMPRESSED_NATIVE, websocket_socket_create_handler);
  register_socket_create_handler(MQTT_CLIENT, websocket_socket_create_handler);
  
  initialized = 1;
}

#endif /* PACKAGE_SOCKETS */