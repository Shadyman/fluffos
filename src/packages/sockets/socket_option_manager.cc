#include "socket_option_manager.h"
#include "socket_option_validator.h"
#include "socket_error_handler.h"
#include "socket_efuns.h"
#include <algorithm>
#include <sstream>
#include <cstring>
#include <regex>
#include <set>

/*
 * Static member definitions
 */
std::unordered_map<int, SocketOptionDescriptor> SocketOptionManager::option_descriptors_;
bool SocketOptionManager::descriptors_initialized_ = false;

/*
 * SocketOptionManager Implementation
 */

SocketOptionManager::SocketOptionManager(int socket_id) 
    : socket_id_(socket_id) {
    if (!descriptors_initialized_) {
        initialize_descriptors();
    }
}

SocketOptionManager::~SocketOptionManager() {
    clear_all_options();
}

void SocketOptionManager::initialize_descriptors() {
    if (descriptors_initialized_) {
        return;
    }
    
    // Core socket options (0-99)
    register_core_options();
    
    // HTTP/HTTPS options (100-119) 
    register_http_options();
    
    // REST options (110-119)
    register_rest_options();
    
    // WebSocket options (120-129)
    register_websocket_options();
    
    // MQTT options (130-139)
    register_mqtt_options();
    
    // External options (140-159)
    register_external_options();
    
    // Database options (160-179)
    register_database_options();
    
    // Cache options (200-219)
    register_cache_options();
    
    // TLS options (320-339)
    register_tls_options();
    
    // GraphQL options (400-419)
    register_graphql_options();
    
    // gRPC options (420-439)
    register_grpc_options();
    
    // Internal options (1000+)
    register_internal_options();
    
    descriptors_initialized_ = true;
}

void SocketOptionManager::register_core_options() {
    // Legacy TLS options (original values for backwards compatibility)
    register_option(SOCKET_OPT_TLS_VERIFY_PEER, OPTION_TYPE_BOOLEAN, OPTION_CATEGORY_TLS, 
                   OPTION_ACCESS_PUBLIC, false, "Enable TLS peer certificate verification");
                   
    register_option(SOCKET_OPT_TLS_SNI_HOSTNAME, OPTION_TYPE_STRING, OPTION_CATEGORY_TLS,
                   OPTION_ACCESS_PUBLIC, "", "TLS SNI hostname for certificate validation");
    
    // Basic socket configuration
    register_option(SOCKET_OPT_KEEPALIVE, OPTION_TYPE_BOOLEAN, OPTION_CATEGORY_CORE,
                   OPTION_ACCESS_PUBLIC, false, "Enable TCP keepalive");
                   
    register_option(SOCKET_OPT_NODELAY, OPTION_TYPE_BOOLEAN, OPTION_CATEGORY_CORE,
                   OPTION_ACCESS_PUBLIC, false, "Disable Nagle algorithm");
                   
    register_option(SOCKET_OPT_TIMEOUT, OPTION_TYPE_INTEGER, OPTION_CATEGORY_CORE,
                   OPTION_ACCESS_PUBLIC, 30000, "Socket timeout in milliseconds", 1000, 300000);
                   
    register_option(SOCKET_OPT_RCVBUF, OPTION_TYPE_INTEGER, OPTION_CATEGORY_CORE,
                   OPTION_ACCESS_PUBLIC, 8192, "Receive buffer size", 1024, 1048576);
                   
    register_option(SOCKET_OPT_SNDBUF, OPTION_TYPE_INTEGER, OPTION_CATEGORY_CORE,
                   OPTION_ACCESS_PUBLIC, 8192, "Send buffer size", 1024, 1048576);
    
    // Authentication options
    register_option(SOCKET_OPT_AUTH_TOKEN, OPTION_TYPE_STRING, OPTION_CATEGORY_AUTH,
                   OPTION_ACCESS_OWNER, "", "Authentication token");
                   
    register_option(SOCKET_OPT_AUTH_USERNAME, OPTION_TYPE_STRING, OPTION_CATEGORY_AUTH,
                   OPTION_ACCESS_OWNER, "", "Authentication username");
                   
    register_option(SOCKET_OPT_AUTH_PASSWORD, OPTION_TYPE_STRING, OPTION_CATEGORY_AUTH,
                   OPTION_ACCESS_PRIVILEGED, "", "Authentication password");
}

void SocketOptionManager::register_http_options() {
    register_option(HTTP_HEADERS, OPTION_TYPE_MAPPING, OPTION_CATEGORY_HTTP,
                   OPTION_ACCESS_PUBLIC, static_cast<void*>(nullptr), "HTTP request/response headers");
                   
    register_option(HTTP_METHOD, OPTION_TYPE_STRING, OPTION_CATEGORY_HTTP,
                   OPTION_ACCESS_PUBLIC, "GET", "HTTP request method");
                   
    register_option(HTTP_URL, OPTION_TYPE_STRING, OPTION_CATEGORY_HTTP,
                   OPTION_ACCESS_PUBLIC, "", "HTTP request URL");
                   
    register_option(HTTP_TIMEOUT, OPTION_TYPE_INTEGER, OPTION_CATEGORY_HTTP,
                   OPTION_ACCESS_PUBLIC, DEFAULT_HTTP_TIMEOUT, "HTTP request timeout", 
                   MIN_HTTP_TIMEOUT, MAX_HTTP_TIMEOUT);
                   
    register_option(HTTP_USER_AGENT, OPTION_TYPE_STRING, OPTION_CATEGORY_HTTP,
                   OPTION_ACCESS_PUBLIC, DEFAULT_HTTP_USER_AGENT, "HTTP User-Agent header");
                   
    register_option(HTTP_FOLLOW_REDIRECTS, OPTION_TYPE_BOOLEAN, OPTION_CATEGORY_HTTP,
                   OPTION_ACCESS_PUBLIC, true, "Follow HTTP redirects");
                   
    register_option(HTTP_MAX_REDIRECTS, OPTION_TYPE_INTEGER, OPTION_CATEGORY_HTTP,
                   OPTION_ACCESS_PUBLIC, DEFAULT_HTTP_MAX_REDIRECTS, "Maximum redirect count",
                   MIN_HTTP_MAX_REDIRECTS, MAX_HTTP_MAX_REDIRECTS);
}

void SocketOptionManager::register_rest_options() {
    // Note: REST options do NOT use SO_ prefix per compliance requirements
    register_option(REST_ROUTER_CONFIG, OPTION_TYPE_MAPPING, OPTION_CATEGORY_REST,
                   OPTION_ACCESS_OWNER, static_cast<void*>(nullptr), "REST router configuration");
                   
    register_option(REST_ADD_ROUTE, OPTION_TYPE_MAPPING, OPTION_CATEGORY_REST,
                   OPTION_ACCESS_OWNER, static_cast<void*>(nullptr), "Add REST API route");
                   
    register_option(REST_OPENAPI_INFO, OPTION_TYPE_MAPPING, OPTION_CATEGORY_REST,
                   OPTION_ACCESS_PUBLIC, static_cast<void*>(nullptr), "OpenAPI specification info");
                   
    register_option(REST_JWT_SECRET, OPTION_TYPE_STRING, OPTION_CATEGORY_REST,
                   OPTION_ACCESS_PRIVILEGED, "", "JWT signing secret");
                   
    register_option(REST_MIDDLEWARE, OPTION_TYPE_ARRAY, OPTION_CATEGORY_REST,
                   OPTION_ACCESS_OWNER, static_cast<void*>(nullptr), "REST middleware chain");
                   
    register_option(REST_CORS_CONFIG, OPTION_TYPE_MAPPING, OPTION_CATEGORY_REST,
                   OPTION_ACCESS_PUBLIC, static_cast<void*>(nullptr), "CORS configuration");
}

