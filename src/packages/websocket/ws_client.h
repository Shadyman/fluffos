#ifndef PACKAGES_WEBSOCKET_WS_CLIENT_H_
#define PACKAGES_WEBSOCKET_WS_CLIENT_H_

/*
 * WebSocket Client Implementation
 * 
 * Client-side WebSocket functionality using libwebsockets
 * with FluffOS unified socket architecture integration.
 */

#include "packages/websocket/websocket.h"
#include <libwebsockets.h>
#include <string>
#include <vector>
#include <memory>

/*
 * WebSocket Client Configuration
 */
struct ws_client_config {
    std::string url;
    std::string protocol;
    std::vector<std::string> subprotocols;
    std::vector<std::string> extensions;
    
    // Connection settings
    int connect_timeout;
    int ping_interval;
    int pong_timeout;
    size_t max_message_size;
    
    // TLS settings
    bool verify_ssl;
    std::string ca_file;
    std::string cert_file;
    std::string key_file;
    std::string ssl_cipher_list;
    
    // Authentication
    std::string username;
    std::string password;
    mapping_t* custom_headers;
    
    // Proxy settings
    std::string proxy_host;
    int proxy_port;
    std::string proxy_username;
    std::string proxy_password;
    
    // Advanced options
    bool follow_redirects;
    int max_redirects;
    bool enable_compression;
    std::string user_agent;
    std::string origin;
    
    ws_client_config() : connect_timeout(30), ping_interval(30), pong_timeout(10),
                        max_message_size(64 * 1024), verify_ssl(true), 
                        custom_headers(nullptr), proxy_port(0), follow_redirects(true),
                        max_redirects(5), enable_compression(true),
                        user_agent("FluffOS-WebSocket/1.0") {
        // Default subprotocols
        subprotocols.push_back("chat");
        subprotocols.push_back("echo");
        
        // Default extensions
        extensions.push_back("permessage-deflate");
    }
    
    ~ws_client_config() {
        if (custom_headers) {
            free_mapping(custom_headers);
        }
    }
};

/*
 * WebSocket Client State
 */
enum ws_client_state {
    WS_CLIENT_DISCONNECTED = 0,
    WS_CLIENT_CONNECTING = 1,
    WS_CLIENT_CONNECTED = 2,
    WS_CLIENT_CLOSING = 3,
    WS_CLIENT_ERROR = 4
};

/*
 * WebSocket Client Implementation
 */
class WebSocketClient {
private:
    std::shared_ptr<ws_connection_context> context_;
    ws_client_config config_;
    ws_client_state state_;
    
    // libwebsockets context
    struct lws_context* lws_context_;
    struct lws* wsi_;
    
    // Connection tracking
    std::string resolved_url_;
    std::string negotiated_protocol_;
    std::vector<std::string> negotiated_extensions_;
    
    // Error handling
    std::string last_error_;
    int last_error_code_;
    
    // Statistics
    time_t connect_time_;
    time_t last_activity_;
    
public:
    WebSocketClient();
    ~WebSocketClient();
    
    // Connection lifecycle
    bool connect(const ws_client_config& config);
    bool disconnect(int close_code = WS_CLOSE_NORMAL, const std::string& reason = "");
    bool is_connected() const { return state_ == WS_CLIENT_CONNECTED; }
    ws_client_state get_state() const { return state_; }
    
    // Message operations
    bool send_text(const std::string& message);
    bool send_binary(const std::vector<uint8_t>& data);
    bool send_ping(const std::string& payload = "");
    bool send_pong(const std::string& payload = "");
    
    // Configuration
    const ws_client_config& get_config() const { return config_; }
    void set_config(const ws_client_config& config) { config_ = config; }
    
    // Connection info
    mapping_t* get_connection_info();
    mapping_t* get_connection_stats();
    std::string get_negotiated_protocol() const { return negotiated_protocol_; }
    std::vector<std::string> get_negotiated_extensions() const { return negotiated_extensions_; }
    
