/*
 * WebSocket Server Implementation
 * 
 * Server-side WebSocket functionality using libwebsockets
 * with FluffOS unified socket architecture integration.
 */

#include "packages/websocket/ws_server.h"
#include "packages/websocket/websocket.h"
#include "base/internal/log.h"
#include "vm/internal/simulate.h"
#include "vm/internal/base/mapping.h"

#include <libwebsockets.h>
#include <cstring>
#include <algorithm>

/*
 * WebSocket Server Implementation
 */

WebSocketServer::WebSocketServer() : running_(false) {
    context_ = std::make_shared<ws_server_context>();
}

WebSocketServer::~WebSocketServer() {
    if (running_) {
        stop();
    }
    cleanup_connections();
    cleanup_protocols();
    cleanup_extensions();
}

bool WebSocketServer::initialize(const ws_server_config& config) {
    config_ = config;
    
    if (!validate_config()) {
        debug(websocket, "WebSocket server config validation failed");
        return false;
    }
    
    context_->bind_address = config_.bind_address;
    context_->port = config_.port;
    context_->tls_enabled = config_.tls_enabled;
    context_->supported_protocols = config_.supported_protocols;
    context_->supported_extensions = config_.supported_extensions;
    context_->max_connections = config_.max_connections;
    context_->max_message_size = config_.max_message_size;
    
    return setup_protocols() && setup_extensions();
}

bool WebSocketServer::start() {
    if (running_) {
        debug(websocket, "WebSocket server already running");
        return true;
    }
    
    if (!setup_lws_context()) {
        debug(websocket, "Failed to setup libwebsockets context");
        return false;
    }
    
    if (!setup_vhost()) {
        debug(websocket, "Failed to setup libwebsockets vhost");
        return false;
    }
    
    running_ = true;
    debug(websocket, "WebSocket server started on %s:%d", 
          config_.bind_address.c_str(), config_.port);
    
    return true;
}

bool WebSocketServer::stop() {
    if (!running_) {
        return true;
    }
    
    // Close all active connections
    std::vector<int> connection_ids = get_connection_ids();
    for (int id : connection_ids) {
        close_connection(id, WS_CLOSE_GOING_AWAY, "Server shutdown");
    }
    
    // Destroy libwebsockets context
    if (context_->context) {
        lws_context_destroy(context_->context);
        context_->context = nullptr;
    }
    
    context_->vhost = nullptr;
    running_ = false;
    
    debug(websocket, "WebSocket server stopped");
    return true;
}

bool WebSocketServer::setup_protocols() {
    protocols_.clear();
    
    // HTTP protocol for handshake
    struct lws_protocols http_proto = {
        "http",
        protocol_callback_http,
        0,
        0,
        0,
        nullptr,
        0
    };
    protocols_.push_back(http_proto);
    
    // WebSocket protocols
    for (const auto& protocol : config_.supported_protocols) {
        struct lws_protocols ws_proto = {
            protocol.c_str(),
            protocol_callback_websocket,
            sizeof(ws_connection_context*),
            config_.max_message_size,
            0,
            nullptr,
            0
        };
        protocols_.push_back(ws_proto);
    }
    
    // Terminator
    struct lws_protocols terminator = { nullptr, nullptr, 0, 0, 0, nullptr, 0 };
    protocols_.push_back(terminator);
    
    return true;
}

bool WebSocketServer::setup_extensions() {
    extensions_.clear();
    
    for (const auto& extension : config_.supported_extensions) {
        if (extension == "permessage-deflate") {
            struct lws_extension ext = {
                "permessage-deflate",
                lws_extension_callback_pm_deflate,
                "permessage-deflate; client_no_context_takeover; client_max_window_bits"
            };
            extensions_.push_back(ext);
        }
    }
    
    // Terminator
    struct lws_extension terminator = { nullptr, nullptr, nullptr };
    extensions_.push_back(terminator);
    
    return true;
}

