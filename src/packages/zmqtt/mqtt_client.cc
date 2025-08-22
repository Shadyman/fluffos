#include "mqtt_client.h"
#include "mqtt_message.h"
#include "base/package_api.h"
#include "vm/internal/base/mapping.h"
#include <random>
#include <chrono>
#include <sstream>
#include <iomanip>

// MQTTClient implementation

MQTTClient::MQTTClient(int socket_fd) 
    : socket_fd_(socket_fd), state_(MQTT_STATE_DISCONNECTED),
      lws_wsi_(nullptr), lws_context_(nullptr), next_packet_id_(1) {
    
    // Initialize SocketOptionManager for unified socket architecture
    option_manager_ = std::make_unique<SocketOptionManager>(socket_fd);
    
    memset(&stats_, 0, sizeof(stats_));
    stats_.connect_time = time(nullptr);
}

MQTTClient::~MQTTClient() {
    if (is_connected()) {
        disconnect();
    }
}

bool MQTTClient::set_config(const mqtt_connection_config& config) {
    if (state_ != MQTT_STATE_DISCONNECTED) {
        set_error("Cannot change configuration while connected");
        return false;
    }
    
    config_ = config;
    
    // Sync configuration to unified socket option system
    sync_config_to_option_manager();
    
    return validate_config();
}

bool MQTTClient::connect(const std::string& broker_host, int broker_port, 
                        const std::string& client_id) {
    
    if (state_ != MQTT_STATE_DISCONNECTED) {
        set_error("Already connected or connecting");
        return false;
    }
    
    // Update configuration
    config_.broker_host = broker_host;
    if (broker_port > 0) {
        config_.broker_port = broker_port;
    }
    if (!client_id.empty()) {
        config_.client_id = client_id;
    } else if (config_.client_id.empty()) {
        config_.client_id = generate_client_id();
    }
    
    if (!validate_config()) {
        return false;
    }
    
    set_state(MQTT_STATE_CONNECTING);
    
    // The actual libwebsockets connection will be established
    // through the socket system integration
    
    return true;
}

bool MQTTClient::disconnect() {
    if (state_ == MQTT_STATE_DISCONNECTED) {
        return true;
    }
    
    set_state(MQTT_STATE_DISCONNECTING);
    
    // Clear subscriptions
    subscriptions_.clear();
    
    // Clear message queues
    std::lock_guard<std::mutex> lock(queue_mutex_);
    while (!outgoing_queue_.empty()) {
        outgoing_queue_.pop();
    }
    pending_publishes_.clear();
    
    set_state(MQTT_STATE_DISCONNECTED);
    lws_wsi_ = nullptr;
    
    return true;
}

bool MQTTClient::publish(const std::string& topic, const std::string& payload, 
                        int qos, bool retain) {
    
    if (!is_connected()) {
        set_error("Not connected to broker");
        return false;
    }
    
    if (!mqtt::is_valid_publish_topic(topic)) {
        set_error("Invalid topic for publishing");
        return false;
    }
    
    if (!mqtt::validate_qos_level(qos)) {
        set_error("Invalid QoS level");
        return false;
    }
    
    mqtt_publish_message msg;
    msg.topic = topic;
    msg.payload = payload;
    msg.qos = qos;
    msg.retain = retain;
    
    if (qos > 0) {
        msg.packet_id = next_packet_id_++;
        if (next_packet_id_ == 0) next_packet_id_ = 1; // Skip 0
        
        // Track pending publish for QoS > 0
        pending_publishes_[msg.packet_id] = msg;
    }
    
    queue_outgoing_message(msg);
    return process_outgoing_queue();
}