void SocketOptionManager::register_websocket_options() {
    // Note: WebSocket options do NOT use SO_ prefix per compliance requirements
    register_option(WS_PROTOCOL, OPTION_TYPE_STRING, OPTION_CATEGORY_WEBSOCKET,
                   OPTION_ACCESS_PUBLIC, "", "WebSocket subprotocol");
                   
    register_option(WS_EXTENSIONS, OPTION_TYPE_ARRAY, OPTION_CATEGORY_WEBSOCKET,
                   OPTION_ACCESS_PUBLIC, static_cast<void*>(nullptr), "WebSocket extensions");
                   
    register_option(WS_MAX_MESSAGE_SIZE, OPTION_TYPE_INTEGER, OPTION_CATEGORY_WEBSOCKET,
                   OPTION_ACCESS_PUBLIC, DEFAULT_WS_MAX_MESSAGE_SIZE, "Maximum message size",
                   MIN_WS_MESSAGE_SIZE, MAX_WS_MESSAGE_SIZE);
                   
    register_option(WS_PING_INTERVAL, OPTION_TYPE_INTEGER, OPTION_CATEGORY_WEBSOCKET,
                   OPTION_ACCESS_PUBLIC, DEFAULT_WS_PING_INTERVAL, "Ping interval in seconds");
                   
    register_option(WS_AUTO_PING, OPTION_TYPE_BOOLEAN, OPTION_CATEGORY_WEBSOCKET,
                   OPTION_ACCESS_PUBLIC, true, "Enable automatic ping/pong");
}

void SocketOptionManager::register_mqtt_options() {
    // Note: MQTT options do NOT use SO_ prefix per compliance requirements
    register_option(MQTT_BROKER, OPTION_TYPE_STRING, OPTION_CATEGORY_MQTT,
                   OPTION_ACCESS_OWNER, "", "MQTT broker hostname");
                   
    register_option(MQTT_CLIENT_ID, OPTION_TYPE_STRING, OPTION_CATEGORY_MQTT,
                   OPTION_ACCESS_OWNER, "", "MQTT client identifier");
                   
    register_option(MQTT_USERNAME, OPTION_TYPE_STRING, OPTION_CATEGORY_MQTT,
                   OPTION_ACCESS_OWNER, "", "MQTT username");
                   
    register_option(MQTT_PASSWORD, OPTION_TYPE_STRING, OPTION_CATEGORY_MQTT,
                   OPTION_ACCESS_PRIVILEGED, "", "MQTT password");
                   
    register_option(MQTT_KEEP_ALIVE, OPTION_TYPE_INTEGER, OPTION_CATEGORY_MQTT,
                   OPTION_ACCESS_PUBLIC, DEFAULT_MQTT_KEEP_ALIVE, "MQTT keepalive interval",
                   MIN_MQTT_KEEP_ALIVE, MAX_MQTT_KEEP_ALIVE);
                   
    register_option(MQTT_QOS, OPTION_TYPE_INTEGER, OPTION_CATEGORY_MQTT,
                   OPTION_ACCESS_PUBLIC, DEFAULT_MQTT_QOS, "MQTT Quality of Service", 0, 2);
}

void SocketOptionManager::register_external_options() {
    // Note: External options do NOT use SO_ prefix per compliance requirements
    register_option(EXTERNAL_COMMAND, OPTION_TYPE_STRING, OPTION_CATEGORY_EXTERNAL,
                   OPTION_ACCESS_PRIVILEGED, "", "External command to execute");
                   
    register_option(EXTERNAL_ARGS, OPTION_TYPE_ARRAY, OPTION_CATEGORY_EXTERNAL,
                   OPTION_ACCESS_PRIVILEGED, static_cast<void*>(nullptr), "Command arguments");
                   
    register_option(EXTERNAL_ENV, OPTION_TYPE_MAPPING, OPTION_CATEGORY_EXTERNAL,
                   OPTION_ACCESS_PRIVILEGED, static_cast<void*>(nullptr), "Environment variables");
                   
    register_option(EXTERNAL_WORKING_DIR, OPTION_TYPE_STRING, OPTION_CATEGORY_EXTERNAL,
                   OPTION_ACCESS_PRIVILEGED, "/tmp", "Working directory");
                   
    register_option(EXTERNAL_TIMEOUT, OPTION_TYPE_INTEGER, OPTION_CATEGORY_EXTERNAL,
                   OPTION_ACCESS_PUBLIC, DEFAULT_EXTERNAL_TIMEOUT, "Execution timeout",
                   MIN_EXTERNAL_TIMEOUT, MAX_EXTERNAL_TIMEOUT);
                   
    register_option(EXTERNAL_ASYNC, OPTION_TYPE_BOOLEAN, OPTION_CATEGORY_EXTERNAL,
                   OPTION_ACCESS_PUBLIC, false, "Asynchronous execution");
}

void SocketOptionManager::register_database_options() {
    register_option(DB_CONNECTION_STRING, OPTION_TYPE_STRING, OPTION_CATEGORY_DATABASE,
                   OPTION_ACCESS_PRIVILEGED, "", "Database connection string");
                   
    register_option(DB_POOL_SIZE, OPTION_TYPE_INTEGER, OPTION_CATEGORY_DATABASE,
                   OPTION_ACCESS_OWNER, 10, "Connection pool size", 1, 100);
                   
    register_option(DB_TIMEOUT, OPTION_TYPE_INTEGER, OPTION_CATEGORY_DATABASE,
                   OPTION_ACCESS_PUBLIC, DEFAULT_DB_TIMEOUT, "Query timeout in seconds", 1, 3600);
}

void SocketOptionManager::register_cache_options() {
    // Cache options use consistent protocol naming
    register_option(CACHE_ENABLE, OPTION_TYPE_BOOLEAN, OPTION_CATEGORY_CACHE,
                   OPTION_ACCESS_PUBLIC, false, "Enable response caching");
                   
    register_option(CACHE_TTL, OPTION_TYPE_INTEGER, OPTION_CATEGORY_CACHE,
                   OPTION_ACCESS_PUBLIC, DEFAULT_CACHE_TTL, "Cache time-to-live",
                   MIN_CACHE_TTL, MAX_CACHE_TTL);
                   
    register_option(CACHE_MAX_SIZE, OPTION_TYPE_INTEGER, OPTION_CATEGORY_CACHE,
                   OPTION_ACCESS_PUBLIC, 1048576, "Maximum cache size in bytes", 1024, 1073741824);
                   
    register_option(CACHE_KEY_PATTERN, OPTION_TYPE_STRING, OPTION_CATEGORY_CACHE,
                   OPTION_ACCESS_PUBLIC, "", "Cache key pattern template");
}