bool WebSocketServer::setup_lws_context() {
    struct lws_context_creation_info info;
    memset(&info, 0, sizeof(info));
    
    info.port = config_.port;
    info.iface = config_.bind_address.empty() ? nullptr : config_.bind_address.c_str();
    info.protocols = protocols_.data();
    info.extensions = extensions_.empty() ? nullptr : extensions_.data();
    info.options = LWS_SERVER_OPTION_VALIDATE_UTF8;
    info.gid = -1;
    info.uid = -1;
    info.user = this;
    
    // TLS configuration
    if (config_.tls_enabled) {
        info.ssl_cert_filepath = config_.cert_file.c_str();
        info.ssl_private_key_filepath = config_.key_file.c_str();
        info.ssl_ca_filepath = config_.ca_file.empty() ? nullptr : config_.ca_file.c_str();
        info.options |= LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;
    }
    
    context_->context = lws_create_context(&info);
    return context_->context != nullptr;
}

bool WebSocketServer::setup_vhost() {
    if (!context_->context) {
        return false;
    }
    
    context_->vhost = lws_get_vhost_by_name(context_->context, "default");
    return context_->vhost != nullptr;
}

bool WebSocketServer::accept_connection(struct lws* wsi) {
    if (context_->connections.size() >= config_.max_connections) {
        debug(websocket, "WebSocket server connection limit reached");
        return false;
    }
    
    int connection_id = register_connection(wsi);
    if (connection_id <= 0) {
        debug(websocket, "Failed to register WebSocket connection");
        return false;
    }
    
    auto conn = context_->connections[connection_id];
    conn->wsi = wsi;
    conn->state = WS_STATE_OPEN;
    conn->connected_at = time(nullptr);
    
    // Set user data
    lws_set_wsi_user(wsi, &connection_id);
    
    debug(websocket, "WebSocket connection accepted: id=%d", connection_id);
    return true;
}

void WebSocketServer::close_connection(int connection_id, int close_code, 
                                     const std::string& reason) {
    auto it = context_->connections.find(connection_id);
    if (it == context_->connections.end()) {
        return;
    }
    
    auto conn = it->second;
    if (conn->wsi && conn->state != WS_STATE_CLOSED) {
        conn->state = WS_STATE_CLOSING;
        
        // Send close frame
        std::vector<uint8_t> close_payload;
        if (close_code != 0) {
            close_payload.resize(2);
            close_payload[0] = (close_code >> 8) & 0xFF;
            close_payload[1] = close_code & 0xFF;
            
            if (!reason.empty()) {
                size_t reason_len = std::min(reason.length(), size_t(123));
                close_payload.insert(close_payload.end(), 
                                   reason.begin(), reason.begin() + reason_len);
            }
        }
        
        lws_close_reason(conn->wsi, static_cast<lws_close_status>(close_code),
                        close_payload.data(), close_payload.size());
        lws_callback_on_writable(conn->wsi);
    }
    
    debug(websocket, "WebSocket connection closed: id=%d, code=%d", 
          connection_id, close_code);
}

void WebSocketServer::handle_connection_close(struct lws* wsi) {
    auto it = wsi_to_connection_id_.find(wsi);
    if (it != wsi_to_connection_id_.end()) {
        int connection_id = it->second;
        auto conn_it = context_->connections.find(connection_id);
        if (conn_it != context_->connections.end()) {
            conn_it->second->state = WS_STATE_CLOSED;
            conn_it->second->wsi = nullptr;
        }
        unregister_connection(wsi);
    }
}

void WebSocketServer::handle_message(struct lws* wsi, const uint8_t* data, 
                                    size_t len, bool is_binary, bool is_final) {
    auto it = wsi_to_connection_id_.find(wsi);
    if (it == wsi_to_connection_id_.end()) {
        return;
    }
    
    int connection_id = it->second;
    auto conn_it = context_->connections.find(connection_id);
    if (conn_it == context_->connections.end()) {
        return;
    }
    
    auto conn = conn_it->second;
    conn->messages_received++;
    conn->bytes_received += len;
    
    if (is_binary) {
        std::vector<uint8_t> binary_data(data, data + len);
        process_binary_message(wsi, binary_data);
    } else {
        std::string text_message(reinterpret_cast<const char*>(data), len);
        process_text_message(wsi, text_message);
    }
}

