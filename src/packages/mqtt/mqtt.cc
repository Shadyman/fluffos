#include "mqtt.h"
#include "mqtt_client.h"
#include "mqtt_message.h"
#include "base/package_api.h"

// Global MQTT client registry
std::map<int, std::unique_ptr<MQTTClient>> g_mqtt_clients;

// Package initialization and management

namespace mqtt {

bool initialize_mqtt_package() {
    // Initialize the MQTT package
    g_mqtt_clients.clear();
    
    // Register MQTT socket modes with the socket system
    // This would integrate with the socket option manager
    
    return true;
}

void cleanup_mqtt_package() {
    // Disconnect all clients and clean up
    for (auto& pair : g_mqtt_clients) {
        if (pair.second && pair.second->is_connected()) {
            pair.second->disconnect();
        }
    }
    
    g_mqtt_clients.clear();
}

bool create_mqtt_client(int socket_fd, const mqtt_connection_config& config) {
    // Remove existing client if any
    remove_mqtt_client(socket_fd);
    
    // Create new MQTT client
    auto client = create_client(socket_fd);
    if (!client) {
        return false;
    }
    
    // Set configuration
    if (!client->set_config(config)) {
        return false;
    }
    
    // Store client in registry
    g_mqtt_clients[socket_fd] = std::move(client);
    
    return true;
}

MQTTClient* get_mqtt_client(int socket_fd) {
    auto it = g_mqtt_clients.find(socket_fd);
    return (it != g_mqtt_clients.end()) ? it->second.get() : nullptr;
}

void remove_mqtt_client(int socket_fd) {
    auto it = g_mqtt_clients.find(socket_fd);
    if (it != g_mqtt_clients.end()) {
        if (it->second && it->second->is_connected()) {
            it->second->disconnect();
        }
        g_mqtt_clients.erase(it);
    }
}

bool is_mqtt_socket_mode(int mode) {
    return (mode == MQTT_CLIENT || mode == MQTT_TLS_CLIENT);
}

bool validate_mqtt_socket_options(int socket_fd, int option, svalue_t* value) {
    if (!IS_MQTT_OPTION(option)) {
        return false;
    }
    
    // Validate MQTT-specific socket options
    switch (option) {
        case MQTT_BROKER:
            return (value && value->type == T_STRING && value->u.string && strlen(value->u.string) > 0);
        
        case MQTT_CLIENT_ID:
            return (value && value->type == T_STRING && value->u.string && 
                   strlen(value->u.string) <= MQTT_MAX_CLIENT_ID_LEN);
        
        case MQTT_USERNAME:
        case MQTT_PASSWORD:
        case MQTT_WILL_TOPIC:
        case MQTT_WILL_MESSAGE:
            return (value && value->type == T_STRING);
        
        case MQTT_KEEP_ALIVE:
            return (value && value->type == T_NUMBER && 
                   value->u.number >= 10 && value->u.number <= 65535);
        
        case MQTT_QOS:
            return (value && value->type == T_NUMBER && 
                   value->u.number >= 0 && value->u.number <= 1); // libwebsockets limitation
        
        case MQTT_RETAIN:
        case MQTT_CLEAN_SESSION:
            return (value && value->type == T_NUMBER && 
                   (value->u.number == 0 || value->u.number == 1));
        
        default:
            return false;
    }
}

int mqtt_callback_handler(struct lws *wsi, enum lws_callback_reasons reason, 
                         void *user, void *in, size_t len) {
    
    // Find the associated MQTT client
    // This would need to be integrated with the socket system
    // to get the socket_fd from the libwebsockets wsi
    
    int socket_fd = -1; // Would be determined from wsi
    
    MQTTClient* client = get_mqtt_client(socket_fd);
    if (!client) {
        return -1;
    }
    
    return client->handle_callback(reason, user, in, len);
}

} // namespace mqtt

// LPC efun implementations

