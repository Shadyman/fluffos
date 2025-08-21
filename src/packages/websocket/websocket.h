#ifndef PACKAGES_WEBSOCKET_H_
#define PACKAGES_WEBSOCKET_H_

/*
 * WebSocket Package Header
 * 
 * FluffOS WebSocket implementation using libwebsockets integration
 * for unified socket architecture support.
 */

#include "base/package_api.h"
#include "packages/sockets/socket_options.h"
#include "packages/sockets/socket_option_manager.h"

#include <libwebsockets.h>
#include <memory>
#include <unordered_map>
#include <string>
#include <vector>

/*
 * WebSocket Constants and Types
 */

// WebSocket Protocol States
enum ws_connection_state {
    WS_STATE_CONNECTING = 0,
    WS_STATE_OPEN = 1,
    WS_STATE_CLOSING = 2,
    WS_STATE_CLOSED = 3
};

// WebSocket Frame Opcodes
enum ws_frame_opcode {
    WS_OPCODE_CONTINUATION = 0x0,
    WS_OPCODE_TEXT = 0x1,
    WS_OPCODE_BINARY = 0x2,
    WS_OPCODE_CLOSE = 0x8,
    WS_OPCODE_PING = 0x9,
    WS_OPCODE_PONG = 0xA
};

// WebSocket Close Codes
enum ws_close_code {
    WS_CLOSE_NORMAL = 1000,
    WS_CLOSE_GOING_AWAY = 1001,
    WS_CLOSE_PROTOCOL_ERROR = 1002,
    WS_CLOSE_UNSUPPORTED = 1003,
    WS_CLOSE_NO_STATUS = 1005,
    WS_CLOSE_ABNORMAL = 1006,
    WS_CLOSE_INVALID_DATA = 1007,
    WS_CLOSE_POLICY_VIOLATION = 1008,
    WS_CLOSE_TOO_LARGE = 1009,
    WS_CLOSE_EXTENSION_REQUIRED = 1010,
    WS_CLOSE_UNEXPECTED = 1011
};

/*
 * WebSocket Connection Context
 */
struct ws_connection_context {
    int socket_fd;
    int lpc_socket_id;
    ws_connection_state state;
    object_t* owner_object;
    
    // Protocol info
    std::string subprotocol;
    std::vector<std::string> extensions;
    bool compression_enabled;
    
    // Connection statistics
    uint64_t messages_sent;
    uint64_t messages_received;
    uint64_t bytes_sent;
    uint64_t bytes_received;
    time_t connected_at;
    time_t last_ping;
    
    // Buffer management
    std::vector<uint8_t> receive_buffer;
    std::vector<uint8_t> send_buffer;
    
    // libwebsockets context
    struct lws* wsi;
    struct lws_context* context;
    
    // Socket options via unified architecture
    std::unique_ptr<SocketOptionManager> option_manager;
    
    ws_connection_context() : socket_fd(-1), lpc_socket_id(-1), 
                              state(WS_STATE_CLOSED), owner_object(nullptr),
                              compression_enabled(false), messages_sent(0),
                              messages_received(0), bytes_sent(0), bytes_received(0),
                              connected_at(0), last_ping(0), wsi(nullptr), 
                              context(nullptr) {}
};

/*
 * WebSocket Server Context
 */
struct ws_server_context {
    struct lws_context* context;
    struct lws_vhost* vhost;
    std::string bind_address;
    int port;
    bool tls_enabled;
    
    // Server configuration
    std::vector<std::string> supported_protocols;
    std::vector<std::string> supported_extensions;
    size_t max_connections;
    size_t max_message_size;
    
    // Active connections
    std::unordered_map<int, std::shared_ptr<ws_connection_context>> connections;
    
    // Owner object
    object_t* owner_object;
    
    ws_server_context() : context(nullptr), vhost(nullptr), port(0),
                         tls_enabled(false), max_connections(1000),
                         max_message_size(64 * 1024), owner_object(nullptr) {}
};

/*
 * WebSocket Frame Structure
 */
struct ws_frame {
    bool fin;
    bool rsv1, rsv2, rsv3;
    ws_frame_opcode opcode;
    bool masked;
    uint64_t payload_length;
    uint32_t mask_key;
    std::vector<uint8_t> payload;
    
    ws_frame() : fin(true), rsv1(false), rsv2(false), rsv3(false),
                 opcode(WS_OPCODE_TEXT), masked(false), payload_length(0),
                 mask_key(0) {}
};

/*
 * WebSocket Package Manager
 */
class WebSocketManager {
private:
    static WebSocketManager* instance_;
    std::unordered_map<int, std::shared_ptr<ws_connection_context>> connections_;
    std::unordered_map<int, std::shared_ptr<ws_server_context>> servers_;
    int next_connection_id_;
    int next_server_id_;
    
    // libwebsockets global initialization
    bool lws_initialized_;
    
public:
    static WebSocketManager* getInstance();
    
    WebSocketManager();
    ~WebSocketManager();
    
    // Server Management
    int create_server(const std::string& address, int port, 
                     const mapping_t* options = nullptr);
    bool bind_server(int server_id, const std::string& address, int port);
    bool close_server(int server_id);
    
    // Client Management
    int create_connection(const std::string& url, 
                         const mapping_t* options = nullptr);
    bool close_connection(int connection_id, int close_code = WS_CLOSE_NORMAL,
                         const std::string& reason = "");
    
