#ifndef MQTT_MESSAGE_H_
#define MQTT_MESSAGE_H_

#include "mqtt.h"
#include <ctime>

/*
 * MQTTMessage class - represents MQTT messages for publish/subscribe
 * 
 * This class handles:
 * - Message serialization and deserialization
 * - Topic validation and wildcards
 * - QoS level management
 * - Message properties and metadata
 * - Integration with libwebsockets message structures
 */

class MQTTMessage {
public:
    MQTTMessage();
    MQTTMessage(const std::string& topic, const std::string& payload, 
               int qos = 0, bool retain = false);
    ~MQTTMessage();
    
    // Message properties
    void set_topic(const std::string& topic);
    void set_payload(const std::string& payload);
    void set_payload(const void* data, size_t length);
    void set_qos(int qos);
    void set_retain(bool retain);
    void set_dup(bool dup);
    void set_packet_id(uint16_t packet_id);
    
    const std::string& get_topic() const { return topic_; }
    const std::string& get_payload() const { return payload_; }
    const void* get_payload_data() const { return payload_.c_str(); }
    size_t get_payload_size() const { return payload_.size(); }
    int get_qos() const { return qos_; }
    bool get_retain() const { return retain_; }
    bool get_dup() const { return dup_; }
    uint16_t get_packet_id() const { return packet_id_; }
    
    // Message metadata
    void set_timestamp(time_t timestamp = 0);
    time_t get_timestamp() const { return timestamp_; }
    void set_client_id(const std::string& client_id);
    const std::string& get_client_id() const { return client_id_; }
    
    // Validation
    bool is_valid() const;
    std::string get_validation_error() const;
    
    // Topic utilities
    bool topic_matches_filter(const std::string& filter) const;
    static bool is_valid_topic_name(const std::string& topic, bool is_filter = false);
    static bool is_valid_topic_filter(const std::string& filter);
    
    // Serialization for libwebsockets
    bool to_lws_publish_param(lws_mqtt_publish_param_t& param) const;
    bool from_lws_publish_param(const lws_mqtt_publish_param_t& param);
    
    // LPC integration
    mapping_t* to_lpc_mapping() const;
    bool from_lpc_mapping(const mapping_t* m);
    
    // Binary payload support
    void set_binary_payload(const void* data, size_t length);
    bool is_binary_payload() const { return is_binary_; }
    
    // Message properties (MQTT 5.0)
    void set_content_type(const std::string& content_type);
    void set_response_topic(const std::string& response_topic);
    void set_correlation_data(const std::string& correlation_data);
    void set_user_property(const std::string& key, const std::string& value);
    void set_message_expiry_interval(uint32_t expiry);
    
    const std::string& get_content_type() const { return content_type_; }
    const std::string& get_response_topic() const { return response_topic_; }
    const std::string& get_correlation_data() const { return correlation_data_; }
    const std::map<std::string, std::string>& get_user_properties() const { return user_properties_; }
    uint32_t get_message_expiry_interval() const { return message_expiry_interval_; }
    
    // Statistics and diagnostics
    size_t get_total_size() const;
    std::string get_debug_string() const;
    
private:
    // Core message data
    std::string topic_;
    std::string payload_;
    int qos_;
    bool retain_;
    bool dup_;
    uint16_t packet_id_;
    
    // Message metadata
    time_t timestamp_;
    std::string client_id_;
    bool is_binary_;
    
    // MQTT 5.0 properties
    std::string content_type_;
    std::string response_topic_;
    std::string correlation_data_;
    std::map<std::string, std::string> user_properties_;
    uint32_t message_expiry_interval_;
    
    // Validation state
    mutable std::string validation_error_;
    
    // Helper methods
    bool validate_topic() const;
    bool validate_qos() const;
    bool validate_payload() const;
};

// MQTT message utilities
namespace mqtt {
    // Topic matching for subscriptions
    bool topic_matches(const std::string& topic, const std::string& filter);
    
    // Topic validation
    bool is_valid_publish_topic(const std::string& topic);
    bool is_valid_subscribe_filter(const std::string& filter);
    
    // QoS utilities
    int min_qos(int qos1, int qos2);
    int max_qos(int qos1, int qos2);
    const char* qos_to_string(int qos);
    
    // Message encoding/decoding
    std::string encode_mqtt_string(const std::string& str);
    bool decode_mqtt_string(const void* data, size_t length, std::string& result);
    
    // Binary data utilities
    std::string binary_to_hex(const void* data, size_t length);
    bool hex_to_binary(const std::string& hex, std::vector<uint8_t>& binary);
    
    // Message factory functions
    std::unique_ptr<MQTTMessage> create_publish_message(
        const std::string& topic, const std::string& payload, 
        int qos = 0, bool retain = false);
    
    std::unique_ptr<MQTTMessage> create_will_message(
        const std::string& topic, const std::string& message,
        int qos = 0, bool retain = false);
    
    // LPC callback message creation
    svalue_t* create_message_callback_args(const MQTTMessage& message);
    void free_message_callback_args(svalue_t* args, int argc);
}

// MQTT message queue for handling multiple messages
class MQTTMessageQueue {
public:
    MQTTMessageQueue(size_t max_size = 1000);
    ~MQTTMessageQueue();
    
    bool enqueue(std::unique_ptr<MQTTMessage> message);
    std::unique_ptr<MQTTMessage> dequeue();
    
    size_t size() const;
    bool empty() const;
    bool full() const;
    void clear();
    
    // Priority handling (QoS-based)
    bool enqueue_priority(std::unique_ptr<MQTTMessage> message);
    
private:
    std::queue<std::unique_ptr<MQTTMessage>> queue_;
    std::queue<std::unique_ptr<MQTTMessage>> priority_queue_;
    size_t max_size_;
    mutable std::mutex mutex_;
};

#endif  // MQTT_MESSAGE_H_