void f_socket_mqtt_connect(void) {
    int socket_fd;
    const char* broker_host;
    int broker_port = 0;
    const char* client_id = nullptr;
    
    if (st_num_arg == 4) {
        client_id = sp[-0].u.string;
        pop_stack();
    }
    if (st_num_arg >= 3) {
        broker_port = sp[-0].u.number;
        pop_stack();
    }
    broker_host = sp[-0].u.string;
    socket_fd = sp[-1].u.number;
    pop_n_elems(2);
    
    // Validate socket
    if (socket_fd < 0) {
        push_number(0);
        return;
    }
    
    // Get or create MQTT client
    MQTTClient* client = mqtt::get_mqtt_client(socket_fd);
    if (!client) {
        // Create new client with default config
        mqtt_connection_config config;
        if (!mqtt::create_mqtt_client(socket_fd, config)) {
            push_number(0);
            return;
        }
        client = mqtt::get_mqtt_client(socket_fd);
    }
    
    if (!client) {
        push_number(0);
        return;
    }
    
    // Set default port if not specified
    if (broker_port == 0) {
        broker_port = MQTT_DEFAULT_PORT;
    }
    
    // Connect to broker
    std::string client_id_str = client_id ? client_id : "";
    bool success = client->connect(broker_host, broker_port, client_id_str);
    
    push_number(success ? 1 : 0);
}

void f_socket_mqtt_disconnect(void) {
    int socket_fd = sp->u.number;
    pop_stack();
    
    MQTTClient* client = mqtt::get_mqtt_client(socket_fd);
    if (!client) {
        push_number(0);
        return;
    }
    
    bool success = client->disconnect();
    push_number(success ? 1 : 0);
}

void f_socket_mqtt_publish(void) {
    int socket_fd;
    const char* topic;
    const char* payload;
    int qos = 0;
    int retain = 0;
    
    if (st_num_arg == 5) {
        retain = sp[-0].u.number;
        pop_stack();
    }
    if (st_num_arg >= 4) {
        qos = sp[-0].u.number;
        pop_stack();
    }
    payload = sp[-0].u.string;
    topic = sp[-1].u.string;
    socket_fd = sp[-2].u.number;
    pop_n_elems(3);
    
    MQTTClient* client = mqtt::get_mqtt_client(socket_fd);
    if (!client) {
        push_number(0);
        return;
    }
    
    bool success = client->publish(topic, payload, qos, retain != 0);
    push_number(success ? 1 : 0);
}

void f_socket_mqtt_subscribe(void) {
    int socket_fd;
    svalue_t* topic_val;
    svalue_t* qos_val = nullptr;
    
    if (st_num_arg == 3) {
        qos_val = sp;
        pop_stack();
    }
    topic_val = sp--;
    socket_fd = sp->u.number;
    pop_stack();
    
    MQTTClient* client = mqtt::get_mqtt_client(socket_fd);
    if (!client) {
        free_svalue(topic_val, "mqtt subscribe");
        if (qos_val) free_svalue(qos_val, "mqtt subscribe qos");
        push_number(0);
        return;
    }
    
    bool success = false;
    
    if (topic_val->type == T_STRING) {
        // Single topic subscription
        int qos = 0;
        if (qos_val && qos_val->type == T_NUMBER) {
            qos = static_cast<int>(qos_val->u.number);
        }
        success = client->subscribe(topic_val->u.string, qos);
        
    } else if (topic_val->type == T_ARRAY) {
        // Multiple topic subscription
        array_t* topics = topic_val->u.arr;
        array_t* qos_levels = (qos_val && qos_val->type == T_ARRAY) ? qos_val->u.arr : nullptr;
        
        std::vector<std::string> topic_list;
        std::vector<int> qos_list;
        
        for (int i = 0; i < topics->size; ++i) {
            if (topics->item[i].type == T_STRING) {
                topic_list.push_back(topics->item[i].u.string);
                
                int qos = 0;
                if (qos_levels && i < qos_levels->size && qos_levels->item[i].type == T_NUMBER) {
                    qos = static_cast<int>(qos_levels->item[i].u.number);
                }
                qos_list.push_back(qos);
            }
        }
        
        if (!topic_list.empty()) {
            success = client->subscribe(topic_list, qos_list);
        }
    }
    
    free_svalue(topic_val, "mqtt subscribe");
    if (qos_val) free_svalue(qos_val, "mqtt subscribe qos");
    push_number(success ? 1 : 0);
}