bool MQTTClient::subscribe(const std::string& topic, int qos) {
    if (!is_connected()) {
        set_error("Not connected to broker");
        return false;
    }
    
    if (!mqtt::is_valid_subscribe_filter(topic)) {
        set_error("Invalid topic filter for subscription");
        return false;
    }
    
    if (!mqtt::validate_qos_level(qos)) {
        set_error("Invalid QoS level");
        return false;
    }
    
    // Check if already subscribed
    if (is_subscribed(topic)) {
        return true;
    }
    
    add_subscription(topic, qos);
    
    // Send subscription request via libwebsockets
    if (lws_wsi_) {
        lws_mqtt_subscribe_param_t sub_param;
        lws_mqtt_topic_elem_t topic_elem;
        
        topic_elem.name = topic.c_str();
        topic_elem.qos = static_cast<lws_mqtt_qos_levels_t>(qos);
        topic_elem.acked = 0;
        
        sub_param.num_topics = 1;
        sub_param.topic = &topic_elem;
        sub_param.packet_id = next_packet_id_++;
        
        int result = lws_mqtt_client_send_subcribe(lws_wsi_, &sub_param);
        return (result >= 0);
    }
    
    return false;
}

bool MQTTClient::subscribe(const std::vector<std::string>& topics, 
                          const std::vector<int>& qos_levels) {
    if (topics.size() != qos_levels.size()) {
        set_error("Topic and QoS arrays must have same length");
        return false;
    }
    
    // Validate all topics and QoS levels first
    for (size_t i = 0; i < topics.size(); ++i) {
        if (!mqtt::is_valid_subscribe_filter(topics[i])) {
            set_error("Invalid topic filter: " + topics[i]);
            return false;
        }
        if (!mqtt::validate_qos_level(qos_levels[i])) {
            set_error("Invalid QoS level for topic: " + topics[i]);
            return false;
        }
    }
    
    // Subscribe to all topics
    bool all_success = true;
    for (size_t i = 0; i < topics.size(); ++i) {
        if (!subscribe(topics[i], qos_levels[i])) {
            all_success = false;
        }
    }
    
    return all_success;
}

bool MQTTClient::unsubscribe(const std::string& topic) {
    if (!is_connected()) {
        set_error("Not connected to broker");
        return false;
    }
    
    if (!is_subscribed(topic)) {
        return true; // Already unsubscribed
    }
    
    remove_subscription(topic);
    
    // Send unsubscription request via libwebsockets
    if (lws_wsi_) {
        lws_mqtt_subscribe_param_t unsub_param;
        lws_mqtt_topic_elem_t topic_elem;
        
        topic_elem.name = topic.c_str();
        topic_elem.qos = QOS0;
        topic_elem.acked = 0;
        
        unsub_param.num_topics = 1;
        unsub_param.topic = &topic_elem;
        unsub_param.packet_id = next_packet_id_++;
        
        int result = lws_mqtt_client_send_unsubcribe(lws_wsi_, &unsub_param);
        return (result >= 0);
    }
    
    return false;
}

bool MQTTClient::unsubscribe(const std::vector<std::string>& topics) {
    bool all_success = true;
    
    for (const auto& topic : topics) {
        if (!unsubscribe(topic)) {
            all_success = false;
        }
    }
    
    return all_success;
}

bool MQTTClient::set_will(const std::string& topic, const std::string& message, 
                         int qos, bool retain) {
    if (!mqtt::is_valid_publish_topic(topic)) {
        set_error("Invalid will topic");
        return false;
    }
    
    if (!mqtt::validate_qos_level(qos)) {
        set_error("Invalid will QoS level");
        return false;
    }
    
    config_.will.topic = topic;
    config_.will.message = message;
    config_.will.qos = qos;
    config_.will.retain = retain;
    
    return true;
}

bool MQTTClient::clear_will() {
    config_.will.topic.clear();
    config_.will.message.clear();
    config_.will.qos = 0;
    config_.will.retain = false;
    
    return true;
}

