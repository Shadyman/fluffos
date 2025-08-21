/*
 * WebSocket Client Implementation
 * 
 * Client-side WebSocket functionality using libwebsockets
 * with FluffOS unified socket architecture integration.
 */

#include "packages/websocket/ws_client.h"
#include "packages/websocket/websocket.h"
#include "base/internal/log.h"
#include "vm/internal/simulate.h"

#include <libwebsockets.h>
#include <cstring>
#include <algorithm>
#include <regex>

/*
 * WebSocket Client Implementation
 */

WebSocketClient::WebSocketClient() : state_(WS_CLIENT_DISCONNECTED), 
                                   lws_context_(nullptr), wsi_(nullptr),
                                   last_error_code_(0), connect_time_(0),
                                   last_activity_(0) {
    context_ = std::make_shared<ws_connection_context>();
}

WebSocketClient::~WebSocketClient() {
    if (is_connected()) {
        disconnect();
    }
    cleanup();
}

bool WebSocketClient::connect(const ws_client_config& config) {
    if (state_ != WS_CLIENT_DISCONNECTED) {
        set_error("Client already connected or connecting");
        return false;
    }
    
    config_ = config;
    
    if (!validate_url(config_.url)) {
        set_error("Invalid WebSocket URL");
        return false;
    }
    
    if (!initialize_context()) {
        set_error("Failed to initialize libwebsockets context");
        return false;
    }
    
    if (!create_connection()) {
        set_error("Failed to create WebSocket connection");
        return false;
    }
    
    set_state(WS_CLIENT_CONNECTING);
    connect_time_ = time(nullptr);
    
    debug(websocket, "WebSocket client connecting to: %s", config_.url.c_str());
    return true;
}

bool WebSocketClient::disconnect(int close_code, const std::string& reason) {
    if (state_ == WS_CLIENT_DISCONNECTED) {
        return true;
    }
    
    if (wsi_ && state_ == WS_CLIENT_CONNECTED) {
        set_state(WS_CLIENT_CLOSING);
        
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
        
        lws_close_reason(wsi_, static_cast<lws_close_status>(close_code),
                        close_payload.data(), close_payload.size());
        lws_callback_on_writable(wsi_);
    }
    
    cleanup_context();
    set_state(WS_CLIENT_DISCONNECTED);
    
    debug(websocket, "WebSocket client disconnected");
    return true;
}

bool WebSocketClient::send_text(const std::string& message) {
    if (state_ != WS_CLIENT_CONNECTED || !wsi_) {
        set_error("Client not connected");
        return false;
    }
    
    if (message.length() > config_.max_message_size) {
        set_error("Message too large");
        return false;
    }
    
    // Allocate buffer with LWS_PRE padding
    size_t total_len = LWS_PRE + message.length();
    std::vector<uint8_t> buffer(total_len);
    
    // Copy message after LWS_PRE padding
    memcpy(buffer.data() + LWS_PRE, message.c_str(), message.length());
    
    int result = lws_write(wsi_, buffer.data() + LWS_PRE, 
                          message.length(), LWS_WRITE_TEXT);
    
    if (result >= 0) {
        context_->messages_sent++;
        context_->bytes_sent += message.length();
        update_activity_time();
        return true;
    }
    
    set_error("Failed to send text message", result);
    return false;
}

bool WebSocketClient::send_binary(const std::vector<uint8_t>& data) {
    if (state_ != WS_CLIENT_CONNECTED || !wsi_) {
        set_error("Client not connected");
        return false;
    }
    
    if (data.size() > config_.max_message_size) {
        set_error("Data too large");
        return false;
    }
    
    // Allocate buffer with LWS_PRE padding
    size_t total_len = LWS_PRE + data.size();
    std::vector<uint8_t> buffer(total_len);
    
    // Copy data after LWS_PRE padding
    memcpy(buffer.data() + LWS_PRE, data.data(), data.size());
    
    int result = lws_write(wsi_, buffer.data() + LWS_PRE, 
                          data.size(), LWS_WRITE_BINARY);
    
    if (result >= 0) {
        context_->messages_sent++;
        context_->bytes_sent += data.size();
        update_activity_time();
        return true;
    }
    
    set_error("Failed to send binary message", result);
    return false;
}

