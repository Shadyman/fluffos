/*
 * gRPC Package Header
 * 
 * FluffOS gRPC package for unified socket architecture.
 * Provides gRPC server and client functionality with Protocol Buffers integration.
 */

#ifndef PACKAGES_GRPC_H_
#define PACKAGES_GRPC_H_

#include "base/package_api.h"
#include "packages/sockets/socket_options.h"
#include "packages/sockets/socket_option_manager.h"

#include <memory>
#include <string>
#include <vector>
#include <map>
#include <functional>
#include <thread>
#include <mutex>

// Forward declarations
class GrpcServer;
class GrpcClient;
class GrpcService;
class GrpcProtobufManager;
class GrpcChannelManager;

/*
 * gRPC Call Types
 */
enum GrpcCallType {
    GRPC_UNARY = 0,        // Single request, single response
    GRPC_SERVER_STREAMING = 1,  // Single request, streaming response
    GRPC_CLIENT_STREAMING = 2,  // Streaming request, single response
    GRPC_BIDIRECTIONAL_STREAMING = 3  // Streaming request and response
};

/*
 * gRPC Status Codes (aligned with gRPC standard)
 */
enum GrpcStatus {
    GRPC_OK = 0,
    GRPC_CANCELLED = 1,
    GRPC_UNKNOWN = 2,
    GRPC_INVALID_ARGUMENT = 3,
    GRPC_DEADLINE_EXCEEDED = 4,
    GRPC_NOT_FOUND = 5,
    GRPC_ALREADY_EXISTS = 6,
    GRPC_PERMISSION_DENIED = 7,
    GRPC_RESOURCE_EXHAUSTED = 8,
    GRPC_FAILED_PRECONDITION = 9,
    GRPC_ABORTED = 10,
    GRPC_OUT_OF_RANGE = 11,
    GRPC_UNIMPLEMENTED = 12,
    GRPC_INTERNAL = 13,
    GRPC_UNAVAILABLE = 14,
    GRPC_DATA_LOSS = 15,
    GRPC_UNAUTHENTICATED = 16
};

/*
 * gRPC Method Information
 */
struct GrpcMethodInfo {
    std::string service_name;
    std::string method_name;
    std::string full_method;  // /service.name/method.name
    GrpcCallType call_type;
    std::string request_type;
    std::string response_type;
    bool requires_auth;
};

/*
 * gRPC Request Structure
 */
struct GrpcRequest {
    std::string service_name;
    std::string method_name;
    std::string request_data;  // Serialized protobuf data
    std::map<std::string, std::string> metadata;
    int socket_fd;
    object_t* requester;
    uint32_t deadline_ms;
    std::string compression_algorithm;
};

/*
 * gRPC Response Structure
 */
struct GrpcResponse {
    GrpcStatus status;
    std::string response_data;  // Serialized protobuf data
    std::string error_message;
    std::string error_details;
    std::map<std::string, std::string> metadata;
    std::map<std::string, std::string> trailing_metadata;
};

/*
 * gRPC Service Handler Function Type
 */
typedef std::function<GrpcResponse(const GrpcRequest&)> GrpcServiceHandler;

/*
 * gRPC Streaming Handler Function Type
 */
typedef std::function<void(const GrpcRequest&, std::function<void(const GrpcResponse&)>)> GrpcStreamingHandler;

/*
 * gRPC Manager (Singleton)
 * Manages gRPC servers and clients for unified socket architecture
 */
class GrpcManager {
public:
    static GrpcManager* getInstance();
    
    // Initialization and cleanup
    bool initialize();
    void shutdown();
    
    // Socket management
    int create_grpc_socket(int socket_fd, const std::string& mode);
    bool handle_grpc_request(int socket_fd, const std::string& data);
    void close_grpc_socket(int socket_fd);
    
    // Service management
    bool register_service(int socket_fd, const std::string& service_definition);
    bool register_method_handler(const std::string& service_name, const std::string& method_name,
                                GrpcServiceHandler handler);
    bool register_streaming_handler(const std::string& service_name, const std::string& method_name,
                                   GrpcStreamingHandler handler);
    