void f_socket_mqtt_unsubscribe(void) {
    int socket_fd;
    svalue_t* topic_val;
    
    topic_val = sp--;
    socket_fd = sp->u.number;
    pop_stack();
    
    MQTTClient* client = mqtt::get_mqtt_client(socket_fd);
    if (!client) {
        free_svalue(topic_val, "mqtt unsubscribe");
        push_number(0);
        return;
    }
    
    bool success = false;
    
    if (topic_val->type == T_STRING) {
        // Single topic unsubscription
        success = client->unsubscribe(topic_val->u.string);
        
    } else if (topic_val->type == T_ARRAY) {
        // Multiple topic unsubscription
        array_t* topics = topic_val->u.arr;
        std::vector<std::string> topic_list;
        
        for (int i = 0; i < topics->size; ++i) {
            if (topics->item[i].type == T_STRING) {
                topic_list.push_back(topics->item[i].u.string);
            }
        }
        
        if (!topic_list.empty()) {
            success = client->unsubscribe(topic_list);
        }
    }
    
    free_svalue(topic_val, "mqtt unsubscribe");
    push_number(success ? 1 : 0);
}

void f_socket_mqtt_status(void) {
    int socket_fd = sp->u.number;
    pop_stack();
    
    MQTTClient* client = mqtt::get_mqtt_client(socket_fd);
    if (!client) {
        push_number(0);
        return;
    }
    
    mapping_t* status = client->get_status_mapping();
    push_refed_mapping(status);
}

void f_socket_mqtt_set_config(void) {
    int socket_fd;
    mapping_t* config_map;
    
    config_map = sp[-0].u.map;
    socket_fd = sp[-1].u.number;
    pop_n_elems(2);
    
    // Get or create MQTT client
    MQTTClient* client = mqtt::get_mqtt_client(socket_fd);
    if (!client) {
        mqtt_connection_config config;
        if (!mqtt::create_mqtt_client(socket_fd, config)) {
            push_number(0);
            return;
        }
        client = mqtt::get_mqtt_client(socket_fd);
    }
    
    if (!client) {
        push_number(0);
        return;
    }
    
    // Convert mapping to config
    mqtt_connection_config config;
    if (!mqtt::mapping_to_config(config_map, config)) {
        push_number(0);
        return;
    }
    
    bool success = client->set_config(config);
    push_number(success ? 1 : 0);
}

void f_socket_mqtt_get_config(void) {
    int socket_fd = sp->u.number;
    pop_stack();
    
    MQTTClient* client = mqtt::get_mqtt_client(socket_fd);
    if (!client) {
        push_number(0);
        return;
    }
    
    mapping_t* config_map = mqtt::config_to_mapping(client->get_config());
    push_refed_mapping(config_map);
}

void f_socket_mqtt_set_will(void) {
    int socket_fd;
    const char* topic;
    const char* message;
    int qos = 0;
    int retain = 0;
    
    if (st_num_arg == 5) {
        retain = sp[-0].u.number;
        pop_stack();
    }
    if (st_num_arg >= 4) {
        qos = sp[-0].u.number;
        pop_stack();
    }
    message = sp[-0].u.string;
    topic = sp[-1].u.string;
    socket_fd = sp[-2].u.number;
    pop_n_elems(3);
    
    MQTTClient* client = mqtt::get_mqtt_client(socket_fd);
    if (!client) {
        push_number(0);
        return;
    }
    
    bool success = client->set_will(topic, message, qos, retain != 0);
    push_number(success ? 1 : 0);
}

void f_socket_mqtt_clear_will(void) {
    int socket_fd = sp->u.number;
    pop_stack();
    
    MQTTClient* client = mqtt::get_mqtt_client(socket_fd);
    if (!client) {
        push_number(0);
        return;
    }
    
    bool success = client->clear_will();
    push_number(success ? 1 : 0);
}