void SocketOptionManager::register_tls_options() {
    register_option(TLS_CIPHER_SUITES, OPTION_TYPE_STRING, OPTION_CATEGORY_TLS,
                   OPTION_ACCESS_PRIVILEGED, "", "TLS cipher suites");
                   
    register_option(TLS_CERTIFICATE_FILE, OPTION_TYPE_STRING, OPTION_CATEGORY_TLS,
                   OPTION_ACCESS_SYSTEM, "", "TLS certificate file path");
                   
    register_option(TLS_PRIVATE_KEY_FILE, OPTION_TYPE_STRING, OPTION_CATEGORY_TLS,
                   OPTION_ACCESS_SYSTEM, "", "TLS private key file path");
                   
    register_option(TLS_VERIFY_DEPTH, OPTION_TYPE_INTEGER, OPTION_CATEGORY_TLS,
                   OPTION_ACCESS_PRIVILEGED, 9, "Certificate verification depth", 1, 20);
}

void SocketOptionManager::register_graphql_options() {
    register_option(GRAPHQL_SCHEMA, OPTION_TYPE_STRING, OPTION_CATEGORY_GRAPHQL,
                   OPTION_ACCESS_OWNER, "", "GraphQL schema definition (SDL)");
                   
    register_option(GRAPHQL_INTROSPECTION, OPTION_TYPE_BOOLEAN, OPTION_CATEGORY_GRAPHQL,
                   OPTION_ACCESS_PUBLIC, true, "Enable GraphQL introspection");
                   
    register_option(GRAPHQL_PLAYGROUND, OPTION_TYPE_BOOLEAN, OPTION_CATEGORY_GRAPHQL,
                   OPTION_ACCESS_PUBLIC, true, "Enable GraphQL Playground");
                   
    register_option(GRAPHQL_MAX_QUERY_DEPTH, OPTION_TYPE_INTEGER, OPTION_CATEGORY_GRAPHQL,
                   OPTION_ACCESS_PUBLIC, DEFAULT_GRAPHQL_MAX_QUERY_DEPTH, "Maximum query depth",
                   MIN_GRAPHQL_QUERY_DEPTH, MAX_GRAPHQL_QUERY_DEPTH);
                   
    register_option(GRAPHQL_MAX_QUERY_COMPLEXITY, OPTION_TYPE_INTEGER, OPTION_CATEGORY_GRAPHQL,
                   OPTION_ACCESS_PUBLIC, DEFAULT_GRAPHQL_MAX_QUERY_COMPLEXITY, "Maximum query complexity",
                   MIN_GRAPHQL_QUERY_COMPLEXITY, MAX_GRAPHQL_QUERY_COMPLEXITY);
                   
    register_option(GRAPHQL_TIMEOUT, OPTION_TYPE_INTEGER, OPTION_CATEGORY_GRAPHQL,
                   OPTION_ACCESS_PUBLIC, DEFAULT_GRAPHQL_TIMEOUT, "GraphQL operation timeout",
                   MIN_GRAPHQL_TIMEOUT, MAX_GRAPHQL_TIMEOUT);
                   
    register_option(GRAPHQL_SUBSCRIPTIONS, OPTION_TYPE_BOOLEAN, OPTION_CATEGORY_GRAPHQL,
                   OPTION_ACCESS_PUBLIC, true, "Enable GraphQL subscriptions");
                   
    register_option(GRAPHQL_CORS_ORIGINS, OPTION_TYPE_ARRAY, OPTION_CATEGORY_GRAPHQL,
                   OPTION_ACCESS_PUBLIC, static_cast<void*>(nullptr), "CORS allowed origins");
                   
    register_option(GRAPHQL_RESOLVER_TIMEOUT, OPTION_TYPE_INTEGER, OPTION_CATEGORY_GRAPHQL,
                   OPTION_ACCESS_PUBLIC, DEFAULT_GRAPHQL_RESOLVER_TIMEOUT, "Resolver timeout in milliseconds",
                   1000, 60000);
                   
    register_option(GRAPHQL_QUERY_CACHE, OPTION_TYPE_BOOLEAN, OPTION_CATEGORY_GRAPHQL,
                   OPTION_ACCESS_PUBLIC, false, "Enable query result caching");
                   
    register_option(GRAPHQL_SCHEMA_FILE, OPTION_TYPE_STRING, OPTION_CATEGORY_GRAPHQL,
                   OPTION_ACCESS_OWNER, "", "Path to GraphQL schema file");
                   
    register_option(GRAPHQL_ENDPOINT_PATH, OPTION_TYPE_STRING, OPTION_CATEGORY_GRAPHQL,
                   OPTION_ACCESS_PUBLIC, "/graphql", "GraphQL endpoint path");
                   
    register_option(GRAPHQL_WS_ENDPOINT, OPTION_TYPE_STRING, OPTION_CATEGORY_GRAPHQL,
                   OPTION_ACCESS_PUBLIC, "/graphql/ws", "GraphQL WebSocket endpoint");
                   
    register_option(GRAPHQL_DEBUG_MODE, OPTION_TYPE_BOOLEAN, OPTION_CATEGORY_GRAPHQL,
                   OPTION_ACCESS_PRIVILEGED, false, "Enable GraphQL debug mode");
}