    // Client operations
    bool connect_to_service(int socket_fd, const std::string& target);
    GrpcResponse call_method(int socket_fd, const GrpcRequest& request);
    bool start_stream(int socket_fd, const std::string& service_name, const std::string& method_name);
    bool send_stream_message(int socket_fd, const std::string& message);
    void close_stream(int socket_fd);
    
    // Protocol buffer management
    bool load_protobuf_schema(const std::string& proto_file);
    bool validate_message(const std::string& type_name, const std::string& data);
    std::string serialize_message(const std::string& type_name, const mapping_t& data);
    mapping_t deserialize_message(const std::string& type_name, const std::string& data);
    
    // Server reflection
    bool enable_reflection(int socket_fd, bool enabled);
    std::vector<std::string> list_services(int socket_fd);
    GrpcMethodInfo get_method_info(const std::string& service_name, const std::string& method_name);
    
    // Health check service
    bool enable_health_check(int socket_fd, bool enabled);
    void set_service_health(const std::string& service_name, bool healthy);
    
    // Statistics and monitoring
    std::map<std::string, int> get_call_statistics();
    std::map<std::string, double> get_latency_metrics();
    int get_active_streams_count();
    
    // Error handling and logging
    void log_error(int socket_fd, const std::string& error, const std::string& context);
    
private:
    GrpcManager();
    ~GrpcManager();
    
    static GrpcManager* instance_;
    std::mutex instance_mutex_;
    
    std::map<int, std::unique_ptr<GrpcServer>> servers_;
    std::map<int, std::unique_ptr<GrpcClient>> clients_;
    std::map<std::string, GrpcServiceHandler> service_handlers_;
    std::map<std::string, GrpcStreamingHandler> streaming_handlers_;
    std::unique_ptr<GrpcProtobufManager> protobuf_manager_;
    std::unique_ptr<GrpcChannelManager> channel_manager_;
    
    bool initialized_;
    std::mutex manager_mutex_;
    
    // Internal methods
    bool parse_grpc_request(const std::string& data, GrpcRequest& request);
    std::string format_grpc_response(const GrpcResponse& response);
    std::string generate_method_key(const std::string& service_name, const std::string& method_name);
    void setup_default_services();
    void register_mud_services();
};

/*
 * gRPC Server Implementation
 * Handles individual gRPC server instances per socket
 */
class GrpcServer {
public:
    GrpcServer(int socket_fd);
    ~GrpcServer();
    
    // Configuration
    bool configure(std::unique_ptr<SocketOptionManager> option_manager);
    void set_service_config(const std::string& service_definition);
    void set_max_message_size(size_t max_size);
    void set_compression_algorithm(const std::string& algorithm);
    void enable_keepalive(bool enabled, uint32_t time_ms, uint32_t timeout_ms);
    
    // Service registration
    bool register_service(const std::string& service_name, const std::string& proto_definition);
    bool register_method(const std::string& service_name, const std::string& method_name,
                        GrpcServiceHandler handler);
    
    // Request handling
    GrpcResponse handle_request(const GrpcRequest& request);
    bool handle_streaming_request(const GrpcRequest& request);
    
    // Server reflection support
    bool handle_reflection_request(const std::string& method, const std::string& request_data,
                                  std::string& response_data);
    
    // Health check service
    bool handle_health_check(const std::string& service_name, std::string& response);
    
    // Server state management
    bool start_server();
    void stop_server();
    bool is_running() const;
    std::string get_server_status() const;
    
    // Statistics
    void record_call(const std::string& method_name, double latency_ms);
    std::map<std::string, int> get_call_counts() const;
    std::map<std::string, double> get_average_latencies() const;
    
private:
    int socket_fd_;
    std::unique_ptr<SocketOptionManager> option_manager_;
    
    // Configuration
    std::string service_config_;
    size_t max_message_size_;
    std::string compression_algorithm_;
    bool keepalive_enabled_;
    uint32_t keepalive_time_ms_;
    uint32_t keepalive_timeout_ms_;
    bool reflection_enabled_;
    bool health_check_enabled_;
    
