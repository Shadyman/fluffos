#ifndef MQTT_H_
#define MQTT_H_

/*
 * MQTT Client Package for FluffOS Unified Socket Architecture
 * 
 * This package provides MQTT client functionality using libwebsockets.
 * IMPORTANT: libwebsockets only supports MQTT CLIENT mode, not server.
 * 
 * Supported Features:
 * - MQTT 3.1.1 and 5.0 protocol support
 * - Secure MQTT (MQTTS) over TLS
 * - Topic subscription and unsubscription
 * - Message publishing with QoS levels 0, 1
 * - Last Will and Testament (LWT)
 * - Keep-alive and reconnection logic
 * - Authentication with username/password
 * - Clean session and persistent session support
 */

#include "base/package_api.h"
#include "socket_option_manager.h"
#include "socket_options.h"
#include "libwebsockets.h"
#include "libwebsockets/lws-mqtt.h"

#include <string>
#include <map>
#include <memory>

// MQTT protocol constants
#define MQTT_VERSION_3_1_1      4
#define MQTT_VERSION_5_0        5
#define MQTT_MAX_CLIENT_ID_LEN  23
#define MQTT_MAX_TOPIC_LEN      65535
#define MQTT_MAX_PAYLOAD_LEN    268435455  // 256MB - 1
#define MQTT_DEFAULT_KEEP_ALIVE 60
#define MQTT_DEFAULT_PORT       1883
#define MQTT_DEFAULT_TLS_PORT   8883

// MQTT Quality of Service levels
enum mqtt_qos_level {
    MQTT_QOS_0 = 0,  // At most once
    MQTT_QOS_1 = 1,  // At least once  
    MQTT_QOS_2 = 2   // Exactly once (not supported by libwebsockets)
};

// MQTT client states
enum mqtt_client_state {
    MQTT_STATE_DISCONNECTED = 0,
    MQTT_STATE_CONNECTING,
    MQTT_STATE_CONNECTED,
    MQTT_STATE_SUBSCRIBING,
    MQTT_STATE_PUBLISHING,
    MQTT_STATE_DISCONNECTING,
    MQTT_STATE_ERROR
};

// MQTT connection flags
enum mqtt_connect_flags {
    MQTT_FLAG_CLEAN_SESSION = 0x02,
    MQTT_FLAG_WILL_FLAG     = 0x04,
    MQTT_FLAG_WILL_QOS_1    = 0x08,
    MQTT_FLAG_WILL_QOS_2    = 0x10,
    MQTT_FLAG_WILL_RETAIN   = 0x20,
    MQTT_FLAG_PASSWORD      = 0x40,
    MQTT_FLAG_USERNAME      = 0x80
};

// Forward declarations
struct mqtt_connection_info;
class MQTTClient;
class MQTTMessage;

// MQTT connection configuration structure
struct mqtt_connection_config {
    std::string broker_host;
    int broker_port;
    std::string client_id;
    std::string username;
    std::string password;
    int keep_alive;
    bool clean_session;
    bool use_tls;
    
    // Last Will and Testament
    struct {
        std::string topic;
        std::string message;
        int qos;
        bool retain;
    } will;
    
    mqtt_connection_config() : 
        broker_port(MQTT_DEFAULT_PORT),
        keep_alive(MQTT_DEFAULT_KEEP_ALIVE),
        clean_session(true),
        use_tls(false) {
        will.qos = 0;
        will.retain = false;
    }
};

// MQTT publish message structure
struct mqtt_publish_message {
    std::string topic;
    std::string payload;
    int qos;
    bool retain;
    bool dup;
    uint16_t packet_id;
    
    mqtt_publish_message() : qos(0), retain(false), dup(false), packet_id(0) {}
};

// MQTT subscription structure
struct mqtt_subscription {
    std::string topic;
    int qos;
    bool subscribed;
    
    mqtt_subscription() : qos(0), subscribed(false) {}
    mqtt_subscription(const std::string& t, int q) : topic(t), qos(q), subscribed(false) {}
};

// MQTT package functions - these will be exposed as efuns
extern "C" {
    // Core MQTT functions
    void f_socket_mqtt_connect(void);
    void f_socket_mqtt_disconnect(void);
    void f_socket_mqtt_publish(void);
    void f_socket_mqtt_subscribe(void);
    void f_socket_mqtt_unsubscribe(void);
    void f_socket_mqtt_status(void);
    
    // MQTT configuration functions
    void f_socket_mqtt_set_config(void);
    void f_socket_mqtt_get_config(void);
    void f_socket_mqtt_set_will(void);
    void f_socket_mqtt_clear_will(void);
}

// MQTT package initialization and management
namespace mqtt {
    // Initialize MQTT package
    bool initialize_mqtt_package();
    
    // Cleanup MQTT package
    void cleanup_mqtt_package();
    
    // Create MQTT client for socket
    bool create_mqtt_client(int socket_fd, const mqtt_connection_config& config);
    
    // Get MQTT client for socket
    MQTTClient* get_mqtt_client(int socket_fd);
    
    // Remove MQTT client for socket
    void remove_mqtt_client(int socket_fd);
    
    // MQTT socket mode validation
    bool is_mqtt_socket_mode(int mode);
    bool validate_mqtt_socket_options(int socket_fd, int option, svalue_t* value);
    
    // MQTT callback handler from libwebsockets
    int mqtt_callback_handler(struct lws *wsi, enum lws_callback_reasons reason, 
                             void *user, void *in, size_t len);
}

// Global MQTT client registry
extern std::map<int, std::unique_ptr<MQTTClient>> g_mqtt_clients;

#endif  // MQTT_H_