bool WebSocketClient::send_ping(const std::string& payload) {
    if (state_ != WS_CLIENT_CONNECTED || !wsi_) {
        set_error("Client not connected");
        return false;
    }
    
    if (payload.length() > 125) {
        set_error("Ping payload too large (max 125 bytes)");
        return false;
    }
    
    // Allocate buffer with LWS_PRE padding
    size_t total_len = LWS_PRE + payload.length();
    std::vector<uint8_t> buffer(total_len);
    
    // Copy payload after LWS_PRE padding
    memcpy(buffer.data() + LWS_PRE, payload.c_str(), payload.length());
    
    int result = lws_write(wsi_, buffer.data() + LWS_PRE, 
                          payload.length(), LWS_WRITE_PING);
    
    if (result >= 0) {
        update_activity_time();
        return true;
    }
    
    set_error("Failed to send ping", result);
    return false;
}

bool WebSocketClient::send_pong(const std::string& payload) {
    if (state_ != WS_CLIENT_CONNECTED || !wsi_) {
        set_error("Client not connected");
        return false;
    }
    
    if (payload.length() > 125) {
        set_error("Pong payload too large (max 125 bytes)");
        return false;
    }
    
    // Allocate buffer with LWS_PRE padding
    size_t total_len = LWS_PRE + payload.length();
    std::vector<uint8_t> buffer(total_len);
    
    // Copy payload after LWS_PRE padding
    memcpy(buffer.data() + LWS_PRE, payload.c_str(), payload.length());
    
    int result = lws_write(wsi_, buffer.data() + LWS_PRE, 
                          payload.length(), LWS_WRITE_PONG);
    
    if (result >= 0) {
        update_activity_time();
        return true;
    }
    
    set_error("Failed to send pong", result);
    return false;
}

mapping_t* WebSocketClient::get_connection_info() {
    mapping_t* info = allocate_mapping(12);
    
    add_mapping_string(info, "url", config_.url.c_str());
    add_mapping_string(info, "state", 
                      state_ == WS_CLIENT_DISCONNECTED ? "disconnected" :
                      state_ == WS_CLIENT_CONNECTING ? "connecting" :
                      state_ == WS_CLIENT_CONNECTED ? "connected" :
                      state_ == WS_CLIENT_CLOSING ? "closing" : "error");
    
    add_mapping_string(info, "protocol", negotiated_protocol_.c_str());
    add_mapping_pair(info, "connect_time", static_cast<int>(connect_time_));
    add_mapping_pair(info, "last_activity", static_cast<int>(last_activity_));
    
    // Extensions
    array_t* extensions = allocate_empty_array(negotiated_extensions_.size());
    for (size_t i = 0; i < negotiated_extensions_.size(); i++) {
        extensions->item[i] = const0;
        extensions->item[i].type = T_STRING;
        extensions->item[i].subtype = STRING_MALLOC;
        extensions->item[i].u.string = string_copy(negotiated_extensions_[i].c_str(),
                                                  "client_connection_info");
    }
    add_mapping_array(info, "extensions", extensions);
    
    // Configuration
    add_mapping_pair(info, "ping_interval", config_.ping_interval);
    add_mapping_pair(info, "max_message_size", static_cast<int>(config_.max_message_size));
    add_mapping_pair(info, "verify_ssl", config_.verify_ssl ? 1 : 0);
    add_mapping_string(info, "user_agent", config_.user_agent.c_str());
    add_mapping_string(info, "origin", config_.origin.c_str());
    
    // Error info
    if (!last_error_.empty()) {
        add_mapping_string(info, "last_error", last_error_.c_str());
        add_mapping_pair(info, "last_error_code", last_error_code_);
    }
    
    return info;
}