void SocketOptionManager::register_grpc_options() {
    register_option(GRPC_SERVICE_CONFIG, OPTION_TYPE_STRING, OPTION_CATEGORY_GRPC,
                   OPTION_ACCESS_OWNER, "", "gRPC service configuration (Protocol Buffers definition)");
                   
    register_option(GRPC_MAX_MESSAGE_SIZE, OPTION_TYPE_INTEGER, OPTION_CATEGORY_GRPC,
                   OPTION_ACCESS_PUBLIC, DEFAULT_GRPC_MAX_MESSAGE_SIZE, "Maximum message size in bytes",
                   MIN_GRPC_MESSAGE_SIZE, MAX_GRPC_MESSAGE_SIZE);
                   
    register_option(GRPC_KEEPALIVE_TIME, OPTION_TYPE_INTEGER, OPTION_CATEGORY_GRPC,
                   OPTION_ACCESS_PUBLIC, DEFAULT_GRPC_KEEPALIVE_TIME, "Keepalive time in milliseconds",
                   MIN_GRPC_KEEPALIVE_TIME, MAX_GRPC_KEEPALIVE_TIME);
                   
    register_option(GRPC_KEEPALIVE_TIMEOUT, OPTION_TYPE_INTEGER, OPTION_CATEGORY_GRPC,
                   OPTION_ACCESS_PUBLIC, DEFAULT_GRPC_KEEPALIVE_TIMEOUT, "Keepalive timeout in milliseconds",
                   1000, 60000);
                   
    register_option(GRPC_REFLECTION_ENABLE, OPTION_TYPE_BOOLEAN, OPTION_CATEGORY_GRPC,
                   OPTION_ACCESS_PUBLIC, true, "Enable gRPC server reflection");
                   
    register_option(GRPC_HEALTH_CHECK, OPTION_TYPE_BOOLEAN, OPTION_CATEGORY_GRPC,
                   OPTION_ACCESS_PUBLIC, true, "Enable gRPC health check service");
                   
    register_option(GRPC_COMPRESSION, OPTION_TYPE_STRING, OPTION_CATEGORY_GRPC,
                   OPTION_ACCESS_PUBLIC, "", "Compression algorithm (gzip, deflate, none)");
                   
    register_option(GRPC_METADATA, OPTION_TYPE_MAPPING, OPTION_CATEGORY_GRPC,
                   OPTION_ACCESS_PUBLIC, static_cast<void*>(nullptr), "Custom gRPC metadata headers");
                   
    register_option(GRPC_DEADLINE, OPTION_TYPE_INTEGER, OPTION_CATEGORY_GRPC,
                   OPTION_ACCESS_PUBLIC, DEFAULT_GRPC_DEADLINE, "Request deadline in milliseconds",
                   MIN_GRPC_DEADLINE, MAX_GRPC_DEADLINE);
                   
    register_option(GRPC_RETRY_POLICY, OPTION_TYPE_STRING, OPTION_CATEGORY_GRPC,
                   OPTION_ACCESS_PUBLIC, "", "Retry policy configuration (JSON)");
                   
    register_option(GRPC_TARGET_ADDRESS, OPTION_TYPE_STRING, OPTION_CATEGORY_GRPC,
                   OPTION_ACCESS_PUBLIC, "", "Target server address (host:port)");
                   
    register_option(GRPC_PROTO_FILE, OPTION_TYPE_STRING, OPTION_CATEGORY_GRPC,
                   OPTION_ACCESS_OWNER, "", "Protocol Buffers .proto file path");
                   
    register_option(GRPC_TLS_ENABLED, OPTION_TYPE_BOOLEAN, OPTION_CATEGORY_GRPC,
                   OPTION_ACCESS_PUBLIC, false, "Enable TLS/SSL encryption");
                   
    register_option(GRPC_TLS_CERT_FILE, OPTION_TYPE_STRING, OPTION_CATEGORY_GRPC,
                   OPTION_ACCESS_SYSTEM, "", "TLS certificate file path");
                   
    register_option(GRPC_TLS_KEY_FILE, OPTION_TYPE_STRING, OPTION_CATEGORY_GRPC,
                   OPTION_ACCESS_SYSTEM, "", "TLS private key file path");
                   
    register_option(GRPC_TLS_CA_FILE, OPTION_TYPE_STRING, OPTION_CATEGORY_GRPC,
                   OPTION_ACCESS_SYSTEM, "", "TLS CA certificate file path");
                   
    register_option(GRPC_MAX_CONNECTIONS, OPTION_TYPE_INTEGER, OPTION_CATEGORY_GRPC,
                   OPTION_ACCESS_PUBLIC, 100, "Maximum concurrent connections", 1, 10000);
                   
    register_option(GRPC_CONNECTION_TIMEOUT, OPTION_TYPE_INTEGER, OPTION_CATEGORY_GRPC,
                   OPTION_ACCESS_PUBLIC, DEFAULT_GRPC_CONNECTION_TIMEOUT, "Connection timeout in milliseconds",
                   1000, 300000);
                   
    register_option(GRPC_LOAD_BALANCING, OPTION_TYPE_STRING, OPTION_CATEGORY_GRPC,
                   OPTION_ACCESS_PUBLIC, "pick_first", "Load balancing policy (pick_first, round_robin)");
                   
    register_option(GRPC_DEBUG_MODE, OPTION_TYPE_BOOLEAN, OPTION_CATEGORY_GRPC,
                   OPTION_ACCESS_PRIVILEGED, false, "Enable gRPC debug mode");
}

void SocketOptionManager::register_internal_options() {
    register_option(SOCKET_OPT_SOCKET_MODE, OPTION_TYPE_INTEGER, OPTION_CATEGORY_INTERNAL,
                   OPTION_ACCESS_SYSTEM, 0, "Socket mode identifier");
                   
    register_option(REST_MODE, OPTION_TYPE_BOOLEAN, OPTION_CATEGORY_INTERNAL,
                   OPTION_ACCESS_SYSTEM, false, "REST mode active");
                   
    register_option(WS_MODE, OPTION_TYPE_BOOLEAN, OPTION_CATEGORY_INTERNAL,
                   OPTION_ACCESS_SYSTEM, false, "WebSocket mode active");
                   
    register_option(MQTT_MODE, OPTION_TYPE_BOOLEAN, OPTION_CATEGORY_INTERNAL,
                   OPTION_ACCESS_SYSTEM, false, "MQTT mode active");
                   
    register_option(EXTERNAL_MODE, OPTION_TYPE_BOOLEAN, OPTION_CATEGORY_INTERNAL,
                   OPTION_ACCESS_SYSTEM, false, "External mode active");
}

void SocketOptionManager::register_option(int option_id, socket_option_type type, 
                                         socket_option_category category, socket_option_access access,
                                         const svalue_t& default_val, const char* desc,
                                         long min_int, long max_int) {
    SocketOptionDescriptor desc_obj;
    desc_obj.type = type;
    desc_obj.category = category;
    desc_obj.access_level = access;
    desc_obj.default_value = default_val;
    desc_obj.description = desc;
    desc_obj.has_constraints = (type == OPTION_TYPE_INTEGER && min_int != max_int);
    
    if (desc_obj.has_constraints) {
        desc_obj.integer_constraints.min_val = min_int;
        desc_obj.integer_constraints.max_val = max_int;
    }
    
    option_descriptors_[option_id] = desc_obj;
}

void SocketOptionManager::register_option(int option_id, socket_option_type type,
                                         socket_option_category category, socket_option_access access,
                                         bool default_bool, const char* desc) {
    svalue_t default_val;
    default_val.type = T_NUMBER;
    default_val.u.number = default_bool ? 1 : 0;
    register_option(option_id, type, category, access, default_val, desc);
}

void SocketOptionManager::register_option(int option_id, socket_option_type type,
                                         socket_option_category category, socket_option_access access,
                                         const char* default_str, const char* desc) {
    svalue_t default_val;
    default_val.type = T_STRING;
    default_val.u.string = default_str;
    register_option(option_id, type, category, access, default_val, desc);
}

void SocketOptionManager::register_option(int option_id, socket_option_type type,
                                         socket_option_category category, socket_option_access access,
                                         int default_int, const char* desc, long min_val, long max_val) {
    svalue_t default_val;
    default_val.type = T_NUMBER;
    default_val.u.number = default_int;
    register_option(option_id, type, category, access, default_val, desc, min_val, max_val);
}

