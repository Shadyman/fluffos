#include "mqtt_message.h"
#include "base/package_api.h"
#include <algorithm>
#include <random>
#include <sstream>
#include <iomanip>
#include <cstring>

// MQTTMessage implementation

MQTTMessage::MQTTMessage() 
    : qos_(0), retain_(false), dup_(false), packet_id_(0),
      timestamp_(0), is_binary_(false), message_expiry_interval_(0) {
    set_timestamp();
}

MQTTMessage::MQTTMessage(const std::string& topic, const std::string& payload, 
                        int qos, bool retain)
    : topic_(topic), payload_(payload), qos_(qos), retain_(retain),
      dup_(false), packet_id_(0), timestamp_(0), is_binary_(false),
      message_expiry_interval_(0) {
    set_timestamp();
}

MQTTMessage::~MQTTMessage() = default;

void MQTTMessage::set_topic(const std::string& topic) {
    topic_ = topic;
}

void MQTTMessage::set_payload(const std::string& payload) {
    payload_ = payload;
    is_binary_ = false;
}

void MQTTMessage::set_payload(const void* data, size_t length) {
    payload_.assign(static_cast<const char*>(data), length);
    is_binary_ = true;
}

void MQTTMessage::set_qos(int qos) {
    if (qos >= 0 && qos <= 2) {
        qos_ = qos;
    }
}

void MQTTMessage::set_retain(bool retain) {
    retain_ = retain;
}

void MQTTMessage::set_dup(bool dup) {
    dup_ = dup;
}

void MQTTMessage::set_packet_id(uint16_t packet_id) {
    packet_id_ = packet_id;
}

void MQTTMessage::set_timestamp(time_t timestamp) {
    timestamp_ = (timestamp == 0) ? time(nullptr) : timestamp;
}

void MQTTMessage::set_client_id(const std::string& client_id) {
    client_id_ = client_id;
}

bool MQTTMessage::is_valid() const {
    validation_error_.clear();
    
    if (!validate_topic()) return false;
    if (!validate_qos()) return false;
    if (!validate_payload()) return false;
    
    return true;
}

std::string MQTTMessage::get_validation_error() const {
    return validation_error_;
}

bool MQTTMessage::validate_topic() const {
    if (topic_.empty()) {
        validation_error_ = "Topic cannot be empty";
        return false;
    }
    
    if (topic_.length() > MQTT_MAX_TOPIC_LEN) {
        validation_error_ = "Topic length exceeds maximum";
        return false;
    }
    
    // Check for invalid characters
    for (char c : topic_) {
        if (c == '+' || c == '#') {
            validation_error_ = "Topic contains wildcard characters";
            return false;
        }
        if (c == '\0') {
            validation_error_ = "Topic contains null character";
            return false;
        }
    }
    
    return true;
}

bool MQTTMessage::validate_qos() const {
    if (qos_ < 0 || qos_ > 2) {
        validation_error_ = "Invalid QoS level";
        return false;
    }
    
    // Note: libwebsockets doesn't support QoS 2
    if (qos_ == 2) {
        validation_error_ = "QoS 2 not supported by libwebsockets";
        return false;
    }
    
    return true;
}

bool MQTTMessage::validate_payload() const {
    if (payload_.length() > MQTT_MAX_PAYLOAD_LEN) {
        validation_error_ = "Payload length exceeds maximum";
        return false;
    }
    
    return true;
}

bool MQTTMessage::topic_matches_filter(const std::string& filter) const {
    return mqtt::topic_matches(topic_, filter);
}

bool MQTTMessage::is_valid_topic_name(const std::string& topic, bool is_filter) {
    if (topic.empty() || topic.length() > MQTT_MAX_TOPIC_LEN) {
        return false;
    }
    
    // Check for null characters
    if (topic.find('\0') != std::string::npos) {
        return false;
    }
    
    if (is_filter) {
        // Subscription filters can contain wildcards
        return mqtt::is_valid_subscribe_filter(topic);
    } else {
        // Publish topics cannot contain wildcards
        return mqtt::is_valid_publish_topic(topic);
    }
}

bool MQTTMessage::is_valid_topic_filter(const std::string& filter) {
    return is_valid_topic_name(filter, true);
}