mapping_t* WebSocketClient::get_connection_stats() {
    mapping_t* stats = allocate_mapping(8);
    
    add_mapping_pair(stats, "messages_sent", 
                    static_cast<int>(context_->messages_sent));
    add_mapping_pair(stats, "messages_received", 
                    static_cast<int>(context_->messages_received));
    add_mapping_pair(stats, "bytes_sent", 
                    static_cast<int>(context_->bytes_sent));
    add_mapping_pair(stats, "bytes_received", 
                    static_cast<int>(context_->bytes_received));
    add_mapping_pair(stats, "connected_at", 
                    static_cast<int>(context_->connected_at));
    add_mapping_pair(stats, "last_ping", 
                    static_cast<int>(context_->last_ping));
    
    time_t now = time(nullptr);
    add_mapping_pair(stats, "uptime", 
                    context_->connected_at > 0 ? 
                    static_cast<int>(now - context_->connected_at) : 0);
    
    return stats;
}

bool WebSocketClient::initialize_context() {
    struct lws_context_creation_info info;
    memset(&info, 0, sizeof(info));
    
    info.port = CONTEXT_PORT_NO_LISTEN;
    info.protocols = nullptr;  // Client doesn't need protocols
    info.gid = -1;
    info.uid = -1;
    info.user = this;
    
    // SSL configuration
    if (config_.verify_ssl) {
        info.options |= LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;
    } else {
        info.options |= LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT |
                       LWS_SERVER_OPTION_DISABLE_IPV6;
    }
    
    lws_context_ = lws_create_context(&info);
    return lws_context_ != nullptr;
}

bool WebSocketClient::setup_connection_info() {
    // Parse URL
    std::string host, path;
    int port;
    bool use_ssl;
    
    if (!parse_url(config_.url, host, port, path, use_ssl)) {
        return false;
    }
    
    resolved_url_ = config_.url;
    return true;
}

bool WebSocketClient::create_connection() {
    if (!setup_connection_info()) {
        return false;
    }
    
    // Parse URL components
    std::string host, path;
    int port;
    bool use_ssl;
    
    if (!parse_url(config_.url, host, port, path, use_ssl)) {
        return false;
    }
    
    struct lws_client_connect_info connect_info;
    memset(&connect_info, 0, sizeof(connect_info));
    
    connect_info.context = lws_context_;
    connect_info.address = host.c_str();
    connect_info.port = port;
    connect_info.path = path.c_str();
    connect_info.host = host.c_str();
    connect_info.origin = config_.origin.empty() ? host.c_str() : config_.origin.c_str();
    connect_info.protocol = config_.protocol.empty() ? nullptr : config_.protocol.c_str();
    connect_info.ietf_version_or_minus_one = -1;
    connect_info.userdata = this;
    
    if (use_ssl) {
        connect_info.ssl_connection = LCCSCF_USE_SSL;
        if (!config_.verify_ssl) {
            connect_info.ssl_connection |= LCCSCF_ALLOW_SELFSIGNED |
                                          LCCSCF_SKIP_SERVER_CERT_HOSTNAME_CHECK;
        }
    }
    
    wsi_ = lws_client_connect_via_info(&connect_info);
    return wsi_ != nullptr;
}

int WebSocketClient::handle_lws_callback(struct lws* wsi, 
                                        enum lws_callback_reasons reason,
                                        void* user, void* in, size_t len) {
    switch (reason) {
        case LWS_CALLBACK_CLIENT_ESTABLISHED:
            handle_connection_established();
            break;
            
        case LWS_CALLBACK_CLIENT_RECEIVE:
            handle_message(static_cast<const uint8_t*>(in), len, false, true);
            break;
            
        case LWS_CALLBACK_CLIENT_RECEIVE_PONG:
            handle_pong(std::string(static_cast<const char*>(in), len));
            break;
            
        case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
            handle_connection_error("Connection error", 0);
            break;
            
        case LWS_CALLBACK_CLIENT_CLOSED:
            handle_connection_closed();
            break;
            
        case LWS_CALLBACK_CLIENT_APPEND_HANDSHAKE_HEADER:
            return add_custom_headers(wsi) ? 0 : -1;
            
        default:
            break;
    }
    
    return 0;
}

void WebSocketClient::set_state(ws_client_state new_state) {
    ws_client_state old_state = state_;
    state_ = new_state;
    
    debug(websocket, "WebSocket client state changed: %d -> %d", 
          static_cast<int>(old_state), static_cast<int>(new_state));
}