void SocketOptionManager::register_option(int option_id, socket_option_type type,
                                         socket_option_category category, socket_option_access access,
                                         void* default_ptr, const char* desc) {
    svalue_t default_val;
    if (default_ptr == nullptr) {
        default_val.type = T_INVALID;
    } else if (type == OPTION_TYPE_MAPPING) {
        default_val.type = T_MAPPING;
        default_val.u.map = static_cast<mapping_t*>(default_ptr);
    } else if (type == OPTION_TYPE_ARRAY) {
        default_val.type = T_ARRAY;
        default_val.u.arr = static_cast<array_t*>(default_ptr);
    } else {
        default_val.type = T_INVALID;
    }
    register_option(option_id, type, category, access, default_val, desc);
}

bool SocketOptionManager::set_option(int option, const svalue_t* value, object_t* caller) {
    if (!is_valid_socket_option(option)) {
        last_error_ = "Invalid socket option: " + std::to_string(option);
        return false;
    }
    
    if (!value) {
        last_error_ = "Null value provided for option " + std::to_string(option);
        return false;
    }
    
    // Check access permissions
    if (!has_access_permission(option, caller)) {
        last_error_ = "Access denied for option " + std::to_string(option);
        return false;
    }
    
    // Use our internal validation system
    if (!validate_option_value(option, value)) {
        return false;
    }
    
    // Type conversion if necessary
    svalue_t converted_value = *value;
    convert_to_type(&converted_value, get_option_type(option));
    
    // Store the option
    options_[option] = converted_value;
    
    // Handle special protocol mode options
    update_protocol_modes(option, &converted_value);
    
    return true;
}

bool SocketOptionManager::get_option(int option, svalue_t* result, object_t* caller) {
    if (!result) {
        last_error_ = "Null result pointer provided";
        return false;
    }
    
    if (!is_valid_socket_option(option)) {
        last_error_ = "Invalid socket option: " + std::to_string(option);
        return false;
    }
    
    // Check read access permissions
    if (!has_access_permission(option, caller)) {
        last_error_ = "Access denied for option " + std::to_string(option);
        return false;
    }
    
    auto it = options_.find(option);
    if (it != options_.end()) {
        *result = it->second;
        return true;
    }
    
    // Return default value if option not set
    *result = get_default_value(option);
    return true;
}

bool SocketOptionManager::has_option(int option) const {
    return options_.find(option) != options_.end();
}

bool SocketOptionManager::remove_option(int option, object_t* caller) {
    if (!is_valid_socket_option(option)) {
        last_error_ = "Invalid socket option: " + std::to_string(option);
        return false;
    }
    
    if (!has_access_permission(option, caller)) {
        last_error_ = "Access denied for option " + std::to_string(option);
        return false;
    }
    
    auto it = options_.find(option);
    if (it != options_.end()) {
        // Clean up any referenced data
        if (it->second.type & T_REFED) {
            // Handle reference cleanup - implementation would depend on FluffOS memory management
        }
        options_.erase(it);
        return true;
    }
    
    return false; // Option was not set
}

socket_option_type SocketOptionManager::get_option_type(int option) const {
    auto it = option_descriptors_.find(option);
    return (it != option_descriptors_.end()) ? it->second.type : OPTION_TYPE_MIXED;
}

socket_option_category SocketOptionManager::get_option_category(int option) const {
    auto it = option_descriptors_.find(option);
    return (it != option_descriptors_.end()) ? it->second.category : OPTION_CATEGORY_CORE;
}

socket_option_access SocketOptionManager::get_access_level(int option) const {
    auto it = option_descriptors_.find(option);
    return (it != option_descriptors_.end()) ? it->second.access_level : OPTION_ACCESS_PUBLIC;
}

const char* SocketOptionManager::get_option_description(int option) const {
    auto it = option_descriptors_.find(option);
    return (it != option_descriptors_.end()) ? it->second.description : "Unknown option";
}

bool SocketOptionManager::validate_option(int option, const svalue_t* value) const {
    return validate_option_value(option, value);
}

bool SocketOptionManager::set_options_from_mapping(const mapping_t* options, object_t* caller) {
    if (!options) {
        last_error_ = "Null mapping provided";
        return false;
    }
    
    // Validate all options first before setting any
    for (int i = 0; i < options->table_size; i++) {
        for (mapping_node_t* node = options->table[i]; node; node = node->next) {
            if (node->values[0].type != T_NUMBER) {
                last_error_ = "Option keys must be integers";
                return false;
            }
            
            int option = node->values[0].u.number;
            if (!validate_option(option, &node->values[1])) {
                return false;
            }
        }
    }
    
    // Now set all options
    for (int i = 0; i < options->table_size; i++) {
        for (mapping_node_t* node = options->table[i]; node; node = node->next) {
            int option = node->values[0].u.number;
            if (!set_option(option, &node->values[1], caller)) {
                return false;
            }
        }
    }
    
    return true;
}

mapping_t* SocketOptionManager::get_all_options(object_t* caller) const {
    // Simplified implementation - return empty mapping for now
    // Real implementation would require proper FluffOS mapping construction
    mapping_t* result = allocate_mapping(0);
    return result;
}

mapping_t* SocketOptionManager::get_options_by_category(socket_option_category category, object_t* caller) const {
    // Simplified implementation - return empty mapping for now
    mapping_t* result = allocate_mapping(0);
    return result;
}

bool SocketOptionManager::is_http_mode() const {
    auto it = options_.find(HTTP_SERVER_MODE);
    return (it != options_.end() && it->second.type == T_NUMBER && it->second.u.number);
}

bool SocketOptionManager::is_rest_mode() const {
    auto it = options_.find(REST_MODE);
    return (it != options_.end() && it->second.type == T_NUMBER && it->second.u.number);
}

bool SocketOptionManager::is_websocket_mode() const {
    auto it = options_.find(WS_MODE);
    return (it != options_.end() && it->second.type == T_NUMBER && it->second.u.number);
}

bool SocketOptionManager::is_mqtt_mode() const {
    auto it = options_.find(MQTT_MODE);
    return (it != options_.end() && it->second.type == T_NUMBER && it->second.u.number);
}

bool SocketOptionManager::is_external_mode() const {
    auto it = options_.find(EXTERNAL_MODE);
    return (it != options_.end() && it->second.type == T_NUMBER && it->second.u.number);
}

bool SocketOptionManager::is_cache_enabled() const {
    auto it = options_.find(CACHE_ENABLE);
    return (it != options_.end() && it->second.type == T_NUMBER && it->second.u.number);
}

void SocketOptionManager::clear_all_options() {
    for (auto& pair : options_) {
        // Clean up any referenced data
        if (pair.second.type & T_REFED) {
            // Handle reference cleanup - implementation would depend on FluffOS memory management
        }
    }
    options_.clear();
}