bool WebSocketServer::send_message(int connection_id, const uint8_t* data, 
                                  size_t len, bool is_binary) {
    auto it = context_->connections.find(connection_id);
    if (it == context_->connections.end()) {
        return false;
    }
    
    auto conn = it->second;
    if (!conn->wsi || conn->state != WS_STATE_OPEN) {
        return false;
    }
    
    // Allocate buffer with LWS_PRE padding
    size_t total_len = LWS_PRE + len;
    std::vector<uint8_t> buffer(total_len);
    
    // Copy data after LWS_PRE padding
    memcpy(buffer.data() + LWS_PRE, data, len);
    
    enum lws_write_protocol protocol = is_binary ? LWS_WRITE_BINARY : LWS_WRITE_TEXT;
    int result = lws_write(conn->wsi, buffer.data() + LWS_PRE, len, protocol);
    
    if (result >= 0) {
        conn->messages_sent++;
        conn->bytes_sent += len;
        return true;
    }
    
    return false;
}

bool WebSocketServer::negotiate_subprotocol(struct lws* wsi, 
                                           const std::string& requested) {
    // Check if requested protocol is supported
    auto it = std::find(config_.supported_protocols.begin(),
                       config_.supported_protocols.end(), requested);
    
    if (it != config_.supported_protocols.end()) {
        lws_set_wsi_user(wsi, const_cast<char*>(requested.c_str()));
        return true;
    }
    
    return false;
}

bool WebSocketServer::negotiate_extensions(struct lws* wsi, 
                                         const std::string& requested) {
    // Basic extension negotiation - can be enhanced
    return std::find(config_.supported_extensions.begin(),
                    config_.supported_extensions.end(), requested) !=
           config_.supported_extensions.end();
}

bool WebSocketServer::validate_origin(struct lws* wsi, const std::string& origin) {
    if (!config_.require_origin) {
        return true;
    }
    
    if (config_.allowed_origins.empty()) {
        return true;
    }
    
    return std::find(config_.allowed_origins.begin(),
                    config_.allowed_origins.end(), origin) !=
           config_.allowed_origins.end();
}

bool WebSocketServer::validate_handshake(struct lws* wsi) {
    // Basic handshake validation
    char origin[256];
    if (lws_hdr_copy(wsi, origin, sizeof(origin), WSI_TOKEN_ORIGIN) > 0) {
        if (!validate_origin(wsi, std::string(origin))) {
            return false;
        }
    }
    
    return true;
}

mapping_t* WebSocketServer::get_server_stats() {
    mapping_t* stats = allocate_mapping(8);
    
    // Server info
    add_mapping_string(stats, "address", config_.bind_address.c_str());
    add_mapping_pair(stats, "port", config_.port);
    add_mapping_pair(stats, "running", running_ ? 1 : 0);
    add_mapping_pair(stats, "tls_enabled", config_.tls_enabled ? 1 : 0);
    
    // Connection stats
    add_mapping_pair(stats, "active_connections", static_cast<int>(context_->connections.size()));
    add_mapping_pair(stats, "max_connections", static_cast<int>(config_.max_connections));
    add_mapping_pair(stats, "max_message_size", static_cast<int>(config_.max_message_size));
    
    // Protocol info
    array_t* protocols = allocate_empty_array(config_.supported_protocols.size());
    for (size_t i = 0; i < config_.supported_protocols.size(); i++) {
        protocols->item[i] = const0;
        protocols->item[i].type = T_STRING;
        protocols->item[i].subtype = STRING_MALLOC;
        protocols->item[i].u.string = string_copy(config_.supported_protocols[i].c_str(),
                                                 "websocket_server_stats");
    }
    add_mapping_array(stats, "protocols", protocols);
    
    return stats;
}

size_t WebSocketServer::get_connection_count() const {
    return context_->connections.size();
}

std::vector<int> WebSocketServer::get_connection_ids() const {
    std::vector<int> ids;
    for (const auto& pair : context_->connections) {
        ids.push_back(pair.first);
    }
    return ids;
}

