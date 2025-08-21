#ifndef PACKAGES_WEBSOCKET_WS_SERVER_H_
#define PACKAGES_WEBSOCKET_WS_SERVER_H_

/*
 * WebSocket Server Implementation
 * 
 * Server-side WebSocket functionality using libwebsockets
 * with FluffOS unified socket architecture integration.
 */

#include "packages/websocket/websocket.h"
#include <libwebsockets.h>
#include <string>
#include <vector>
#include <memory>

/*
 * WebSocket Server Configuration
 */
struct ws_server_config {
    std::string bind_address;
    int port;
    bool tls_enabled;
    std::string cert_file;
    std::string key_file;
    std::string ca_file;
    
    // Protocol configuration
    std::vector<std::string> supported_protocols;
    std::vector<std::string> supported_extensions;
    
    // Connection limits
    size_t max_connections;
    size_t max_message_size;
    int connection_timeout;
    int ping_interval;
    int pong_timeout;
    
    // Security settings
    std::vector<std::string> allowed_origins;
    bool require_origin;
    bool validate_utf8;
    
    ws_server_config() : port(0), tls_enabled(false), max_connections(1000),
                        max_message_size(64 * 1024), connection_timeout(60),
                        ping_interval(30), pong_timeout(10), require_origin(false),
                        validate_utf8(true) {
        // Default supported protocols
        supported_protocols.push_back("chat");
        supported_protocols.push_back("echo");
        
        // Default supported extensions
        supported_extensions.push_back("permessage-deflate");
    }
};

/*
 * WebSocket Server Implementation
 */
class WebSocketServer {
private:
    std::shared_ptr<ws_server_context> context_;
    ws_server_config config_;
    bool running_;
    
    // libwebsockets protocol list
    std::vector<struct lws_protocols> protocols_;
    std::vector<struct lws_extension> extensions_;
    
    // Connection tracking
    std::unordered_map<struct lws*, int> wsi_to_connection_id_;
    
public:
    WebSocketServer();
    ~WebSocketServer();
    
    // Server lifecycle
    bool initialize(const ws_server_config& config);
    bool start();
    bool stop();
    bool is_running() const { return running_; }
    
    // Configuration
    const ws_server_config& get_config() const { return config_; }
    void set_config(const ws_server_config& config) { config_ = config; }
    
    // Connection management
    bool accept_connection(struct lws* wsi);
    void close_connection(int connection_id, int close_code, const std::string& reason);
    void handle_connection_close(struct lws* wsi);
    
    // Message handling
    void handle_message(struct lws* wsi, const uint8_t* data, size_t len, 
                       bool is_binary, bool is_final);
    bool send_message(int connection_id, const uint8_t* data, size_t len, 
                     bool is_binary);
    
    // Protocol negotiation
    bool negotiate_subprotocol(struct lws* wsi, const std::string& requested);
    bool negotiate_extensions(struct lws* wsi, const std::string& requested);
    
    // Security validation
    bool validate_origin(struct lws* wsi, const std::string& origin);
    bool validate_handshake(struct lws* wsi);
    
    // Statistics and monitoring
    mapping_t* get_server_stats();
    size_t get_connection_count() const;
    std::vector<int> get_connection_ids() const;
    
    // libwebsockets callback integration
    int handle_lws_callback(struct lws* wsi, enum lws_callback_reasons reason,
                           void* user, void* in, size_t len);
    
    // Context access
    std::shared_ptr<ws_server_context> get_context() { return context_; }
    struct lws_context* get_lws_context() { return context_ ? context_->context : nullptr; }
    
private:
    // Initialization helpers
    bool setup_protocols();
    bool setup_extensions();
    bool setup_lws_context();
    bool setup_vhost();
    
    // Protocol callback handlers
    static int protocol_callback_http(struct lws* wsi, 
                                    enum lws_callback_reasons reason,
                                    void* user, void* in, size_t len);
    static int protocol_callback_websocket(struct lws* wsi,
                                         enum lws_callback_reasons reason,
                                         void* user, void* in, size_t len);
    
    // Connection helpers
    int register_connection(struct lws* wsi);
    void unregister_connection(struct lws* wsi);
    std::shared_ptr<ws_connection_context> get_connection_by_wsi(struct lws* wsi);
    
    // Message processing
    void process_text_message(struct lws* wsi, const std::string& message);
    void process_binary_message(struct lws* wsi, const std::vector<uint8_t>& data);
    void process_ping(struct lws* wsi, const std::string& payload);
    void process_pong(struct lws* wsi, const std::string& payload);
    
    // Error handling
    void handle_protocol_error(struct lws* wsi, const std::string& error);
    void handle_connection_error(struct lws* wsi, const std::string& error);
    
    // Utility functions
    bool is_valid_protocol(const std::string& protocol) const;
    bool is_valid_extension(const std::string& extension) const;
    std::string get_client_ip(struct lws* wsi);
    std::string get_request_uri(struct lws* wsi);
    
    // TLS/SSL helpers
    bool setup_ssl_context();
    bool load_certificates();
    
    // Configuration validation
    bool validate_config() const;
    void apply_config_to_context();
    
    // Memory management
    void cleanup_connections();
    void cleanup_protocols();
    void cleanup_extensions();
};

/*
 * Global WebSocket Server Management
 */
class WebSocketServerManager {
private:
    static WebSocketServerManager* instance_;
    std::unordered_map<int, std::unique_ptr<WebSocketServer>> servers_;
    int next_server_id_;
    
public:
    static WebSocketServerManager* getInstance();
    
    WebSocketServerManager();
    ~WebSocketServerManager();
    
    // Server management
    int create_server(const ws_server_config& config);
    bool start_server(int server_id);
    bool stop_server(int server_id);
    bool remove_server(int server_id);
    
    // Server access
    WebSocketServer* get_server(int server_id);
    std::vector<int> get_server_ids() const;
    
    // Global operations
    void shutdown_all_servers();
    mapping_t* get_all_server_stats();
    
private:
    int allocate_server_id();
    void cleanup_servers();
};

/*
 * Utility Functions
 */

// Convert mapping to server config
bool mapping_to_server_config(const mapping_t* options, ws_server_config& config);

// Convert server config to mapping
mapping_t* server_config_to_mapping(const ws_server_config& config);

// Validate server configuration
bool validate_server_config(const ws_server_config& config, std::string& error);

// Get default server configuration
ws_server_config get_default_server_config();

#endif  // PACKAGES_WEBSOCKET_WS_SERVER_H_