    // Message Operations
    bool send_text(int connection_id, const std::string& message);
    bool send_binary(int connection_id, const std::vector<uint8_t>& data);
    bool send_ping(int connection_id, const std::string& payload = "");
    bool send_pong(int connection_id, const std::string& payload = "");
    
    // Connection State
    ws_connection_state get_connection_state(int connection_id);
    mapping_t* get_connection_info(int connection_id);
    std::shared_ptr<ws_connection_context> get_connection(int connection_id);
    std::shared_ptr<ws_server_context> get_server(int server_id);
    
    // Protocol Operations
    bool set_subprotocol(int connection_id, const std::string& protocol);
    std::string get_subprotocol(int connection_id);
    bool negotiate_extensions(int connection_id, 
                            const std::vector<std::string>& extensions);
    
    // Statistics
    mapping_t* get_connection_stats(int connection_id);
    void reset_connection_stats(int connection_id);
    array_t* list_connections();
    
    // Frame Processing
    bool parse_frame(const std::vector<uint8_t>& frame_data, ws_frame& frame);
    std::vector<uint8_t> build_frame(ws_frame_opcode opcode, 
                                   const std::vector<uint8_t>& payload,
                                   bool mask = false);
    
    // Validation and Security
    bool validate_frame(const std::vector<uint8_t>& frame_data);
    bool check_origin(int connection_id, const std::string& origin);
    std::string generate_websocket_key();
    std::string compute_websocket_accept(const std::string& key);
    
    // Cleanup
    void cleanup_closed_connections();
    void shutdown();
    
private:
    bool initialize_libwebsockets();
    void cleanup_libwebsockets();
    
    // libwebsockets callback handling
    static int lws_callback_handler(struct lws* wsi, 
                                  enum lws_callback_reasons reason,
                                  void* user, void* in, size_t len);
    
    // Connection management helpers
    int allocate_connection_id();
    int allocate_server_id();
    void remove_connection(int connection_id);
    void remove_server(int server_id);
    
    // Option management integration
    void apply_connection_options(std::shared_ptr<ws_connection_context> conn,
                                const mapping_t* options);
    void apply_server_options(std::shared_ptr<ws_server_context> server,
                            const mapping_t* options);
    
    // Frame masking/unmasking
    void mask_payload(std::vector<uint8_t>& payload, uint32_t mask_key);
    void unmask_payload(std::vector<uint8_t>& payload, uint32_t mask_key);
    
    // Utility functions
    std::vector<uint8_t> string_to_bytes(const std::string& str);
    std::string bytes_to_string(const std::vector<uint8_t>& bytes);
    uint64_t parse_extended_length(const uint8_t* data, size_t len);
    size_t write_extended_length(uint8_t* data, uint64_t length);
};

/*
 * Global WebSocket Functions - exported to LPC
 */

// Server functions
svalue_t f_websocket_create_server(int num_arg, svalue_t* args);
svalue_t f_websocket_bind_server(int num_arg, svalue_t* args);
svalue_t f_websocket_close_server(int num_arg, svalue_t* args);

// Client functions
svalue_t f_websocket_connect(int num_arg, svalue_t* args);
svalue_t f_websocket_client_handshake(int num_arg, svalue_t* args);

// Message operations
svalue_t f_websocket_send_text(int num_arg, svalue_t* args);
svalue_t f_websocket_send_binary(int num_arg, svalue_t* args);
svalue_t f_websocket_send_ping(int num_arg, svalue_t* args);
svalue_t f_websocket_send_pong(int num_arg, svalue_t* args);

// Frame operations
svalue_t f_websocket_parse_frame(int num_arg, svalue_t* args);
svalue_t f_websocket_build_frame(int num_arg, svalue_t* args);

// Connection management
svalue_t f_websocket_upgrade_connection(int num_arg, svalue_t* args);
svalue_t f_websocket_close_connection(int num_arg, svalue_t* args);
svalue_t f_websocket_get_connection_info(int num_arg, svalue_t* args);

// Protocol operations
svalue_t f_websocket_set_subprotocol(int num_arg, svalue_t* args);
svalue_t f_websocket_get_subprotocol(int num_arg, svalue_t* args);
svalue_t f_websocket_negotiate_extensions(int num_arg, svalue_t* args);
svalue_t f_websocket_get_extensions(int num_arg, svalue_t* args);

// Validation and security
svalue_t f_websocket_validate_frame(int num_arg, svalue_t* args);
svalue_t f_websocket_check_origin(int num_arg, svalue_t* args);
svalue_t f_websocket_generate_key(int num_arg, svalue_t* args);
svalue_t f_websocket_compute_accept(int num_arg, svalue_t* args);

// State management
svalue_t f_websocket_get_state(int num_arg, svalue_t* args);
svalue_t f_websocket_set_ping_interval(int num_arg, svalue_t* args);
svalue_t f_websocket_get_ping_interval(int num_arg, svalue_t* args);
svalue_t f_websocket_set_max_message_size(int num_arg, svalue_t* args);

// Compression support
svalue_t f_websocket_enable_compression(int num_arg, svalue_t* args);
svalue_t f_websocket_disable_compression(int num_arg, svalue_t* args);
svalue_t f_websocket_is_compression_enabled(int num_arg, svalue_t* args);

// Statistics and monitoring
svalue_t f_websocket_get_stats(int num_arg, svalue_t* args);
svalue_t f_websocket_reset_stats(int num_arg, svalue_t* args);
svalue_t f_websocket_list_connections(int num_arg, svalue_t* args);

#endif  // PACKAGES_WEBSOCKET_H_