bool MQTTMessage::to_lws_publish_param(lws_mqtt_publish_param_t& param) const {
    if (!is_valid()) {
        return false;
    }
    
    // Clear the parameter structure
    memset(&param, 0, sizeof(param));
    
    // Set topic (note: lws expects non-const char*)
    param.topic = const_cast<char*>(topic_.c_str());
    param.topic_len = static_cast<uint16_t>(topic_.length());
    
    // Set payload
    param.payload = payload_.c_str();
    param.payload_len = static_cast<uint32_t>(payload_.length());
    param.payload_pos = 0;
    
    // Set QoS
    switch (qos_) {
        case 0: param.qos = QOS0; break;
        case 1: param.qos = QOS1; break;
        default:
            return false; // QoS 2 not supported
    }
    
    // Set packet ID for QoS > 0
    param.packet_id = packet_id_;
    param.dup = dup_ ? 1 : 0;
    
    return true;
}

bool MQTTMessage::from_lws_publish_param(const lws_mqtt_publish_param_t& param) {
    // Extract topic
    if (param.topic && param.topic_len > 0) {
        topic_.assign(param.topic, param.topic_len);
    }
    
    // Extract payload
    if (param.payload && param.payload_len > 0) {
        payload_.assign(static_cast<const char*>(param.payload), param.payload_len);
    }
    
    // Extract QoS
    switch (param.qos) {
        case QOS0: qos_ = 0; break;
        case QOS1: qos_ = 1; break;
        default: qos_ = 0; break;
    }
    
    // Extract other fields
    packet_id_ = param.packet_id;
    dup_ = (param.dup != 0);
    
    set_timestamp();
    return true;
}

mapping_t* MQTTMessage::to_lpc_mapping() const {
    mapping_t* m = allocate_mapping(16);
    
    m = add_mapping_string(m, "topic", topic_.c_str());
    m = add_mapping_string(m, "payload", payload_.c_str());
    m = add_mapping_pair(m, "qos", number(qos_));
    m = add_mapping_pair(m, "retain", number(retain_ ? 1 : 0));
    m = add_mapping_pair(m, "dup", number(dup_ ? 1 : 0));
    m = add_mapping_pair(m, "packet_id", number(packet_id_));
    m = add_mapping_pair(m, "timestamp", number(timestamp_));
    m = add_mapping_string(m, "client_id", client_id_.c_str());
    m = add_mapping_pair(m, "is_binary", number(is_binary_ ? 1 : 0));
    m = add_mapping_pair(m, "payload_size", number(payload_.size()));
    
    // MQTT 5.0 properties
    if (!content_type_.empty()) {
        m = add_mapping_string(m, "content_type", content_type_.c_str());
    }
    if (!response_topic_.empty()) {
        m = add_mapping_string(m, "response_topic", response_topic_.c_str());
    }
    if (!correlation_data_.empty()) {
        m = add_mapping_string(m, "correlation_data", correlation_data_.c_str());
    }
    if (message_expiry_interval_ > 0) {
        m = add_mapping_pair(m, "message_expiry_interval", number(message_expiry_interval_));
    }
    
    return m;
}

bool MQTTMessage::from_lpc_mapping(const mapping_t* m) {
    if (!m) return false;
    
    svalue_t* val;
    
    // Extract topic
    if ((val = find_string_in_mapping(m, "topic")) && val->type == T_STRING) {
        topic_ = val->u.string;
    }
    
    // Extract payload
    if ((val = find_string_in_mapping(m, "payload")) && val->type == T_STRING) {
        payload_ = val->u.string;
    }
    
    // Extract QoS
    if ((val = find_string_in_mapping(m, "qos")) && val->type == T_NUMBER) {
        qos_ = static_cast<int>(val->u.number);
    }
    
    // Extract retain flag
    if ((val = find_string_in_mapping(m, "retain")) && val->type == T_NUMBER) {
        retain_ = (val->u.number != 0);
    }
    
    // Extract dup flag
    if ((val = find_string_in_mapping(m, "dup")) && val->type == T_NUMBER) {
        dup_ = (val->u.number != 0);
    }
    
    // Extract packet ID
    if ((val = find_string_in_mapping(m, "packet_id")) && val->type == T_NUMBER) {
        packet_id_ = static_cast<uint16_t>(val->u.number);
    }
    
    // Extract client ID
    if ((val = find_string_in_mapping(m, "client_id")) && val->type == T_STRING) {
        client_id_ = val->u.string;
    }
    
    // Extract binary flag
    if ((val = find_string_in_mapping(m, "is_binary")) && val->type == T_NUMBER) {
        is_binary_ = (val->u.number != 0);
    }
    
    set_timestamp();
    return true;
}