mapping_t* MQTTClient::get_status_mapping() const {
    mapping_t* m = allocate_mapping(20);
    svalue_t key, *value;
    
    // Add socket_fd
    key.type = T_STRING;
    key.u.string = const_cast<char*>("socket_fd");
    value = find_for_insert(m, &key, 1);
    value->type = T_NUMBER;
    value->u.number = socket_fd_;
    
    // Add state
    key.u.string = const_cast<char*>("state");
    value = find_for_insert(m, &key, 1);
    value->type = T_NUMBER;
    value->u.number = static_cast<int>(state_);
    
    // Add broker_host
    key.u.string = const_cast<char*>("broker_host");
    value = find_for_insert(m, &key, 1);
    value->type = T_STRING;
    value->u.string = string_copy(config_.broker_host.c_str(), "mqtt broker_host");
    
    // Add broker_port
    key.u.string = const_cast<char*>("broker_port");
    value = find_for_insert(m, &key, 1);
    value->type = T_NUMBER;
    value->u.number = config_.broker_port;
    
    // Add client_id
    key.u.string = const_cast<char*>("client_id");
    value = find_for_insert(m, &key, 1);
    value->type = T_STRING;
    value->u.string = string_copy(config_.client_id.c_str(), "mqtt client_id");
    
    // Add username
    key.u.string = const_cast<char*>("username");
    value = find_for_insert(m, &key, 1);
    value->type = T_STRING;
    value->u.string = string_copy(config_.username.c_str(), "mqtt username");
    
    // Add keep_alive
    key.u.string = const_cast<char*>("keep_alive");
    value = find_for_insert(m, &key, 1);
    value->type = T_NUMBER;
    value->u.number = config_.keep_alive;
    
    // Add clean_session
    key.u.string = const_cast<char*>("clean_session");
    value = find_for_insert(m, &key, 1);
    value->type = T_NUMBER;
    value->u.number = config_.clean_session ? 1 : 0;
    
    // Add use_tls
    key.u.string = const_cast<char*>("use_tls");
    value = find_for_insert(m, &key, 1);
    value->type = T_NUMBER;
    value->u.number = config_.use_tls ? 1 : 0;
    
    // Add connected
    key.u.string = const_cast<char*>("connected");
    value = find_for_insert(m, &key, 1);
    value->type = T_NUMBER;
    value->u.number = is_connected() ? 1 : 0;
    
    // Subscription information
    array_t* sub_topics = allocate_empty_array(subscriptions_.size());
    array_t* sub_qos = allocate_empty_array(subscriptions_.size());
    
    size_t idx = 0;
    for (const auto& sub : subscriptions_) {
        sub_topics->item[idx].type = T_STRING;
        sub_topics->item[idx].u.string = string_copy(sub.first.c_str(), "mqtt topic");
        
        sub_qos->item[idx].type = T_NUMBER;
        sub_qos->item[idx].u.number = sub.second.qos;
        
        idx++;
    }
    
    // Add subscribed_topics array
    key.u.string = const_cast<char*>("subscribed_topics");
    value = find_for_insert(m, &key, 1);
    value->type = T_ARRAY;
    value->u.arr = sub_topics;
    
    // Add subscription_qos array
    key.u.string = const_cast<char*>("subscription_qos");
    value = find_for_insert(m, &key, 1);
    value->type = T_ARRAY;
    value->u.arr = sub_qos;
    
    // Statistics
    key.u.string = const_cast<char*>("messages_sent");
    value = find_for_insert(m, &key, 1);
    value->type = T_NUMBER;
    value->u.number = stats_.messages_sent;
    
    key.u.string = const_cast<char*>("messages_received");
    value = find_for_insert(m, &key, 1);
    value->type = T_NUMBER;
    value->u.number = stats_.messages_received;
    
    key.u.string = const_cast<char*>("bytes_sent");
    value = find_for_insert(m, &key, 1);
    value->type = T_NUMBER;
    value->u.number = stats_.bytes_sent;
    
    key.u.string = const_cast<char*>("bytes_received");
    value = find_for_insert(m, &key, 1);
    value->type = T_NUMBER;
    value->u.number = stats_.bytes_received;
    
    key.u.string = const_cast<char*>("connect_time");
    value = find_for_insert(m, &key, 1);
    value->type = T_NUMBER;
    value->u.number = stats_.connect_time;
    
    key.u.string = const_cast<char*>("last_activity");
    value = find_for_insert(m, &key, 1);
    value->type = T_NUMBER;
    value->u.number = stats_.last_activity;
    
    // Last Will and Testament
    if (!config_.will.topic.empty()) {
        key.u.string = const_cast<char*>("will_topic");
        value = find_for_insert(m, &key, 1);
        value->type = T_STRING;
        value->u.string = string_copy(config_.will.topic.c_str(), "mqtt will_topic");
        
        key.u.string = const_cast<char*>("will_message");
        value = find_for_insert(m, &key, 1);
        value->type = T_STRING;
        value->u.string = string_copy(config_.will.message.c_str(), "mqtt will_message");
        
        key.u.string = const_cast<char*>("will_qos");
        value = find_for_insert(m, &key, 1);
        value->type = T_NUMBER;
        value->u.number = config_.will.qos;
        
        key.u.string = const_cast<char*>("will_retain");
        value = find_for_insert(m, &key, 1);
        value->type = T_NUMBER;
        value->u.number = config_.will.retain ? 1 : 0;
    }
    
    // Error information
    if (!last_error_.empty()) {
        key.u.string = const_cast<char*>("last_error");
        value = find_for_insert(m, &key, 1);
        value->type = T_STRING;
        value->u.string = string_copy(last_error_.c_str(), "mqtt last_error");
    }
    
    return m;
}