void WebSocketClient::handle_connection_established() {
    set_state(WS_CLIENT_CONNECTED);
    context_->connected_at = time(nullptr);
    context_->state = WS_STATE_OPEN;
    
    // Handle protocol negotiation
    handle_protocol_negotiation(wsi_);
    handle_extension_negotiation(wsi_);
    
    debug(websocket, "WebSocket client connected successfully");
    clear_error();
}

void WebSocketClient::handle_connection_error(const std::string& error, int error_code) {
    set_error(error, error_code);
    set_state(WS_CLIENT_ERROR);
    cleanup_context();
    
    debug(websocket, "WebSocket client connection error: %s", error.c_str());
}

void WebSocketClient::handle_connection_closed() {
    set_state(WS_CLIENT_DISCONNECTED);
    context_->state = WS_STATE_CLOSED;
    wsi_ = nullptr;
    
    debug(websocket, "WebSocket client connection closed");
}

void WebSocketClient::handle_message(const uint8_t* data, size_t len, 
                                    bool is_binary, bool is_final) {
    context_->messages_received++;
    context_->bytes_received += len;
    update_activity_time();
    
    if (is_binary) {
        debug(websocket, "WebSocket client received binary message: %zu bytes", len);
    } else {
        std::string message(reinterpret_cast<const char*>(data), len);
        debug(websocket, "WebSocket client received text message: %s", message.c_str());
    }
}

void WebSocketClient::handle_ping(const std::string& payload) {
    debug(websocket, "WebSocket client received ping");
    // libwebsockets handles pong response automatically
}

void WebSocketClient::handle_pong(const std::string& payload) {
    debug(websocket, "WebSocket client received pong");
    context_->last_ping = time(nullptr);
    update_activity_time();
}

void WebSocketClient::handle_close(int close_code, const std::string& reason) {
    debug(websocket, "WebSocket client received close: code=%d, reason=%s", 
          close_code, reason.c_str());
    set_state(WS_CLIENT_CLOSING);
}

void WebSocketClient::handle_protocol_negotiation(struct lws* wsi) {
    const char* protocol = lws_get_protocol(wsi)->name;
    if (protocol) {
        negotiated_protocol_ = protocol;
        debug(websocket, "WebSocket client negotiated protocol: %s", protocol);
    }
}

void WebSocketClient::handle_extension_negotiation(struct lws* wsi) {
    // libwebsockets handles extension negotiation internally
    // We can query the negotiated extensions if needed
}

bool WebSocketClient::parse_url(const std::string& url, std::string& host, 
                               int& port, std::string& path, bool& use_ssl) {
    std::regex url_regex(R"(^(wss?):\/\/([^:\/\s]+)(?::(\d+))?(\/.*)?$)");
    std::smatch matches;
    
    if (!std::regex_match(url, matches, url_regex)) {
        return false;
    }
    
    std::string scheme = matches[1].str();
    host = matches[2].str();
    
    if (matches[3].matched) {
        port = std::stoi(matches[3].str());
    } else {
        port = (scheme == "wss") ? 443 : 80;
    }
    
    path = matches[4].matched ? matches[4].str() : "/";
    use_ssl = (scheme == "wss");
    
    return true;
}

bool WebSocketClient::validate_url(const std::string& url) {
    std::string host, path;
    int port;
    bool use_ssl;
    
    return parse_url(url, host, port, path, use_ssl) &&
           !host.empty() && port > 0 && port <= 65535;
}

bool WebSocketClient::add_custom_headers(struct lws* wsi) {
    if (!config_.custom_headers) {
        return true;
    }
    
    // Add custom headers from mapping
    // This would require iterating through the mapping and calling
    // lws_add_http_header_by_name for each header
    
    return true;
}