void SocketOptionManager::dump_options(outbuffer_t* buffer) const {
    if (!buffer) return;
    
    outbuf_add(buffer, "Socket Options for ID ");
    outbuf_addv(buffer, "%d:\n", socket_id_);
    
    for (const auto& pair : options_) {
        const char* category_name = get_category_name(get_option_category(pair.first));
        const char* type_name = get_type_name(get_option_type(pair.first));
        
        outbuf_addv(buffer, "  [%d] %s (%s:%s) = ", 
                   pair.first, get_option_description(pair.first), 
                   category_name, type_name);
        
        // Format value based on type
        switch (pair.second.type) {
            case T_NUMBER:
                outbuf_addv(buffer, "%d", pair.second.u.number);
                break;
            case T_STRING:
                outbuf_addv(buffer, "\"%s\"", pair.second.u.string);
                break;
            case T_REAL:
                outbuf_addv(buffer, "%g", pair.second.u.real);
                break;
            case T_MAPPING:
                outbuf_add(buffer, "<mapping>");
                break;
            case T_ARRAY:
                outbuf_add(buffer, "<array>");
                break;
            default:
                outbuf_add(buffer, "<unknown>");
                break;
        }
        outbuf_add(buffer, "\n");
    }
}

array_t* SocketOptionManager::get_option_names() const {
    array_t* result = allocate_array(option_descriptors_.size());
    if (!result) {
        return nullptr;
    }
    
    int index = 0;
    for (const auto& pair : option_descriptors_) {
        result->item[index].type = T_STRING;
        result->item[index].u.string = socket_option_to_string(pair.first);
        index++;
    }
    
    return result;
}

array_t* SocketOptionManager::get_categories() const {
    // Get unique categories
    std::set<socket_option_category> unique_categories;
    for (const auto& pair : option_descriptors_) {
        unique_categories.insert(pair.second.category);
    }
    
    array_t* result = allocate_array(unique_categories.size());
    if (!result) {
        return nullptr;
    }
    
    int index = 0;
    for (socket_option_category cat : unique_categories) {
        result->item[index].type = T_STRING;
        result->item[index].u.string = get_category_name(cat);
        index++;
    }
    
    return result;
}

// Private helper methods

bool SocketOptionManager::validate_option_value(int option, const svalue_t* value) const {
    auto it = option_descriptors_.find(option);
    if (it == option_descriptors_.end()) {
        last_error_ = "Unknown option: " + std::to_string(option);
        return false;
    }
    
    const SocketOptionDescriptor& desc = it->second;
    
    // Type validation
    switch (desc.type) {
        case OPTION_TYPE_INTEGER:
            if (value->type != T_NUMBER) {
                last_error_ = "Expected integer value for option " + std::to_string(option);
                return false;
            }
            return validate_integer_constraints(option, value->u.number);
            
        case OPTION_TYPE_STRING:
            if (value->type != T_STRING) {
                last_error_ = "Expected string value for option " + std::to_string(option);
                return false;
            }
            return validate_string_constraints(option, value->u.string);
            
        case OPTION_TYPE_BOOLEAN:
            if (value->type != T_NUMBER) {
                last_error_ = "Expected boolean value for option " + std::to_string(option);
                return false;
            }
            // Any integer value is acceptable for boolean (0 = false, !0 = true)
            return true;
            
        case OPTION_TYPE_FLOAT:
            if (value->type != T_REAL && value->type != T_NUMBER) {
                last_error_ = "Expected numeric value for option " + std::to_string(option);
                return false;
            }
            {
                double val = (value->type == T_REAL) ? value->u.real : static_cast<double>(value->u.number);
                return validate_float_constraints(option, val);
            }
            
        case OPTION_TYPE_MAPPING:
            if (value->type != T_MAPPING) {
                last_error_ = "Expected mapping value for option " + std::to_string(option);
                return false;
            }
            return validate_mapping_structure(option, value->u.map);
            
        case OPTION_TYPE_ARRAY:
            if (value->type != T_ARRAY) {
                last_error_ = "Expected array value for option " + std::to_string(option);
                return false;
            }
            return validate_array_structure(option, value->u.arr);
            
        case OPTION_TYPE_MIXED:
            return true; // Any type is acceptable for mixed
            
        default:
            last_error_ = "Invalid option type for option " + std::to_string(option);
            return false;
    }
}

bool SocketOptionManager::has_access_permission(int option, object_t* caller) const {
    socket_option_access required_access = get_access_level(option);
    socket_option_access caller_access = get_caller_access_level(caller);
    
    return caller_access >= required_access;
}

socket_option_access SocketOptionManager::get_caller_access_level(object_t* caller) const {
    if (!caller) {
        return OPTION_ACCESS_SYSTEM; // System calls have highest access
    }
    
    // Check if caller is the socket owner
    if (check_system_permission(caller)) {
        return OPTION_ACCESS_SYSTEM;
    }
    
    // Check socket ownership  
    if (is_socket_owner(caller)) {
        return OPTION_ACCESS_OWNER;
    }
    
    return OPTION_ACCESS_PUBLIC;
}

bool SocketOptionManager::check_system_permission(object_t* caller) const {
    if (!caller) return true; // System calls
    
    // Simplified system privilege check - would need to be implemented
    // based on actual FluffOS privilege system
    return false; // Conservative approach for now
}

bool SocketOptionManager::is_socket_owner(object_t* caller) const {
    if (!caller) return true;
    
    // Implementation depends on how socket ownership is tracked in FluffOS
    // This would typically check if the caller is the object that created the socket
    return get_socket_owner(socket_id_) == caller;
}

void SocketOptionManager::convert_to_type(svalue_t* value, socket_option_type target_type) {
    if (!value) return;
    
    switch (target_type) {
        case OPTION_TYPE_BOOLEAN:
            if (value->type == T_NUMBER) {
                // Already correct type, just normalize to 0/1
                value->u.number = value->u.number ? 1 : 0;
            } else if (value->type == T_STRING) {
                // Convert string to boolean
                const char* str = value->u.string;
                if (strcasecmp(str, "true") == 0 || strcasecmp(str, "yes") == 0 || strcasecmp(str, "1") == 0) {
                    value->type = T_NUMBER;
                    value->u.number = 1;
                } else {
                    value->type = T_NUMBER;
                    value->u.number = 0;
                }
            }
            break;
            
        case OPTION_TYPE_INTEGER:
            if (value->type == T_REAL) {
                // Convert float to integer
                value->type = T_NUMBER;
                value->u.number = static_cast<LPC_INT>(value->u.real);
            } else if (value->type == T_STRING) {
                // Convert string to integer
                value->type = T_NUMBER;
                value->u.number = atoi(value->u.string);
            }
            break;
            
        case OPTION_TYPE_FLOAT:
            if (value->type == T_NUMBER) {
                // Convert integer to float
                LPC_FLOAT val = static_cast<LPC_FLOAT>(value->u.number);
                value->type = T_REAL;
                value->u.real = val;
            } else if (value->type == T_STRING) {
                // Convert string to float
                value->type = T_REAL;
                value->u.real = atof(value->u.string);
            }
            break;
            
        case OPTION_TYPE_STRING:
            if (value->type == T_NUMBER) {
                // Convert integer to string - would need proper string allocation
                char buffer[32];
                snprintf(buffer, sizeof(buffer), "%ld", value->u.number);
                // This is simplified - real implementation would use FluffOS string allocation
                value->type = T_STRING;
                value->u.string = make_shared_string(buffer);
            } else if (value->type == T_REAL) {
                // Convert float to string
                char buffer[64];
                snprintf(buffer, sizeof(buffer), "%g", value->u.real);
                value->type = T_STRING;
                value->u.string = make_shared_string(buffer);
            }
            break;
            
        default:
            // No conversion needed or not supported
            break;
    }
}