void MQTTMessage::set_binary_payload(const void* data, size_t length) {
    set_payload(data, length);
}

void MQTTMessage::set_content_type(const std::string& content_type) {
    content_type_ = content_type;
}

void MQTTMessage::set_response_topic(const std::string& response_topic) {
    response_topic_ = response_topic;
}

void MQTTMessage::set_correlation_data(const std::string& correlation_data) {
    correlation_data_ = correlation_data;
}

void MQTTMessage::set_user_property(const std::string& key, const std::string& value) {
    user_properties_[key] = value;
}

void MQTTMessage::set_message_expiry_interval(uint32_t expiry) {
    message_expiry_interval_ = expiry;
}

size_t MQTTMessage::get_total_size() const {
    return topic_.size() + payload_.size() + sizeof(*this);
}

std::string MQTTMessage::get_debug_string() const {
    std::ostringstream oss;
    oss << "MQTTMessage{topic=\"" << topic_ << "\", payload_size=" << payload_.size()
        << ", qos=" << qos_ << ", retain=" << retain_ << ", dup=" << dup_
        << ", packet_id=" << packet_id_ << ", timestamp=" << timestamp_ << "}";
    return oss.str();
}

// MQTT utility functions

namespace mqtt {

bool topic_matches(const std::string& topic, const std::string& filter) {
    // Simple implementation of MQTT topic matching
    // This should handle + and # wildcards properly
    
    if (filter == "#") {
        return true; // Matches everything
    }
    
    std::vector<std::string> topic_levels, filter_levels;
    
    // Split topic and filter by '/'
    std::stringstream topic_ss(topic), filter_ss(filter);
    std::string level;
    
    while (std::getline(topic_ss, level, '/')) {
        topic_levels.push_back(level);
    }
    
    while (std::getline(filter_ss, level, '/')) {
        filter_levels.push_back(level);
    }
    
    size_t topic_idx = 0, filter_idx = 0;
    
    while (filter_idx < filter_levels.size() && topic_idx < topic_levels.size()) {
        const std::string& filter_level = filter_levels[filter_idx];
        const std::string& topic_level = topic_levels[topic_idx];
        
        if (filter_level == "#") {
            return true; // # matches all remaining levels
        } else if (filter_level == "+") {
            // + matches exactly one level
            topic_idx++;
            filter_idx++;
        } else if (filter_level == topic_level) {
            // Exact match
            topic_idx++;
            filter_idx++;
        } else {
            return false; // No match
        }
    }
    
    // Both should be fully consumed for a match, unless filter ends with #
    if (filter_idx < filter_levels.size() && filter_levels[filter_idx] == "#") {
        return true;
    }
    
    return (topic_idx == topic_levels.size() && filter_idx == filter_levels.size());
}

bool is_valid_publish_topic(const std::string& topic) {
    if (topic.empty() || topic.length() > MQTT_MAX_TOPIC_LEN) {
        return false;
    }
    
    // Publish topics cannot contain wildcards
    return topic.find('+') == std::string::npos && topic.find('#') == std::string::npos;
}

bool is_valid_subscribe_filter(const std::string& filter) {
    if (filter.empty() || filter.length() > MQTT_MAX_TOPIC_LEN) {
        return false;
    }
    
    // Basic wildcard validation
    size_t hash_pos = filter.find('#');
    if (hash_pos != std::string::npos) {
        // # must be the last character and preceded by / or be the entire filter
        if (hash_pos != filter.length() - 1) {
            return false;
        }
        if (hash_pos > 0 && filter[hash_pos - 1] != '/') {
            return false;
        }
    }
    
    return true;
}

int min_qos(int qos1, int qos2) {
    return std::min(qos1, qos2);
}

int max_qos(int qos1, int qos2) {
    return std::max(qos1, qos2);
}

const char* qos_to_string(int qos) {
    switch (qos) {
        case 0: return "QoS 0 (At most once)";
        case 1: return "QoS 1 (At least once)";
        case 2: return "QoS 2 (Exactly once)";
        default: return "Invalid QoS";
    }
}

std::string encode_mqtt_string(const std::string& str) {
    // Simple encoding - in a full implementation this would handle
    // proper MQTT string encoding with length prefixes
    return str;
}

bool decode_mqtt_string(const void* data, size_t length, std::string& result) {
    if (!data || length == 0) {
        result.clear();
        return true;
    }
    
    result.assign(static_cast<const char*>(data), length);
    return true;
}

std::string binary_to_hex(const void* data, size_t length) {
    if (!data || length == 0) {
        return "";
    }
    
    std::ostringstream oss;
    const uint8_t* bytes = static_cast<const uint8_t*>(data);
    
    for (size_t i = 0; i < length; ++i) {
        oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(bytes[i]);
    }
    
    return oss.str();
}

bool hex_to_binary(const std::string& hex, std::vector<uint8_t>& binary) {
    if (hex.length() % 2 != 0) {
        return false;
    }
    
    binary.clear();
    binary.reserve(hex.length() / 2);
    
    for (size_t i = 0; i < hex.length(); i += 2) {
        std::string byte_str = hex.substr(i, 2);
        try {
            uint8_t byte = static_cast<uint8_t>(std::stoul(byte_str, nullptr, 16));
            binary.push_back(byte);
        } catch (...) {
            return false;
        }
    }
    
    return true;
}

std::unique_ptr<MQTTMessage> create_publish_message(
    const std::string& topic, const std::string& payload, 
    int qos, bool retain) {
    return std::make_unique<MQTTMessage>(topic, payload, qos, retain);
}

std::unique_ptr<MQTTMessage> create_will_message(
    const std::string& topic, const std::string& message,
    int qos, bool retain) {
    auto msg = std::make_unique<MQTTMessage>(topic, message, qos, retain);
    return msg;
}

svalue_t* create_message_callback_args(const MQTTMessage& message) {
    svalue_t* args = CALLOCATE(5, svalue_t, TAG_TEMPORARY, "mqtt message callback args");
    
    args[0].type = T_STRING;
    args[0].u.string = string_copy(message.get_topic().c_str(), "mqtt topic");
    
    args[1].type = T_STRING;
    args[1].u.string = string_copy(message.get_payload().c_str(), "mqtt payload");
    
    args[2].type = T_NUMBER;
    args[2].u.number = message.get_qos();
    
    args[3].type = T_NUMBER;
    args[3].u.number = message.get_retain() ? 1 : 0;
    
    args[4].type = T_NUMBER;
    args[4].u.number = message.get_packet_id();
    
    return args;
}

void free_message_callback_args(svalue_t* args, int argc) {
    if (args) {
        for (int i = 0; i < argc; ++i) {
            free_svalue(&args[i], "mqtt callback args");
        }
        FREE(args);
    }
}

} // namespace mqtt