mapping_t* WebSocketClient::parse_response_headers(struct lws* wsi) {
    mapping_t* headers = allocate_mapping(8);
    
    // Parse response headers using libwebsockets API
    char header_value[256];
    
    // Server header
    if (lws_hdr_copy(wsi, header_value, sizeof(header_value), 
                     WSI_TOKEN_HTTP_SERVER) > 0) {
        add_mapping_string(headers, "server", header_value);
    }
    
    // WebSocket accept key
    if (lws_hdr_copy(wsi, header_value, sizeof(header_value), 
                     WSI_TOKEN_WEBSOCKET_ACCEPT_KEY) > 0) {
        add_mapping_string(headers, "sec-websocket-accept", header_value);
    }
    
    // WebSocket protocol
    if (lws_hdr_copy(wsi, header_value, sizeof(header_value), 
                     WSI_TOKEN_WEBSOCKET_PROTOCOL) > 0) {
        add_mapping_string(headers, "sec-websocket-protocol", header_value);
    }
    
    // WebSocket extensions
    if (lws_hdr_copy(wsi, header_value, sizeof(header_value), 
                     WSI_TOKEN_WEBSOCKET_EXTENSIONS) > 0) {
        add_mapping_string(headers, "sec-websocket-extensions", header_value);
    }
    
    return headers;
}

bool WebSocketClient::setup_ssl_info() {
    // SSL configuration would be handled in create_connection
    return true;
}

bool WebSocketClient::verify_ssl_certificate(struct lws* wsi) {
    if (!config_.verify_ssl) {
        return true;
    }
    
    // SSL certificate verification is handled by libwebsockets
    return true;
}

bool WebSocketClient::setup_proxy_info() {
    // Proxy configuration would be set in lws_client_connect_info
    return true;
}

std::string WebSocketClient::get_websocket_key() {
    // Generate WebSocket key
    unsigned char random_bytes[16];
    if (RAND_bytes(random_bytes, sizeof(random_bytes)) != 1) {
        // Fallback to simple random
        for (int i = 0; i < 16; i++) {
            random_bytes[i] = static_cast<unsigned char>(rand() & 0xFF);
        }
    }
    
    // Base64 encode
    char* encoded = nullptr;
    size_t encoded_len = 0;
    
    // Use libwebsockets base64 encoding if available
    // Otherwise implement base64 encoding
    
    return std::string("dGhlIHNhbXBsZSBub25jZQ=="); // Sample key
}

bool WebSocketClient::validate_websocket_accept(const std::string& accept, 
                                               const std::string& key) {
    // Validate WebSocket accept header against key
    // This is normally handled by libwebsockets
    return true;
}

void WebSocketClient::update_activity_time() {
    last_activity_ = time(nullptr);
}

void WebSocketClient::set_error(const std::string& error, int error_code) {
    last_error_ = error;
    last_error_code_ = error_code;
    debug(websocket, "WebSocket client error: %s (code=%d)", error.c_str(), error_code);
}

void WebSocketClient::clear_error() {
    last_error_.clear();
    last_error_code_ = 0;
}

void WebSocketClient::cleanup() {
    cleanup_context();
    context_.reset();
}

void WebSocketClient::cleanup_context() {
    if (wsi_) {
        wsi_ = nullptr;
    }
    
    if (lws_context_) {
        lws_context_destroy(lws_context_);
        lws_context_ = nullptr;
    }
}

/*
 * WebSocket Client Manager Implementation
 */

WebSocketClientManager* WebSocketClientManager::instance_ = nullptr;

WebSocketClientManager* WebSocketClientManager::getInstance() {
    if (!instance_) {
        instance_ = new WebSocketClientManager();
    }
    return instance_;
}

WebSocketClientManager::WebSocketClientManager() : next_client_id_(1) {
}

WebSocketClientManager::~WebSocketClientManager() {
    disconnect_all_clients();
}

int WebSocketClientManager::create_client(const ws_client_config& config) {
    int client_id = allocate_client_id();
    
    auto client = std::make_unique<WebSocketClient>();
    client->set_config(config);
    
    clients_[client_id] = std::move(client);
    debug(websocket, "WebSocket client created: id=%d", client_id);
    
    return client_id;
}

bool WebSocketClientManager::connect_client(int client_id) {
    auto it = clients_.find(client_id);
    if (it != clients_.end()) {
        return it->second->connect(it->second->get_config());
    }
    return false;
}

bool WebSocketClientManager::disconnect_client(int client_id, int close_code, 
                                              const std::string& reason) {
    auto it = clients_.find(client_id);
    if (it != clients_.end()) {
        return it->second->disconnect(close_code, reason);
    }
    return false;
}

