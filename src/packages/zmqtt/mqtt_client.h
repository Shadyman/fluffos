#ifndef MQTT_CLIENT_H_
#define MQTT_CLIENT_H_

#include "mqtt.h"
#include <vector>
#include <queue>
#include <mutex>
#include <memory>

/*
 * MQTTClient class - manages individual MQTT client connections
 * 
 * This class handles:
 * - Connection management to MQTT broker
 * - Message publishing and subscription
 * - Callback handling from libwebsockets
 * - State management and error handling
 * - Integration with FluffOS socket system
 */

class MQTTClient {
public:
    explicit MQTTClient(int socket_fd);
    ~MQTTClient();
    
    // Configuration management
    bool set_config(const mqtt_connection_config& config);
    const mqtt_connection_config& get_config() const { return config_; }
    
    // Connection management
    bool connect(const std::string& broker_host, int broker_port = 0, 
                const std::string& client_id = "");
    bool disconnect();
    bool is_connected() const { return state_ == MQTT_STATE_CONNECTED; }
    mqtt_client_state get_state() const { return state_; }
    
    // Publishing
    bool publish(const std::string& topic, const std::string& payload, 
                int qos = 0, bool retain = false);
    
    // Subscription management
    bool subscribe(const std::string& topic, int qos = 0);
    bool subscribe(const std::vector<std::string>& topics, 
                  const std::vector<int>& qos_levels);
    bool unsubscribe(const std::string& topic);
    bool unsubscribe(const std::vector<std::string>& topics);
    
    // Last Will and Testament
    bool set_will(const std::string& topic, const std::string& message, 
                 int qos = 0, bool retain = false);
    bool clear_will();
    
    // Status and diagnostics
    mapping_t* get_status_mapping() const;
    std::string get_last_error() const { return last_error_; }
    
    // libwebsockets integration
    void set_lws_wsi(struct lws* wsi) { lws_wsi_ = wsi; }
    struct lws* get_lws_wsi() const { return lws_wsi_; }
    
    // Callback handlers from libwebsockets
    int handle_callback(enum lws_callback_reasons reason, void* user, 
                       void* in, size_t len);

private:
    // Internal state management
    void set_state(mqtt_client_state new_state);
    void set_error(const std::string& error);
    
    // Message queue management
    void queue_outgoing_message(const mqtt_publish_message& msg);
    bool process_outgoing_queue();
    
    // Subscription tracking
    void add_subscription(const std::string& topic, int qos);
    void remove_subscription(const std::string& topic);
    bool is_subscribed(const std::string& topic) const;
    
    // libwebsockets callback handlers
    int handle_client_established();
    int handle_client_writeable();
    int handle_client_receive(void* in, size_t len);
    int handle_client_closed();
    int handle_mqtt_subscribed(void* in, size_t len);
    int handle_mqtt_unsubscribed(void* in, size_t len);
    int handle_mqtt_client_rx(void* in, size_t len);
    
    // LPC callback invocation
    void invoke_lpc_callback(const std::string& callback_name, 
                           const std::vector<svalue_t>& args);
    void invoke_connect_callback(int result_code);
    void invoke_disconnect_callback(int reason_code);
    void invoke_message_callback(const std::string& topic, 
                               const std::string& payload, int qos, bool retain);
    void invoke_subscribe_callback(const std::string& topic, int granted_qos);
    void invoke_unsubscribe_callback(const std::string& topic);
    void invoke_publish_callback(int packet_id, int result);
    
    // Generate unique client ID if not provided
    std::string generate_client_id() const;
    
    // Validate configuration
    bool validate_config() const;
    
private:
    int socket_fd_;
    mqtt_client_state state_;
    mqtt_connection_config config_;
    std::string last_error_;
    
    // libwebsockets connection
    struct lws* lws_wsi_;
    struct lws_context* lws_context_;
    
    // Subscription tracking
    std::map<std::string, mqtt_subscription> subscriptions_;
    
    // Message queues
    std::queue<mqtt_publish_message> outgoing_queue_;
    std::mutex queue_mutex_;
    
    // Packet ID management for QoS > 0
    uint16_t next_packet_id_;
    std::map<uint16_t, mqtt_publish_message> pending_publishes_;
    
    // Statistics
    struct {
        uint64_t messages_sent;
        uint64_t messages_received;
        uint64_t bytes_sent;
        uint64_t bytes_received;
        uint64_t connect_time;
        uint64_t last_activity;
    } stats_;
};

// MQTT client factory functions
namespace mqtt {
    // Create and initialize MQTT client
    std::unique_ptr<MQTTClient> create_client(int socket_fd);
    
    // Validate MQTT configuration
    bool validate_client_config(const mqtt_connection_config& config);
    
    // Convert LPC mapping to MQTT config
    bool mapping_to_config(const mapping_t* m, mqtt_connection_config& config);
    
    // Convert MQTT config to LPC mapping  
    mapping_t* config_to_mapping(const mqtt_connection_config& config);
    
    // Generate random client ID
    std::string generate_random_client_id();
    
    // Validate topic name according to MQTT specification
    bool validate_topic_name(const std::string& topic, bool is_subscription = false);
    
    // Validate QoS level
    bool validate_qos_level(int qos);
}

#endif  // MQTT_CLIENT_H_