    // Services and methods
    std::map<std::string, std::string> registered_services_;  // service_name -> proto_definition
    std::map<std::string, GrpcServiceHandler> method_handlers_;
    std::map<std::string, GrpcMethodInfo> method_info_;
    
    // Server state
    bool running_;
    bool configured_;
    std::mutex server_mutex_;
    
    // Statistics
    std::map<std::string, int> call_counts_;
    std::map<std::string, double> total_latencies_;
    std::mutex stats_mutex_;
    
    // Internal methods
    bool validate_request(const GrpcRequest& request);
    GrpcResponse create_error_response(GrpcStatus status, const std::string& message);
    void apply_compression(std::string& data, const std::string& algorithm);
    bool decompress_data(std::string& data, const std::string& algorithm);
};

/*
 * gRPC Client Implementation
 * Handles gRPC client connections and method calls
 */
class GrpcClient {
public:
    GrpcClient(int socket_fd);
    ~GrpcClient();
    
    // Configuration
    bool configure(std::unique_ptr<SocketOptionManager> option_manager);
    void set_target(const std::string& target);
    void set_deadline(uint32_t deadline_ms);
    void set_retry_policy(const std::string& policy);
    void set_compression(const std::string& algorithm);
    
    // Connection management
    bool connect();
    void disconnect();
    bool is_connected() const;
    
    // Method calls
    GrpcResponse call_unary_method(const std::string& service_name, const std::string& method_name,
                                  const std::string& request_data,
                                  const std::map<std::string, std::string>& metadata = {});
    
    // Streaming support
    bool start_client_stream(const std::string& service_name, const std::string& method_name);
    bool send_stream_message(const std::string& message_data);
    GrpcResponse finish_client_stream();
    
    bool start_server_stream(const std::string& service_name, const std::string& method_name,
                            const std::string& request_data);
    bool read_stream_message(std::string& response_data);
    
    bool start_bidirectional_stream(const std::string& service_name, const std::string& method_name);
    
    // Client state
    std::string get_connection_status() const;
    std::map<std::string, std::string> get_channel_info() const;
    
private:
    int socket_fd_;
    std::unique_ptr<SocketOptionManager> option_manager_;
    
    // Configuration
    std::string target_;
    uint32_t deadline_ms_;
    std::string retry_policy_;
    std::string compression_algorithm_;
    
    // Connection state
    bool connected_;
    bool configured_;
    std::mutex client_mutex_;
    
    // Streaming state
    bool streaming_active_;
    std::string active_stream_method_;
    GrpcCallType active_stream_type_;
    
    // Internal methods
    std::string format_method_path(const std::string& service_name, const std::string& method_name);
    bool send_request(const std::string& method_path, const std::string& request_data,
                     const std::map<std::string, std::string>& metadata);
    bool receive_response(std::string& response_data, std::map<std::string, std::string>& metadata);
    void handle_connection_error(const std::string& error);
};

/*
 * Protocol Buffers Manager
 * Handles .proto file parsing and message serialization/deserialization
 */
class GrpcProtobufManager {
public:
    GrpcProtobufManager();
    ~GrpcProtobufManager();
    
    // Schema management
    bool load_proto_file(const std::string& file_path);
    bool load_proto_string(const std::string& proto_content);
    bool validate_schema();
    
    // Message operations
    bool create_message_type(const std::string& type_name);
    std::string serialize_from_mapping(const std::string& type_name, const mapping_t& data);
    mapping_t deserialize_to_mapping(const std::string& type_name, const std::string& data);
    
    // Service introspection
    std::vector<std::string> get_service_names() const;
    std::vector<std::string> get_method_names(const std::string& service_name) const;
    GrpcMethodInfo get_method_details(const std::string& service_name, 
                                     const std::string& method_name) const;
    
    // Type validation
    bool validate_message_data(const std::string& type_name, const mapping_t& data);
    std::vector<std::string> get_message_field_names(const std::string& type_name) const;
    std::string get_field_type(const std::string& type_name, const std::string& field_name) const;
    
private:
    // Protocol buffer schema storage
    std::map<std::string, std::string> loaded_schemas_;
    std::map<std::string, std::vector<GrpcMethodInfo>> service_methods_;
    std::map<std::string, std::vector<std::string>> message_fields_;
    