bool WebSocketClientManager::remove_client(int client_id) {
    auto it = clients_.find(client_id);
    if (it != clients_.end()) {
        it->second->disconnect();
        clients_.erase(it);
        return true;
    }
    return false;
}

WebSocketClient* WebSocketClientManager::get_client(int client_id) {
    auto it = clients_.find(client_id);
    if (it != clients_.end()) {
        return it->second.get();
    }
    return nullptr;
}

std::vector<int> WebSocketClientManager::get_client_ids() const {
    std::vector<int> ids;
    for (const auto& pair : clients_) {
        ids.push_back(pair.first);
    }
    return ids;
}

bool WebSocketClientManager::send_text(int client_id, const std::string& message) {
    auto client = get_client(client_id);
    if (client) {
        return client->send_text(message);
    }
    return false;
}

bool WebSocketClientManager::send_binary(int client_id, const std::vector<uint8_t>& data) {
    auto client = get_client(client_id);
    if (client) {
        return client->send_binary(data);
    }
    return false;
}

bool WebSocketClientManager::send_ping(int client_id, const std::string& payload) {
    auto client = get_client(client_id);
    if (client) {
        return client->send_ping(payload);
    }
    return false;
}

bool WebSocketClientManager::send_pong(int client_id, const std::string& payload) {
    auto client = get_client(client_id);
    if (client) {
        return client->send_pong(payload);
    }
    return false;
}

void WebSocketClientManager::disconnect_all_clients() {
    for (auto& pair : clients_) {
        pair.second->disconnect(WS_CLOSE_GOING_AWAY, "Manager shutdown");
    }
    clients_.clear();
}

mapping_t* WebSocketClientManager::get_all_client_stats() {
    mapping_t* all_stats = allocate_mapping(clients_.size());
    
    for (const auto& pair : clients_) {
        mapping_t* client_stats = pair.second->get_connection_stats();
        add_mapping_pair(all_stats, std::to_string(pair.first).c_str(), 
                        client_stats);
    }
    
    return all_stats;
}

std::vector<int> WebSocketClientManager::get_connected_clients() const {
    std::vector<int> connected;
    for (const auto& pair : clients_) {
        if (pair.second->is_connected()) {
            connected.push_back(pair.first);
        }
    }
    return connected;
}

std::vector<int> WebSocketClientManager::get_connecting_clients() const {
    std::vector<int> connecting;
    for (const auto& pair : clients_) {
        if (pair.second->get_state() == WS_CLIENT_CONNECTING) {
            connecting.push_back(pair.first);
        }
    }
    return connecting;
}

int WebSocketClientManager::allocate_client_id() {
    return next_client_id_++;
}

void WebSocketClientManager::cleanup_clients() {
    clients_.clear();
}

void WebSocketClientManager::cleanup_disconnected_clients() {
    std::vector<int> to_remove;
    
    for (const auto& pair : clients_) {
        if (pair.second->get_state() == WS_CLIENT_DISCONNECTED) {
            to_remove.push_back(pair.first);
        }
    }
    
    for (int id : to_remove) {
        clients_.erase(id);
    }
}

/*
 * Utility Functions
 */

bool mapping_to_client_config(const mapping_t* options, ws_client_config& config) {
    if (!options) {
        return false;
    }
    
    svalue_t* value;
    
    // Connection settings
    if ((value = find_mapping_value(options, "connect_timeout")) && value->type == T_NUMBER) {
        config.connect_timeout = value->u.number;
    }
    
    if ((value = find_mapping_value(options, "ping_interval")) && value->type == T_NUMBER) {
        config.ping_interval = value->u.number;
    }
    
    if ((value = find_mapping_value(options, "max_message_size")) && value->type == T_NUMBER) {
        config.max_message_size = value->u.number;
    }
    
    // SSL settings
    if ((value = find_mapping_value(options, "verify_ssl")) && value->type == T_NUMBER) {
        config.verify_ssl = value->u.number != 0;
    }
    
    if ((value = find_mapping_value(options, "ca_file")) && value->type == T_STRING) {
        config.ca_file = value->u.string;
    }
    
    // Protocol settings
    if ((value = find_mapping_value(options, "protocol")) && value->type == T_STRING) {
        config.protocol = value->u.string;
    }
    
    if ((value = find_mapping_value(options, "origin")) && value->type == T_STRING) {
        config.origin = value->u.string;
    }
    
    if ((value = find_mapping_value(options, "user_agent")) && value->type == T_STRING) {
        config.user_agent = value->u.string;
    }
    
    // Subprotocols
    if ((value = find_mapping_value(options, "subprotocols")) && value->type == T_ARRAY) {
        config.subprotocols.clear();
        for (int i = 0; i < value->u.arr->size; i++) {
            if (value->u.arr->item[i].type == T_STRING) {
                config.subprotocols.push_back(value->u.arr->item[i].u.string);
            }
        }
    }
    
    return true;
}