svalue_t SocketOptionManager::get_default_value(int option) {
    auto it = option_descriptors_.find(option);
    if (it != option_descriptors_.end()) {
        return it->second.default_value;
    }
    
    // Return null value if no default found
    svalue_t null_val;
    null_val.type = T_INVALID;
    return null_val;
}

bool SocketOptionManager::validate_integer_constraints(int option, long value) const {
    auto it = option_descriptors_.find(option);
    if (it == option_descriptors_.end()) {
        return false;
    }
    
    const SocketOptionDescriptor& desc = it->second;
    if (!desc.has_constraints) {
        return true;
    }
    
    if (value < desc.integer_constraints.min_val || value > desc.integer_constraints.max_val) {
        last_error_ = "Value " + std::to_string(value) + " is outside valid range [" +
                     std::to_string(desc.integer_constraints.min_val) + ", " +
                     std::to_string(desc.integer_constraints.max_val) + "] for option " +
                     std::to_string(option);
        return false;
    }
    
    return true;
}

bool SocketOptionManager::validate_string_constraints(int option, const char* value) const {
    if (!value) {
        last_error_ = "Null string value for option " + std::to_string(option);
        return false;
    }
    
    auto it = option_descriptors_.find(option);
    if (it == option_descriptors_.end()) {
        return false;
    }
    
    const SocketOptionDescriptor& desc = it->second;
    if (!desc.has_constraints) {
        return true;
    }
    
    size_t len = strlen(value);
    if (len < desc.string_constraints.min_length || len > desc.string_constraints.max_length) {
        last_error_ = "String length " + std::to_string(len) + " is outside valid range [" +
                     std::to_string(desc.string_constraints.min_length) + ", " +
                     std::to_string(desc.string_constraints.max_length) + "] for option " +
                     std::to_string(option);
        return false;
    }
    
    // Regex validation if specified
    if (desc.validation_regex && strlen(desc.validation_regex) > 0) {
        try {
            std::regex pattern(desc.validation_regex);
            if (!std::regex_match(value, pattern)) {
                last_error_ = "String value '" + std::string(value) + "' does not match required pattern for option " +
                             std::to_string(option);
                return false;
            }
        } catch (const std::regex_error& e) {
            last_error_ = "Invalid regex pattern for option " + std::to_string(option);
            return false;
        }
    }
    
    return true;
}

bool SocketOptionManager::validate_float_constraints(int option, double value) const {
    auto it = option_descriptors_.find(option);
    if (it == option_descriptors_.end()) {
        return false;
    }
    
    const SocketOptionDescriptor& desc = it->second;
    if (!desc.has_constraints) {
        return true;
    }
    
    if (value < desc.float_constraints.min_val || value > desc.float_constraints.max_val) {
        last_error_ = "Value " + std::to_string(value) + " is outside valid range [" +
                     std::to_string(desc.float_constraints.min_val) + ", " +
                     std::to_string(desc.float_constraints.max_val) + "] for option " +
                     std::to_string(option);
        return false;
    }
    
    return true;
}

bool SocketOptionManager::validate_mapping_structure(int option, const mapping_t* value) const {
    if (!value) {
        last_error_ = "Null mapping value for option " + std::to_string(option);
        return false;
    }
    
    // Basic mapping validation - could be extended for specific option requirements
    return true;
}

bool SocketOptionManager::validate_array_structure(int option, const array_t* value) const {
    if (!value) {
        last_error_ = "Null array value for option " + std::to_string(option);
        return false;
    }
    
    // Basic array validation - could be extended for specific option requirements
    return true;
}

int SocketOptionManager::get_socket_mode_from_options() const {
    auto it = options_.find(SOCKET_OPT_SOCKET_MODE);
    if (it != options_.end() && it->second.type == T_NUMBER) {
        return it->second.u.number;
    }
    return -1; // Unknown mode
}

void SocketOptionManager::update_protocol_modes(int option, const svalue_t* value) {
    // Update internal mode flags based on option settings
    switch (option) {
        case REST_ROUTER_CONFIG:
        case REST_ADD_ROUTE:
            set_internal_mode(REST_MODE, true);
            break;
            
        case WS_PROTOCOL:
        case WS_EXTENSIONS:
            set_internal_mode(WS_MODE, true);
            break;
            
        case MQTT_BROKER:
        case MQTT_CLIENT_ID:
            set_internal_mode(MQTT_MODE, true);
            break;
            
        case EXTERNAL_COMMAND:
            set_internal_mode(EXTERNAL_MODE, true);
            break;
    }
}

void SocketOptionManager::set_internal_mode(int mode_option, bool enabled) {
    svalue_t mode_val;
    mode_val.type = T_NUMBER;
    mode_val.u.number = enabled ? 1 : 0;
    options_[mode_option] = mode_val;
}

// Utility functions for type and category names
const char* SocketOptionManager::get_type_name(socket_option_type type) const {
    switch (type) {
        case OPTION_TYPE_INTEGER: return "integer";
        case OPTION_TYPE_STRING: return "string";
        case OPTION_TYPE_BOOLEAN: return "boolean";
        case OPTION_TYPE_FLOAT: return "float";
        case OPTION_TYPE_MAPPING: return "mapping";
        case OPTION_TYPE_ARRAY: return "array";
        case OPTION_TYPE_MIXED: return "mixed";
        default: return "unknown";
    }
}

const char* SocketOptionManager::get_category_name(socket_option_category category) const {
    switch (category) {
        case OPTION_CATEGORY_CORE: return "core";
        case OPTION_CATEGORY_AUTH: return "auth";
        case OPTION_CATEGORY_HTTP: return "http";
        case OPTION_CATEGORY_REST: return "rest";
        case OPTION_CATEGORY_WEBSOCKET: return "websocket";
        case OPTION_CATEGORY_MQTT: return "mqtt";
        case OPTION_CATEGORY_EXTERNAL: return "external";
        case OPTION_CATEGORY_DATABASE: return "database";
        case OPTION_CATEGORY_CACHE: return "cache";
        case OPTION_CATEGORY_APACHE: return "apache";
        case OPTION_CATEGORY_TLS: return "tls";
        case OPTION_CATEGORY_MONITORING: return "monitoring";
        case OPTION_CATEGORY_INTERNAL: return "internal";
        default: return "unknown";
    }
}