int WebSocketServer::handle_lws_callback(struct lws* wsi, 
                                        enum lws_callback_reasons reason,
                                        void* user, void* in, size_t len) {
    switch (reason) {
        case LWS_CALLBACK_ESTABLISHED:
            return accept_connection(wsi) ? 0 : -1;
            
        case LWS_CALLBACK_RECEIVE:
            handle_message(wsi, static_cast<const uint8_t*>(in), len, false, true);
            break;
            
        case LWS_CALLBACK_RECEIVE_PONG:
            // Handle pong frame
            break;
            
        case LWS_CALLBACK_CLOSED:
            handle_connection_close(wsi);
            break;
            
        case LWS_CALLBACK_FILTER_PROTOCOL_CONNECTION:
            return validate_handshake(wsi) ? 0 : -1;
            
        default:
            break;
    }
    
    return 0;
}

// Static protocol callbacks
int WebSocketServer::protocol_callback_http(struct lws* wsi, 
                                           enum lws_callback_reasons reason,
                                           void* user, void* in, size_t len) {
    // Basic HTTP handling for WebSocket upgrade
    switch (reason) {
        case LWS_CALLBACK_HTTP:
            // Reject non-WebSocket requests
            lws_return_http_status(wsi, HTTP_STATUS_FORBIDDEN, nullptr);
            return -1;
            
        default:
            break;
    }
    
    return lws_callback_http_dummy(wsi, reason, user, in, len);
}

int WebSocketServer::protocol_callback_websocket(struct lws* wsi,
                                                enum lws_callback_reasons reason,
                                                void* user, void* in, size_t len) {
    // Get server instance from context user data
    WebSocketServer* server = static_cast<WebSocketServer*>(
        lws_context_user(lws_get_context(wsi)));
    
    if (server) {
        return server->handle_lws_callback(wsi, reason, user, in, len);
    }
    
    return 0;
}

int WebSocketServer::register_connection(struct lws* wsi) {
    static int next_id = 1;
    int connection_id = next_id++;
    
    auto conn = std::make_shared<ws_connection_context>();
    conn->lpc_socket_id = connection_id;
    conn->wsi = wsi;
    conn->state = WS_STATE_CONNECTING;
    
    context_->connections[connection_id] = conn;
    wsi_to_connection_id_[wsi] = connection_id;
    
    return connection_id;
}

void WebSocketServer::unregister_connection(struct lws* wsi) {
    auto it = wsi_to_connection_id_.find(wsi);
    if (it != wsi_to_connection_id_.end()) {
        int connection_id = it->second;
        context_->connections.erase(connection_id);
        wsi_to_connection_id_.erase(it);
    }
}

std::shared_ptr<ws_connection_context> WebSocketServer::get_connection_by_wsi(struct lws* wsi) {
    auto it = wsi_to_connection_id_.find(wsi);
    if (it != wsi_to_connection_id_.end()) {
        auto conn_it = context_->connections.find(it->second);
        if (conn_it != context_->connections.end()) {
            return conn_it->second;
        }
    }
    return nullptr;
}

void WebSocketServer::process_text_message(struct lws* wsi, const std::string& message) {
    // Default text message processing - can be overridden
    debug(websocket, "WebSocket text message received: %s", message.c_str());
    
    // Echo message back (for testing)
    send_message(wsi_to_connection_id_[wsi], 
                reinterpret_cast<const uint8_t*>(message.c_str()),
                message.length(), false);
}

void WebSocketServer::process_binary_message(struct lws* wsi, 
                                            const std::vector<uint8_t>& data) {
    // Default binary message processing - can be overridden
    debug(websocket, "WebSocket binary message received: %zu bytes", data.size());
    
    // Echo message back (for testing)
    send_message(wsi_to_connection_id_[wsi], data.data(), data.size(), true);
}

void WebSocketServer::process_ping(struct lws* wsi, const std::string& payload) {
    debug(websocket, "WebSocket ping received");
    // libwebsockets handles pong response automatically
}

void WebSocketServer::process_pong(struct lws* wsi, const std::string& payload) {
    debug(websocket, "WebSocket pong received");
    // Update connection activity time
    auto conn = get_connection_by_wsi(wsi);
    if (conn) {
        conn->last_ping = time(nullptr);
    }
}

void WebSocketServer::handle_protocol_error(struct lws* wsi, const std::string& error) {
    debug(websocket, "WebSocket protocol error: %s", error.c_str());
    auto it = wsi_to_connection_id_.find(wsi);
    if (it != wsi_to_connection_id_.end()) {
        close_connection(it->second, WS_CLOSE_PROTOCOL_ERROR, error);
    }
}