mapping_t* client_config_to_mapping(const ws_client_config& config) {
    mapping_t* mapping = allocate_mapping(16);
    
    add_mapping_string(mapping, "url", config.url.c_str());
    add_mapping_string(mapping, "protocol", config.protocol.c_str());
    add_mapping_pair(mapping, "connect_timeout", config.connect_timeout);
    add_mapping_pair(mapping, "ping_interval", config.ping_interval);
    add_mapping_pair(mapping, "max_message_size", static_cast<int>(config.max_message_size));
    add_mapping_pair(mapping, "verify_ssl", config.verify_ssl ? 1 : 0);
    add_mapping_string(mapping, "ca_file", config.ca_file.c_str());
    add_mapping_string(mapping, "user_agent", config.user_agent.c_str());
    add_mapping_string(mapping, "origin", config.origin.c_str());
    add_mapping_pair(mapping, "follow_redirects", config.follow_redirects ? 1 : 0);
    add_mapping_pair(mapping, "enable_compression", config.enable_compression ? 1 : 0);
    
    // Subprotocols
    array_t* subprotocols = allocate_empty_array(config.subprotocols.size());
    for (size_t i = 0; i < config.subprotocols.size(); i++) {
        subprotocols->item[i] = const0;
        subprotocols->item[i].type = T_STRING;
        subprotocols->item[i].subtype = STRING_MALLOC;
        subprotocols->item[i].u.string = string_copy(config.subprotocols[i].c_str(),
                                                    "client_config_mapping");
    }
    add_mapping_array(mapping, "subprotocols", subprotocols);
    
    return mapping;
}

bool validate_client_config(const ws_client_config& config, std::string& error) {
    if (config.url.empty()) {
        error = "URL is required";
        return false;
    }
    
    if (!is_valid_websocket_url(config.url)) {
        error = "Invalid WebSocket URL";
        return false;
    }
    
    if (config.connect_timeout <= 0) {
        error = "Connect timeout must be positive";
        return false;
    }
    
    if (config.max_message_size == 0) {
        error = "Maximum message size must be greater than 0";
        return false;
    }
    
    return true;
}

ws_client_config get_default_client_config() {
    return ws_client_config();
}

bool parse_websocket_url(const std::string& url, std::string& host, int& port,
                        std::string& path, bool& use_ssl) {
    std::regex url_regex(R"(^(wss?):\/\/([^:\/\s]+)(?::(\d+))?(\/.*)?$)");
    std::smatch matches;
    
    if (!std::regex_match(url, matches, url_regex)) {
        return false;
    }
    
    std::string scheme = matches[1].str();
    host = matches[2].str();
    
    if (matches[3].matched) {
        port = std::stoi(matches[3].str());
    } else {
        port = (scheme == "wss") ? 443 : 80;
    }
    
    path = matches[4].matched ? matches[4].str() : "/";
    use_ssl = (scheme == "wss");
    
    return true;
}

bool is_valid_websocket_url(const std::string& url) {
    std::string host, path;
    int port;
    bool use_ssl;
    
    return parse_websocket_url(url, host, port, path, use_ssl) &&
           !host.empty() && port > 0 && port <= 65535;
}

bool is_valid_subprotocol(const std::string& protocol) {
    if (protocol.empty()) {
        return false;
    }
    
    // Basic validation - protocol name should be alphanumeric with some symbols
    std::regex protocol_regex(R"(^[a-zA-Z0-9_.-]+$)");
    return std::regex_match(protocol, protocol_regex);
}