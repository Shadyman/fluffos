/*
 * LPC Interface for gRPC Package
 * 
 * Defines the LPC function prototypes and macros for interacting with
 * the gRPC package from LPC code in the FluffOS unified socket system.
 */

#ifndef LPC_INTERFACE_H_
#define LPC_INTERFACE_H_

/* 
 * gRPC LPC Function Prototypes
 * These functions are exposed to the LPC mudlib and can be called from LPC code.
 */

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Socket creation and management functions
 */
int f_grpc_create_socket(int mode);
int f_grpc_configure_socket(int socket_fd, mapping_t* options);
int f_grpc_close_socket(int socket_fd);
int f_grpc_socket_status(int socket_fd);

/*
 * Service registration functions (for gRPC servers)
 */
int f_grpc_register_service(int socket_fd, const char* service_name, const char* proto_definition);
int f_grpc_register_method(int socket_fd, const char* service_name, const char* method_name, 
                          const char* callback_function);
int f_grpc_unregister_service(int socket_fd, const char* service_name);

/*
 * Client method invocation functions
 */
mapping_t* f_grpc_call_method(int socket_fd, const char* service_name, const char* method_name,
                              const char* request_data, mapping_t* metadata);
int f_grpc_call_method_async(int socket_fd, const char* service_name, const char* method_name,
                            const char* request_data, const char* callback_function);

/*
 * Streaming functions
 */
int f_grpc_start_client_stream(int socket_fd, const char* service_name, const char* method_name);
int f_grpc_start_server_stream(int socket_fd, const char* service_name, const char* method_name,
                              const char* request_data);
int f_grpc_start_bidirectional_stream(int socket_fd, const char* service_name, 
                                     const char* method_name);
int f_grpc_send_stream_message(int socket_fd, const char* message_data);
mapping_t* f_grpc_read_stream_message(int socket_fd);
mapping_t* f_grpc_finish_stream(int socket_fd);

/*
 * Protocol Buffers functions
 */
int f_grpc_load_proto_file(const char* file_path);
int f_grpc_load_proto_string(const char* proto_content);
array_t* f_grpc_get_service_names(void);
array_t* f_grpc_get_method_names(const char* service_name);
mapping_t* f_grpc_get_method_details(const char* service_name, const char* method_name);
char* f_grpc_serialize_message(const char* type_name, mapping_t* data);
mapping_t* f_grpc_deserialize_message(const char* type_name, const char* data);
int f_grpc_validate_message(const char* type_name, mapping_t* data);

/*
 * Channel management functions
 */
char* f_grpc_create_channel(const char* target_address, mapping_t* options);
int f_grpc_close_channel(const char* channel_id);
int f_grpc_channel_ready(const char* channel_id);
mapping_t* f_grpc_channel_stats(const char* channel_id);
array_t* f_grpc_active_channels(void);

/*
 * Configuration and utility functions
 */
mapping_t* f_grpc_get_config(void);
int f_grpc_set_debug_mode(int enabled);
mapping_t* f_grpc_get_package_info(void);
char* f_grpc_version(void);

#ifdef __cplusplus
}
#endif

/*
 * LPC Macro Definitions
 * These provide convenient constants for LPC code.
 */

/* gRPC Call Types */
#define GRPC_UNARY                     0
#define GRPC_SERVER_STREAMING          1
#define GRPC_CLIENT_STREAMING          2
#define GRPC_BIDIRECTIONAL_STREAMING   3

/* gRPC Status Codes */
#define GRPC_OK                        0
#define GRPC_CANCELLED                 1
#define GRPC_UNKNOWN                   2
#define GRPC_INVALID_ARGUMENT          3
#define GRPC_DEADLINE_EXCEEDED         4
#define GRPC_NOT_FOUND                 5
#define GRPC_ALREADY_EXISTS            6
#define GRPC_PERMISSION_DENIED         7
#define GRPC_RESOURCE_EXHAUSTED        8
#define GRPC_FAILED_PRECONDITION       9
#define GRPC_ABORTED                   10
#define GRPC_OUT_OF_RANGE              11
#define GRPC_UNIMPLEMENTED             12
#define GRPC_INTERNAL                  13
#define GRPC_UNAVAILABLE               14
#define GRPC_DATA_LOSS                 15
#define GRPC_UNAUTHENTICATED           16

/* gRPC Socket Modes (from socket_modes.h) */
#define GRPC_CLIENT_MODE               1
#define GRPC_SERVER_MODE               2

/* gRPC Options (from socket_options.h) */
#define GRPC_SERVICE_CONFIG            420
#define GRPC_MAX_MESSAGE_SIZE          421
#define GRPC_KEEPALIVE_TIME            422
#define GRPC_KEEPALIVE_TIMEOUT         423
#define GRPC_COMPRESSION               424
#define GRPC_RETRY_POLICY              425
#define GRPC_LOAD_BALANCING            426
#define GRPC_CHANNEL_ARGS              427
#define GRPC_TLS_ENABLED               428
#define GRPC_TLS_CONFIG                429
#define GRPC_TARGET_ADDRESS            430
#define GRPC_PROTO_FILE                431
#define GRPC_TLS_CERT_FILE             432
#define GRPC_TLS_KEY_FILE              433
#define GRPC_TLS_CA_FILE               434
#define GRPC_AUTHENTICATION            435
#define GRPC_DEADLINE                  436
#define GRPC_REFLECTION_ENABLE         437
#define GRPC_HEALTH_CHECK              438
#define GRPC_DEBUG_MODE                439

/* Helper Macros for LPC Code */
#define GRPC_SUCCESS(status)           ((status) == GRPC_OK)
#define GRPC_FAILED(status)            ((status) != GRPC_OK)
#define IS_GRPC_SOCKET(fd)             (socket_get_mode(fd) >= SOCKET_GRPC_CLIENT && \
                                        socket_get_mode(fd) <= SOCKET_GRPC_SERVER)

#endif /* LPC_INTERFACE_H_ */