void WebSocketServer::handle_connection_error(struct lws* wsi, const std::string& error) {
    debug(websocket, "WebSocket connection error: %s", error.c_str());
    handle_connection_close(wsi);
}

bool WebSocketServer::is_valid_protocol(const std::string& protocol) const {
    return std::find(config_.supported_protocols.begin(),
                    config_.supported_protocols.end(), protocol) !=
           config_.supported_protocols.end();
}

bool WebSocketServer::is_valid_extension(const std::string& extension) const {
    return std::find(config_.supported_extensions.begin(),
                    config_.supported_extensions.end(), extension) !=
           config_.supported_extensions.end();
}

std::string WebSocketServer::get_client_ip(struct lws* wsi) {
    char client_ip[32];
    lws_get_peer_simple(wsi, client_ip, sizeof(client_ip));
    return std::string(client_ip);
}

std::string WebSocketServer::get_request_uri(struct lws* wsi) {
    char uri[256];
    lws_hdr_copy(wsi, uri, sizeof(uri), WSI_TOKEN_GET_URI);
    return std::string(uri);
}

bool WebSocketServer::validate_config() const {
    if (config_.port <= 0 || config_.port > 65535) {
        return false;
    }
    
    if (config_.max_connections == 0) {
        return false;
    }
    
    if (config_.tls_enabled && 
        (config_.cert_file.empty() || config_.key_file.empty())) {
        return false;
    }
    
    return true;
}

void WebSocketServer::cleanup_connections() {
    context_->connections.clear();
    wsi_to_connection_id_.clear();
}

void WebSocketServer::cleanup_protocols() {
    protocols_.clear();
}

void WebSocketServer::cleanup_extensions() {
    extensions_.clear();
}

/*
 * WebSocket Server Manager Implementation
 */

WebSocketServerManager* WebSocketServerManager::instance_ = nullptr;

WebSocketServerManager* WebSocketServerManager::getInstance() {
    if (!instance_) {
        instance_ = new WebSocketServerManager();
    }
    return instance_;
}

WebSocketServerManager::WebSocketServerManager() : next_server_id_(1) {
}

WebSocketServerManager::~WebSocketServerManager() {
    shutdown_all_servers();
}

int WebSocketServerManager::create_server(const ws_server_config& config) {
    int server_id = allocate_server_id();
    
    auto server = std::make_unique<WebSocketServer>();
    if (server->initialize(config)) {
        servers_[server_id] = std::move(server);
        debug(websocket, "WebSocket server created: id=%d", server_id);
        return server_id;
    }
    
    debug(websocket, "Failed to create WebSocket server");
    return -1;
}

bool WebSocketServerManager::start_server(int server_id) {
    auto it = servers_.find(server_id);
    if (it != servers_.end()) {
        return it->second->start();
    }
    return false;
}

bool WebSocketServerManager::stop_server(int server_id) {
    auto it = servers_.find(server_id);
    if (it != servers_.end()) {
        return it->second->stop();
    }
    return false;
}

bool WebSocketServerManager::remove_server(int server_id) {
    auto it = servers_.find(server_id);
    if (it != servers_.end()) {
        it->second->stop();
        servers_.erase(it);
        return true;
    }
    return false;
}

WebSocketServer* WebSocketServerManager::get_server(int server_id) {
    auto it = servers_.find(server_id);
    if (it != servers_.end()) {
        return it->second.get();
    }
    return nullptr;
}

std::vector<int> WebSocketServerManager::get_server_ids() const {
    std::vector<int> ids;
    for (const auto& pair : servers_) {
        ids.push_back(pair.first);
    }
    return ids;
}

void WebSocketServerManager::shutdown_all_servers() {
    for (auto& pair : servers_) {
        pair.second->stop();
    }
    servers_.clear();
}

mapping_t* WebSocketServerManager::get_all_server_stats() {
    mapping_t* all_stats = allocate_mapping(servers_.size());
    
    for (const auto& pair : servers_) {
        mapping_t* server_stats = pair.second->get_server_stats();
        svalue_t key, *value;
        key.type = T_STRING;
        key.u.string = const_cast<char*>(std::to_string(pair.first).c_str());
        
        value = find_for_insert(all_stats, &key, 1);
        value->type = T_MAPPING;
        value->u.map = server_stats;
    }
    
    return all_stats;
}