void MQTTClient::set_state(mqtt_client_state new_state) {
    state_ = new_state;
    stats_.last_activity = time(nullptr);
}

void MQTTClient::set_error(const std::string& error) {
    last_error_ = error;
    set_state(MQTT_STATE_ERROR);
}

void MQTTClient::queue_outgoing_message(const mqtt_publish_message& msg) {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    outgoing_queue_.push(msg);
}

bool MQTTClient::process_outgoing_queue() {
    if (!lws_wsi_) {
        return false;
    }
    
    std::lock_guard<std::mutex> lock(queue_mutex_);
    
    while (!outgoing_queue_.empty()) {
        const auto& msg = outgoing_queue_.front();
        
        lws_mqtt_publish_param_t pub_param;
        if (!mqtt::create_publish_message(msg.topic, msg.payload, msg.qos, msg.retain)->to_lws_publish_param(pub_param)) {
            outgoing_queue_.pop();
            continue;
        }
        
        int result = lws_mqtt_client_send_publish(lws_wsi_, &pub_param, 
                                                 msg.payload.c_str(), 
                                                 msg.payload.length(), 
                                                 LWS_MQTT_FINAL_PART);
        
        if (result >= 0) {
            stats_.messages_sent++;
            stats_.bytes_sent += msg.payload.length();
            outgoing_queue_.pop();
        } else {
            set_error("Failed to send publish message");
            return false;
        }
    }
    
    return true;
}

void MQTTClient::add_subscription(const std::string& topic, int qos) {
    subscriptions_[topic] = mqtt_subscription(topic, qos);
}

void MQTTClient::remove_subscription(const std::string& topic) {
    subscriptions_.erase(topic);
}

bool MQTTClient::is_subscribed(const std::string& topic) const {
    return subscriptions_.find(topic) != subscriptions_.end();
}

int MQTTClient::handle_callback(enum lws_callback_reasons reason, void* user, 
                               void* in, size_t len) {
    switch (reason) {
        case LWS_CALLBACK_MQTT_CLIENT_ESTABLISHED:
            return handle_client_established();
        
        case LWS_CALLBACK_MQTT_CLIENT_WRITEABLE:
            return handle_client_writeable();
        
        case LWS_CALLBACK_MQTT_CLIENT_RX:
            return handle_mqtt_client_rx(in, len);
        
        case LWS_CALLBACK_MQTT_CLIENT_CLOSED:
            return handle_client_closed();
        
        case LWS_CALLBACK_MQTT_SUBSCRIBED:
            return handle_mqtt_subscribed(in, len);
        
        case LWS_CALLBACK_MQTT_UNSUBSCRIBED:
            return handle_mqtt_unsubscribed(in, len);
        
        default:
            break;
    }
    
    return 0;
}

int MQTTClient::handle_client_established() {
    set_state(MQTT_STATE_CONNECTED);
    stats_.connect_time = time(nullptr);
    
    invoke_connect_callback(0); // Success
    return 0;
}