    // Error handling
    std::string get_last_error() const { return last_error_; }
    int get_last_error_code() const { return last_error_code_; }
    
    // libwebsockets callback integration
    int handle_lws_callback(struct lws* wsi, enum lws_callback_reasons reason,
                           void* user, void* in, size_t len);
    
    // Context access
    std::shared_ptr<ws_connection_context> get_context() { return context_; }
    struct lws* get_wsi() { return wsi_; }
    
private:
    // Initialization
    bool initialize_context();
    bool setup_connection_info();
    bool create_connection();
    
    // Protocol negotiation
    void handle_protocol_negotiation(struct lws* wsi);
    void handle_extension_negotiation(struct lws* wsi);
    
    // Message handling
    void handle_message(const uint8_t* data, size_t len, bool is_binary, bool is_final);
    void handle_ping(const std::string& payload);
    void handle_pong(const std::string& payload);
    void handle_close(int close_code, const std::string& reason);
    
    // Connection state management
    void set_state(ws_client_state new_state);
    void handle_connection_established();
    void handle_connection_error(const std::string& error, int error_code = 0);
    void handle_connection_closed();
    
    // URL parsing and validation
    bool parse_url(const std::string& url, std::string& host, int& port, 
                  std::string& path, bool& use_ssl);
    bool validate_url(const std::string& url);
    
    // Header management
    bool add_custom_headers(struct lws* wsi);
    mapping_t* parse_response_headers(struct lws* wsi);
    
    // TLS/SSL handling
    bool setup_ssl_info();
    bool verify_ssl_certificate(struct lws* wsi);
    
    // Proxy handling
    bool setup_proxy_info();
    
    // Utility functions
    std::string get_websocket_key();
    bool validate_websocket_accept(const std::string& accept, const std::string& key);
    void update_activity_time();
    
    // Error management
    void set_error(const std::string& error, int error_code = 0);
    void clear_error();
    
    // Memory management
    void cleanup();
    void cleanup_context();
};

/*
 * Global WebSocket Client Management
 */
class WebSocketClientManager {
private:
    static WebSocketClientManager* instance_;
    std::unordered_map<int, std::unique_ptr<WebSocketClient>> clients_;
    int next_client_id_;
    
public:
    static WebSocketClientManager* getInstance();
    
    WebSocketClientManager();
    ~WebSocketClientManager();
    
    // Client management
    int create_client(const ws_client_config& config);
    bool connect_client(int client_id);
    bool disconnect_client(int client_id, int close_code = WS_CLOSE_NORMAL, 
                          const std::string& reason = "");
    bool remove_client(int client_id);
    
    // Client access
    WebSocketClient* get_client(int client_id);
    std::vector<int> get_client_ids() const;
    
    // Message operations
    bool send_text(int client_id, const std::string& message);
    bool send_binary(int client_id, const std::vector<uint8_t>& data);
    bool send_ping(int client_id, const std::string& payload = "");
    bool send_pong(int client_id, const std::string& payload = "");
    
    // Global operations
    void disconnect_all_clients();
    mapping_t* get_all_client_stats();
    
    // Connection state queries
    std::vector<int> get_connected_clients() const;
    std::vector<int> get_connecting_clients() const;
    
private:
    int allocate_client_id();
    void cleanup_clients();
    void cleanup_disconnected_clients();
};

/*
 * Utility Functions
 */

// Convert mapping to client config
bool mapping_to_client_config(const mapping_t* options, ws_client_config& config);

// Convert client config to mapping  
mapping_t* client_config_to_mapping(const ws_client_config& config);

// Validate client configuration
bool validate_client_config(const ws_client_config& config, std::string& error);

// Get default client configuration
ws_client_config get_default_client_config();

// Parse WebSocket URL
bool parse_websocket_url(const std::string& url, std::string& host, int& port,
                        std::string& path, bool& use_ssl);

// URL validation
bool is_valid_websocket_url(const std::string& url);

// Protocol validation
bool is_valid_subprotocol(const std::string& protocol);

#endif  // PACKAGES_WEBSOCKET_WS_CLIENT_H_