int WebSocketServerManager::allocate_server_id() {
    return next_server_id_++;
}

void WebSocketServerManager::cleanup_servers() {
    servers_.clear();
}

/*
 * Utility Functions
 */

bool mapping_to_server_config(const mapping_t* options, ws_server_config& config) {
    if (!options) {
        return false;
    }
    
    svalue_t* value;
    
    // TLS configuration
    if ((value = find_string_in_mapping(options, "tls_enabled")) && value->type == T_NUMBER) {
        config.tls_enabled = value->u.number != 0;
    }
    
    if ((value = find_string_in_mapping(options, "cert_file")) && value->type == T_STRING) {
        config.cert_file = value->u.string;
    }
    
    if ((value = find_string_in_mapping(options, "key_file")) && value->type == T_STRING) {
        config.key_file = value->u.string;
    }
    
    // Connection limits
    if ((value = find_string_in_mapping(options, "max_connections")) && value->type == T_NUMBER) {
        config.max_connections = value->u.number;
    }
    
    if ((value = find_string_in_mapping(options, "max_message_size")) && value->type == T_NUMBER) {
        config.max_message_size = value->u.number;
    }
    
    // Timeouts
    if ((value = find_string_in_mapping(options, "ping_interval")) && value->type == T_NUMBER) {
        config.ping_interval = value->u.number;
    }
    
    // Protocols
    if ((value = find_string_in_mapping(options, "protocols")) && value->type == T_ARRAY) {
        config.supported_protocols.clear();
        for (int i = 0; i < value->u.arr->size; i++) {
            if (value->u.arr->item[i].type == T_STRING) {
                config.supported_protocols.push_back(value->u.arr->item[i].u.string);
            }
        }
    }
    
    return true;
}

mapping_t* server_config_to_mapping(const ws_server_config& config) {
    mapping_t* mapping = allocate_mapping(16);
    
    add_mapping_string(mapping, "bind_address", config.bind_address.c_str());
    add_mapping_pair(mapping, "port", config.port);
    add_mapping_pair(mapping, "tls_enabled", config.tls_enabled ? 1 : 0);
    add_mapping_string(mapping, "cert_file", config.cert_file.c_str());
    add_mapping_string(mapping, "key_file", config.key_file.c_str());
    add_mapping_pair(mapping, "max_connections", static_cast<int>(config.max_connections));
    add_mapping_pair(mapping, "max_message_size", static_cast<int>(config.max_message_size));
    add_mapping_pair(mapping, "ping_interval", config.ping_interval);
    add_mapping_pair(mapping, "pong_timeout", config.pong_timeout);
    add_mapping_pair(mapping, "require_origin", config.require_origin ? 1 : 0);
    add_mapping_pair(mapping, "validate_utf8", config.validate_utf8 ? 1 : 0);
    
    // Protocols array
    array_t* protocols = allocate_empty_array(config.supported_protocols.size());
    for (size_t i = 0; i < config.supported_protocols.size(); i++) {
        protocols->item[i] = const0;
        protocols->item[i].type = T_STRING;
        protocols->item[i].subtype = STRING_MALLOC;
        protocols->item[i].u.string = string_copy(config.supported_protocols[i].c_str(),
                                                 "server_config_mapping");
    }
    add_mapping_array(mapping, "protocols", protocols);
    
    return mapping;
}

bool validate_server_config(const ws_server_config& config, std::string& error) {
    if (config.port <= 0 || config.port > 65535) {
        error = "Invalid port number";
        return false;
    }
    
    if (config.max_connections == 0) {
        error = "Maximum connections must be greater than 0";
        return false;
    }
    
    if (config.tls_enabled) {
        if (config.cert_file.empty()) {
            error = "TLS certificate file required when TLS is enabled";
            return false;
        }
        if (config.key_file.empty()) {
            error = "TLS private key file required when TLS is enabled";
            return false;
        }
    }
    
    return true;
}

ws_server_config get_default_server_config() {
    return ws_server_config();
}