int MQTTClient::handle_client_writeable() {
    process_outgoing_queue();
    return 0;
}

int MQTTClient::handle_client_closed() {
    set_state(MQTT_STATE_DISCONNECTED);
    
    invoke_disconnect_callback(0); // Normal disconnect
    return 0;
}

int MQTTClient::handle_mqtt_subscribed(void* in, size_t len) {
    // Handle subscription acknowledgment
    if (in && len > 0) {
        // Parse SUBACK response
        // This is a simplified implementation
        invoke_subscribe_callback("", 0);
    }
    
    return 0;
}

int MQTTClient::handle_mqtt_unsubscribed(void* in, size_t len) {
    // Handle unsubscription acknowledgment
    if (in && len > 0) {
        // Parse UNSUBACK response
        invoke_unsubscribe_callback("");
    }
    
    return 0;
}

int MQTTClient::handle_mqtt_client_rx(void* in, size_t len) {
    // Handle incoming MQTT message
    if (!in || len == 0) {
        return 0;
    }
    
    // This is a simplified implementation
    // In a full implementation, we would parse the MQTT message properly
    
    stats_.messages_received++;
    stats_.bytes_received += len;
    stats_.last_activity = time(nullptr);
    
    // For now, invoke a generic message callback
    invoke_message_callback("test/topic", std::string(static_cast<const char*>(in), len), 0, false);
    
    return 0;
}

void MQTTClient::invoke_lpc_callback(const std::string& callback_name, 
                                   const std::vector<svalue_t>& args) {
    // This would invoke LPC callbacks in the actual implementation
    // For now, this is a placeholder
}

void MQTTClient::invoke_connect_callback(int result_code) {
    std::vector<svalue_t> args(2);
    args[0].type = T_NUMBER;
    args[0].u.number = socket_fd_;
    args[1].type = T_NUMBER;
    args[1].u.number = result_code;
    
    invoke_lpc_callback("mqtt_connect_callback", args);
}

void MQTTClient::invoke_disconnect_callback(int reason_code) {
    std::vector<svalue_t> args(2);
    args[0].type = T_NUMBER;
    args[0].u.number = socket_fd_;
    args[1].type = T_NUMBER;
    args[1].u.number = reason_code;
    
    invoke_lpc_callback("mqtt_disconnect_callback", args);
}

void MQTTClient::invoke_message_callback(const std::string& topic, 
                                       const std::string& payload, 
                                       int qos, bool retain) {
    std::vector<svalue_t> args(5);
    args[0].type = T_NUMBER;
    args[0].u.number = socket_fd_;
    args[1].type = T_STRING;
    args[1].u.string = string_copy(topic.c_str(), "mqtt topic");
    args[2].type = T_STRING;
    args[2].u.string = string_copy(payload.c_str(), "mqtt payload");
    args[3].type = T_NUMBER;
    args[3].u.number = qos;
    args[4].type = T_NUMBER;
    args[4].u.number = retain ? 1 : 0;
    
    invoke_lpc_callback("mqtt_message_callback", args);
}

void MQTTClient::invoke_subscribe_callback(const std::string& topic, int granted_qos) {
    std::vector<svalue_t> args(3);
    args[0].type = T_NUMBER;
    args[0].u.number = socket_fd_;
    args[1].type = T_STRING;
    args[1].u.string = string_copy(topic.c_str(), "mqtt topic");
    args[2].type = T_NUMBER;
    args[2].u.number = granted_qos;
    
    invoke_lpc_callback("mqtt_subscribe_callback", args);
}

void MQTTClient::invoke_unsubscribe_callback(const std::string& topic) {
    std::vector<svalue_t> args(2);
    args[0].type = T_NUMBER;
    args[0].u.number = socket_fd_;
    args[1].type = T_STRING;
    args[1].u.string = string_copy(topic.c_str(), "mqtt topic");
    
    invoke_lpc_callback("mqtt_unsubscribe_callback", args);
}