    bool schema_loaded_;
    std::mutex protobuf_mutex_;
    
    // Internal parsing methods
    bool parse_proto_content(const std::string& content);
    void extract_services_and_methods(const std::string& content);
    void extract_message_types(const std::string& content);
    GrpcCallType determine_call_type(const std::string& method_signature);
};

/*
 * Channel Manager
 * Manages gRPC channels and connection pools
 */
class GrpcChannelManager {
public:
    GrpcChannelManager();
    ~GrpcChannelManager();
    
    // Channel management
    std::string create_channel(const std::string& target, 
                              const std::map<std::string, std::string>& options);
    bool close_channel(const std::string& channel_id);
    bool is_channel_ready(const std::string& channel_id);
    
    // Connection pooling
    void set_max_connections_per_target(int max_connections);
    void set_connection_timeout(uint32_t timeout_ms);
    void set_keepalive_settings(uint32_t time_ms, uint32_t timeout_ms);
    
    // Load balancing
    void set_load_balancing_policy(const std::string& policy);
    void add_backend_address(const std::string& target, const std::string& address);
    void remove_backend_address(const std::string& target, const std::string& address);
    
    // Channel statistics
    std::map<std::string, std::string> get_channel_stats(const std::string& channel_id);
    std::vector<std::string> get_active_channels() const;
    
private:
    std::map<std::string, std::string> active_channels_;  // channel_id -> target
    std::map<std::string, std::vector<std::string>> target_backends_;  // target -> addresses
    
    // Configuration
    int max_connections_per_target_;
    uint32_t connection_timeout_ms_;
    uint32_t keepalive_time_ms_;
    uint32_t keepalive_timeout_ms_;
    std::string load_balancing_policy_;
    
    std::mutex channel_mutex_;
    
    // Internal methods
    std::string generate_channel_id();
    void cleanup_inactive_channels();
    bool validate_target_address(const std::string& target);
};

// C interface for LPC integration
extern "C" {
    void init_grpc_package();
    void clean_grpc_package();
    
    // Socket integration functions
    void grpc_socket_close(int fd);
    int grpc_socket_read(int fd, char* buf, int len);
    int grpc_socket_write(int fd, const char* buf, int len);
    
    // gRPC server operations
    int grpc_register_service(int fd, const char* service_name, const char* proto_definition);
    int grpc_start_server(int fd);
    void grpc_stop_server(int fd);
    
    // gRPC client operations
    int grpc_connect_to_service(int fd, const char* target);
    int grpc_call_method(int fd, const char* service_name, const char* method_name,
                        const char* request_data, char* response_buffer, int buffer_size);
    
    // Streaming operations
    int grpc_start_stream(int fd, const char* service_name, const char* method_name, int stream_type);
    int grpc_send_stream_message(int fd, const char* message_data);
    int grpc_receive_stream_message(int fd, char* message_buffer, int buffer_size);
    void grpc_close_stream(int fd);
    
    // Protocol buffer operations
    int grpc_load_proto_file(const char* file_path);
    int grpc_serialize_message(const char* type_name, const char* json_data, 
                              char* protobuf_buffer, int buffer_size);
    int grpc_deserialize_message(const char* type_name, const char* protobuf_data,
                                char* json_buffer, int buffer_size);
    
    // Reflection and introspection
    int grpc_list_services(int fd, char* services_buffer, int buffer_size);
    int grpc_get_method_info(const char* service_name, const char* method_name,
                            char* info_buffer, int buffer_size);
    
    // Health check and monitoring
    int grpc_health_check(int fd, const char* service_name);
    int grpc_get_server_stats(int fd, char* stats_buffer, int buffer_size);
}

// Debug logging for gRPC operations
#ifdef DEBUG
#define GRPC_DEBUG(x) debug_message("[GRPC] " x)
#define GRPC_DEBUG_F(fmt, ...) debug_message("[GRPC] " fmt, __VA_ARGS__)
#else
#define GRPC_DEBUG(x)
#define GRPC_DEBUG_F(fmt, ...)
#endif

#endif /* PACKAGES_GRPC_H_ */