// MQTTMessageQueue implementation

MQTTMessageQueue::MQTTMessageQueue(size_t max_size) : max_size_(max_size) {}

MQTTMessageQueue::~MQTTMessageQueue() {
    clear();
}

bool MQTTMessageQueue::enqueue(std::unique_ptr<MQTTMessage> message) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (queue_.size() >= max_size_) {
        return false;
    }
    
    queue_.push(std::move(message));
    return true;
}

std::unique_ptr<MQTTMessage> MQTTMessageQueue::dequeue() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Check priority queue first
    if (!priority_queue_.empty()) {
        auto message = std::move(priority_queue_.front());
        priority_queue_.pop();
        return message;
    }
    
    // Then regular queue
    if (!queue_.empty()) {
        auto message = std::move(queue_.front());
        queue_.pop();
        return message;
    }
    
    return nullptr;
}

size_t MQTTMessageQueue::size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return queue_.size() + priority_queue_.size();
}

bool MQTTMessageQueue::empty() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return queue_.empty() && priority_queue_.empty();
}

bool MQTTMessageQueue::full() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return (queue_.size() + priority_queue_.size()) >= max_size_;
}

void MQTTMessageQueue::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    while (!queue_.empty()) {
        queue_.pop();
    }
    
    while (!priority_queue_.empty()) {
        priority_queue_.pop();
    }
}

bool MQTTMessageQueue::enqueue_priority(std::unique_ptr<MQTTMessage> message) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if ((queue_.size() + priority_queue_.size()) >= max_size_) {
        return false;
    }
    
    priority_queue_.push(std::move(message));
    return true;
}