void MQTTClient::invoke_publish_callback(int packet_id, int result) {
    std::vector<svalue_t> args(3);
    args[0].type = T_NUMBER;
    args[0].u.number = socket_fd_;
    args[1].type = T_NUMBER;
    args[1].u.number = packet_id;
    args[2].type = T_NUMBER;
    args[2].u.number = result;
    
    invoke_lpc_callback("mqtt_publish_callback", args);
}

std::string MQTTClient::generate_client_id() const {
    return mqtt::generate_random_client_id();
}

bool MQTTClient::validate_config() const {
    if (config_.broker_host.empty()) {
        const_cast<MQTTClient*>(this)->set_error("Broker host not specified");
        return false;
    }
    
    if (config_.broker_port <= 0 || config_.broker_port > 65535) {
        const_cast<MQTTClient*>(this)->set_error("Invalid broker port");
        return false;
    }
    
    if (config_.client_id.length() > MQTT_MAX_CLIENT_ID_LEN) {
        const_cast<MQTTClient*>(this)->set_error("Client ID too long");
        return false;
    }
    
    if (config_.keep_alive < 0 || config_.keep_alive > 65535) {
        const_cast<MQTTClient*>(this)->set_error("Invalid keep-alive interval");
        return false;
    }
    
    return true;
}

// MQTT utility functions

namespace mqtt {

std::unique_ptr<MQTTClient> create_client(int socket_fd) {
    return std::make_unique<MQTTClient>(socket_fd);
}

bool validate_client_config(const mqtt_connection_config& config) {
    return !config.broker_host.empty() && 
           config.broker_port > 0 && config.broker_port <= 65535 &&
           config.client_id.length() <= MQTT_MAX_CLIENT_ID_LEN &&
           config.keep_alive >= 0 && config.keep_alive <= 65535;
}

bool mapping_to_config(const mapping_t* m, mqtt_connection_config& config) {
    if (!m) return false;
    
    svalue_t* val;
    
    if ((val = find_string_in_mapping(m, "broker_host")) && val->type == T_STRING) {
        config.broker_host = val->u.string;
    }
    
    if ((val = find_string_in_mapping(m, "broker_port")) && val->type == T_NUMBER) {
        config.broker_port = static_cast<int>(val->u.number);
    }
    
    if ((val = find_string_in_mapping(m, "client_id")) && val->type == T_STRING) {
        config.client_id = val->u.string;
    }
    
    if ((val = find_string_in_mapping(m, "username")) && val->type == T_STRING) {
        config.username = val->u.string;
    }
    
    if ((val = find_string_in_mapping(m, "password")) && val->type == T_STRING) {
        config.password = val->u.string;
    }
    
    if ((val = find_string_in_mapping(m, "keep_alive")) && val->type == T_NUMBER) {
        config.keep_alive = static_cast<int>(val->u.number);
    }
    
    if ((val = find_string_in_mapping(m, "clean_session")) && val->type == T_NUMBER) {
        config.clean_session = (val->u.number != 0);
    }
    
    if ((val = find_string_in_mapping(m, "use_tls")) && val->type == T_NUMBER) {
        config.use_tls = (val->u.number != 0);
    }
    
    return true;
}

mapping_t* config_to_mapping(const mqtt_connection_config& config) {
    mapping_t* m = allocate_mapping(10);
    svalue_t key, *value;
    
    key.type = T_STRING;
    
    // Add broker_host
    key.u.string = const_cast<char*>("broker_host");
    value = find_for_insert(m, &key, 1);
    value->type = T_STRING;
    value->u.string = string_copy(config.broker_host.c_str(), "mqtt config broker_host");
    
    // Add broker_port
    key.u.string = const_cast<char*>("broker_port");
    value = find_for_insert(m, &key, 1);
    value->type = T_NUMBER;
    value->u.number = config.broker_port;
    
    // Add client_id
    key.u.string = const_cast<char*>("client_id");
    value = find_for_insert(m, &key, 1);
    value->type = T_STRING;
    value->u.string = string_copy(config.client_id.c_str(), "mqtt config client_id");
    
    // Add username
    key.u.string = const_cast<char*>("username");
    value = find_for_insert(m, &key, 1);
    value->type = T_STRING;
    value->u.string = string_copy(config.username.c_str(), "mqtt config username");
    
    // Add password
    key.u.string = const_cast<char*>("password");
    value = find_for_insert(m, &key, 1);
    value->type = T_STRING;
    value->u.string = string_copy(config.password.c_str(), "mqtt config password");
    
    // Add keep_alive
    key.u.string = const_cast<char*>("keep_alive");
    value = find_for_insert(m, &key, 1);
    value->type = T_NUMBER;
    value->u.number = config.keep_alive;
    
    // Add clean_session
    key.u.string = const_cast<char*>("clean_session");
    value = find_for_insert(m, &key, 1);
    value->type = T_NUMBER;
    value->u.number = config.clean_session ? 1 : 0;
    
    // Add use_tls
    key.u.string = const_cast<char*>("use_tls");
    value = find_for_insert(m, &key, 1);
    value->type = T_NUMBER;
    value->u.number = config.use_tls ? 1 : 0;
    
    return m;
}

std::string generate_random_client_id() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, 15);
    
    std::ostringstream oss;
    oss << "fluffos_";
    
    for (int i = 0; i < 16; ++i) {
        oss << std::hex << dis(gen);
    }
    
    return oss.str();
}