// Helper function to convert svalue_t types for internal use
bool SocketOptionManager::svalue_matches_type(const svalue_t* value, socket_option_type expected_type) const {
    if (!value) return false;
    
    switch (expected_type) {
        case OPTION_TYPE_INTEGER:
            return (value->type == T_NUMBER);
        case OPTION_TYPE_STRING:
            return (value->type == T_STRING);
        case OPTION_TYPE_BOOLEAN:
            return (value->type == T_NUMBER); // Booleans stored as numbers
        case OPTION_TYPE_FLOAT:
            return (value->type == T_REAL || value->type == T_NUMBER);
        case OPTION_TYPE_MAPPING:
            return (value->type == T_MAPPING);
        case OPTION_TYPE_ARRAY:
            return (value->type == T_ARRAY);
        case OPTION_TYPE_MIXED:
            return true; // Any type allowed
        default:
            return false;
    }
}

/*
 * Global utility function implementations
 */

const SocketOptionDescriptor* get_option_descriptor(int option) {
    if (!SocketOptionManager::descriptors_initialized_) {
        return nullptr;
    }
    
    auto it = SocketOptionManager::option_descriptors_.find(option);
    return (it != SocketOptionManager::option_descriptors_.end()) ? &it->second : nullptr;
}

bool validate_socket_option(int option, const svalue_t* value, const char** error_msg) {
    // Basic validation - check if option exists
    if (!is_valid_socket_option(option)) {
        if (error_msg) {
            static std::string error_str = "Invalid socket option: " + std::to_string(option);
            *error_msg = error_str.c_str();
        }
        return false;
    }
    
    // Get expected type and validate
    const SocketOptionDescriptor* desc = get_option_descriptor(option);
    if (desc && value) {
        socket_option_type expected_type = desc->type;
        
        switch (expected_type) {
            case OPTION_TYPE_INTEGER:
                if (value->type != T_NUMBER) {
                    if (error_msg) {
                        static std::string error_str = "Expected integer value for option " + std::to_string(option);
                        *error_msg = error_str.c_str();
                    }
                    return false;
                }
                break;
            case OPTION_TYPE_STRING:
                if (value->type != T_STRING) {
                    if (error_msg) {
                        static std::string error_str = "Expected string value for option " + std::to_string(option);
                        *error_msg = error_str.c_str();
                    }
                    return false;
                }
                break;
            // Add other type checks as needed
        }
    }
    
    return true;
}

const char* socket_option_to_string(int option) {
    // Map option IDs to string names
    switch (option) {
        case SOCKET_OPT_TLS_VERIFY_PEER: return "SOCKET_OPT_TLS_VERIFY_PEER";
        case SOCKET_OPT_TLS_SNI_HOSTNAME: return "SOCKET_OPT_TLS_SNI_HOSTNAME";
        case SOCKET_OPT_KEEPALIVE: return "SOCKET_OPT_KEEPALIVE";
        case SOCKET_OPT_NODELAY: return "SOCKET_OPT_NODELAY";
        case SOCKET_OPT_TIMEOUT: return "SOCKET_OPT_TIMEOUT";
        case HTTP_HEADERS: return "HTTP_HEADERS";
        case HTTP_METHOD: return "HTTP_METHOD";
        case HTTP_URL: return "HTTP_URL";
        case REST_ADD_ROUTE: return "REST_ADD_ROUTE";
        case REST_JWT_SECRET: return "REST_JWT_SECRET";
        case WS_PROTOCOL: return "WS_PROTOCOL";
        case WS_MAX_MESSAGE_SIZE: return "WS_MAX_MESSAGE_SIZE";
        case MQTT_BROKER: return "MQTT_BROKER";
        case MQTT_CLIENT_ID: return "MQTT_CLIENT_ID";
        case EXTERNAL_COMMAND: return "EXTERNAL_COMMAND";
        case CACHE_ENABLE: return "CACHE_ENABLE";
        case CACHE_TTL: return "CACHE_TTL";
        default: return "UNKNOWN_OPTION";
    }
}

int string_to_socket_option(const char* name) {
    if (!name) return -1;
    
    // Map string names to option IDs
    if (strcmp(name, "SOCKET_OPT_TLS_VERIFY_PEER") == 0) return SOCKET_OPT_TLS_VERIFY_PEER;
    if (strcmp(name, "SOCKET_OPT_TLS_SNI_HOSTNAME") == 0) return SOCKET_OPT_TLS_SNI_HOSTNAME;
    if (strcmp(name, "SOCKET_OPT_KEEPALIVE") == 0) return SOCKET_OPT_KEEPALIVE;
    if (strcmp(name, "SOCKET_OPT_NODELAY") == 0) return SOCKET_OPT_NODELAY;
    if (strcmp(name, "SOCKET_OPT_TIMEOUT") == 0) return SOCKET_OPT_TIMEOUT;
    if (strcmp(name, "HTTP_HEADERS") == 0) return HTTP_HEADERS;
    if (strcmp(name, "HTTP_METHOD") == 0) return HTTP_METHOD;
    if (strcmp(name, "HTTP_URL") == 0) return HTTP_URL;
    if (strcmp(name, "REST_ADD_ROUTE") == 0) return REST_ADD_ROUTE;
    if (strcmp(name, "WS_PROTOCOL") == 0) return WS_PROTOCOL;
    if (strcmp(name, "MQTT_BROKER") == 0) return MQTT_BROKER;
    if (strcmp(name, "EXTERNAL_COMMAND") == 0) return EXTERNAL_COMMAND;
    if (strcmp(name, "CACHE_ENABLE") == 0) return CACHE_ENABLE;
    if (strcmp(name, "CACHE_TTL") == 0) return CACHE_TTL;
    
    return -1; // Unknown option
}

array_t* get_options_in_category(socket_option_category category) {
    std::vector<int> matching_options;
    
    for (const auto& pair : SocketOptionManager::option_descriptors_) {
        if (pair.second.category == category) {
            matching_options.push_back(pair.first);
        }
    }
    
    array_t* result = allocate_array(matching_options.size());
    if (!result) {
        return nullptr;
    }
    
    for (size_t i = 0; i < matching_options.size(); i++) {
        result->item[i].type = T_NUMBER;
        result->item[i].u.number = matching_options[i];
    }
    
    return result;
}

bool is_valid_socket_option(int option) {
    return SocketOptionManager::option_descriptors_.find(option) != 
           SocketOptionManager::option_descriptors_.end();
}

bool is_protocol_option(int option) {
    return IS_REST_OPTION(option) || IS_WS_OPTION(option) || 
           IS_MQTT_OPTION(option) || IS_EXTERNAL_OPTION(option);
}

bool requires_protocol_mode(int option, socket_mode_extended mode) {
    if (IS_REST_OPTION(option)) {
        return (mode == REST_SERVER || mode == REST_CLIENT);
    } else if (IS_WS_OPTION(option)) {
        return (mode == WEBSOCKET_SERVER || mode == WEBSOCKET_CLIENT ||
                mode == WEBSOCKET_TLS_SERVER || mode == WEBSOCKET_TLS_CLIENT);
    } else if (IS_MQTT_OPTION(option)) {
        return (mode == MQTT_CLIENT || mode == MQTT_TLS_CLIENT);
    } else if (IS_EXTERNAL_OPTION(option)) {
        return (mode == EXTERNAL_PROCESS || mode == EXTERNAL_COMMAND_MODE);
    }
    
    return false; // Core options don't require specific modes
}