bool validate_topic_name(const std::string& topic, bool is_subscription) {
    if (topic.empty() || topic.length() > MQTT_MAX_TOPIC_LEN) {
        return false;
    }
    
    if (is_subscription) {
        return is_valid_subscribe_filter(topic);
    } else {
        return is_valid_publish_topic(topic);
    }
}

bool validate_qos_level(int qos) {
    return qos >= 0 && qos <= 1; // libwebsockets doesn't support QoS 2
}

} // namespace mqtt

// MQTTClient implementation methods

void MQTTClient::sync_config_to_option_manager() {
    if (!option_manager_) return;
    
    svalue_t value;
    
    // Sync MQTT_BROKER option
    if (!config_.broker_host.empty()) {
        value.type = T_STRING;
        value.u.string = make_shared_string(config_.broker_host.c_str());
        option_manager_->set_option(MQTT_BROKER, &value);
        free_string(value.u.string);
    }
    
    // Sync MQTT_CLIENT_ID option
    if (!config_.client_id.empty()) {
        value.type = T_STRING;
        value.u.string = make_shared_string(config_.client_id.c_str());
        option_manager_->set_option(MQTT_CLIENT_ID, &value);
        free_string(value.u.string);
    }
    
    // Sync MQTT_USERNAME option
    if (!config_.username.empty()) {
        value.type = T_STRING;
        value.u.string = make_shared_string(config_.username.c_str());
        option_manager_->set_option(MQTT_USERNAME, &value);
        free_string(value.u.string);
    }
    
    // Sync MQTT_PASSWORD option  
    if (!config_.password.empty()) {
        value.type = T_STRING;
        value.u.string = make_shared_string(config_.password.c_str());
        option_manager_->set_option(MQTT_PASSWORD, &value);
        free_string(value.u.string);
    }
    
    // Sync MQTT_KEEP_ALIVE option
    value.type = T_NUMBER;
    value.u.number = config_.keep_alive;
    option_manager_->set_option(MQTT_KEEP_ALIVE, &value);
    
    // Sync MQTT_CLEAN_SESSION option
    value.type = T_NUMBER;
    value.u.number = config_.clean_session ? 1 : 0;
    option_manager_->set_option(MQTT_CLEAN_SESSION, &value);
    
    // Sync MQTT_WILL_TOPIC option
    if (!config_.will.topic.empty()) {
        value.type = T_STRING;
        value.u.string = make_shared_string(config_.will.topic.c_str());
        option_manager_->set_option(MQTT_WILL_TOPIC, &value);
        free_string(value.u.string);
    }
    
    // Sync MQTT_WILL_MESSAGE option
    if (!config_.will.message.empty()) {
        value.type = T_STRING;
        value.u.string = make_shared_string(config_.will.message.c_str());
        option_manager_->set_option(MQTT_WILL_MESSAGE, &value);
        free_string(value.u.string);
    }
}