# Unified Socket Architecture for FluffOS

**A comprehensive, elegant solution for network protocol integration**

## Overview

This document presents the unified socket architecture that consolidates all network protocols (HTTP, REST, WebSocket, MQTT, External Processes) under a single, consistent API. This architecture eliminates redundant efuns, provides elegant configuration through socket options, and maintains backward compatibility while dramatically simplifying the codebase.

**Core Principle**: **ONE API TO RULE THEM ALL** - All network protocols use `socket_create()` with specialized mode handlers.

## Background

The current FluffOS implementation has architectural inconsistencies:
- ✅ **Well-integrated**: HTTP and External packages use socket handlers correctly
- ❌ **Poorly integrated**: REST package has 11 separate efuns that bypass the socket system
- ❌ **Redundant**: WebSocket and MQTT have wrapper efuns that just call socket handlers

This unified architecture resolves these inconsistencies while building on the excellent socket handler pattern already established.

## Current Problems Solved

### Problem 1: Redundant APIs
```c
// ❌ BEFORE: Multiple ways to do the same thing
int ws1 = socket_create(WEBSOCKET_SERVER, "callback", "close");           // Works via handler
int ws2 = websocket_socket_create(WEBSOCKET_SERVER, "callback", "close"); // Redundant wrapper
int mqtt1 = socket_create(MQTT_CLIENT, "callback", "close");              // Works via handler  
int mqtt2 = mqtt_socket_create("broker_url", "callback", "close");        // Redundant wrapper

// ✅ AFTER: Single consistent API
int ws_server = socket_create(WEBSOCKET_SERVER, "ws_callback", "ws_close");
int mqtt_client = socket_create(MQTT_CLIENT, "mqtt_callback", "mqtt_close");
socket_set_option(mqtt_client, MQTT_BROKER, "mqtt://broker:1883");
```

### Problem 2: REST Package Bypasses Socket System
```c
// ❌ BEFORE: 11 separate REST efuns
int router = rest_create_router();
rest_add_route(router, "GET", "/users/{id}", "handler");
rest_set_route_docs(router, "GET", "/users/{id}", docs);
mapping result = rest_process_route(router, request);

// ✅ AFTER: Integrated with socket system
int rest_socket = socket_create(REST_SERVER, "rest_callback", "rest_close");
socket_set_option(rest_socket, REST_ADD_ROUTE, route_definition);
socket_set_option(rest_socket, REST_OPENAPI_INFO, api_metadata);
// Requests automatically routed and processed via unified callback
```

## Architecture Overview

### Socket Mode Organization

Based on the [socket_create_extension_proposal.md](socket_create_extension_proposal.md), socket modes are organized by package dependencies with proper spacing for future expansion:

```c
enum socket_mode {
    // Core FluffOS modes: 0-9
    MUD = 0, STREAM = 1, DATAGRAM = 2, STREAM_BINARY = 3,
    DATAGRAM_BINARY = 4, STREAM_TLS = 5, STREAM_TLS_BINARY = 6,
    STREAM_COMPRESSED = 7, STREAM_TLS_COMPRESSED = 8, DATAGRAM_COMPRESSED = 9,
    
    // HTTP/REST modes: 20-25 (PACKAGE_HTTP + PACKAGE_REST)
    HTTP_SERVER = 20, HTTPS_SERVER = 21, HTTP_CLIENT = 22, HTTPS_CLIENT = 23,
    REST_SERVER = 24, REST_CLIENT = 25,
    
    // WebSocket/MQTT modes: 30-37 (libwebsockets)
    WEBSOCKET_SERVER = 30, WEBSOCKET_CLIENT = 31,
    WEBSOCKET_SECURE_SERVER = 32, WEBSOCKET_SECURE_CLIENT = 33,
    WEBSOCKET_FILE_STREAM = 34, WEBSOCKET_BINARY_STREAM = 35,
    WEBSOCKET_COMPRESSED_NATIVE = 36, MQTT_CLIENT = 37,
    
    // External process modes: 40-44 (PACKAGE_EXTERNAL)
    EXTERNAL_PIPE = 40, EXTERNAL_SOCKETPAIR = 41, EXTERNAL_FIFO = 42,
    EXTERNAL_EVENTFD = 43, EXTERNAL_INOTIFY = 44
};
```

### Enhanced Socket Options System

All protocol configuration is handled through `socket_set_option()` and `socket_get_option()`:

```c
enum socket_options {
    // Core socket options (existing): 0-99
    SO_TLS_VERIFY_PEER = 0,
    SO_TLS_SNI_HOSTNAME = 1,
    
    // HTTP/REST options: 100-119
    HTTP_HEADERS = 100,
    HTTP_METHOD = 101,
    HTTP_URL = 102,
    HTTP_BODY = 103,
    HTTP_TIMEOUT = 104,
    HTTP_USER_AGENT = 105,
    HTTP_FOLLOW_REDIRECTS = 106,
    HTTP_MAX_REDIRECTS = 107,
    HTTP_CONNECT_TIMEOUT = 108,
    HTTP_READ_TIMEOUT = 109,
    
    // REST configuration: 110-119
    REST_ROUTER_CONFIG = 110,        // Set router metadata
    REST_ADD_ROUTE = 111,            // Add route definition
    REST_OPENAPI_INFO = 112,         // Set OpenAPI metadata
    REST_JWT_SECRET = 113,           // Set JWT secret
    REST_DOCS_PATH = 114,            // Set docs endpoint
    REST_VALIDATION_SCHEMA = 115,    // Set validation schemas
    REST_MIDDLEWARE = 116,           // Set middleware chain
    REST_ERROR_HANDLER = 117,        // Set error handler
    REST_CORS_CONFIG = 118,          // Set CORS configuration
    REST_RATE_LIMIT = 119,           // Set rate limiting
    
    // WebSocket options: 120-129
    WS_PROTOCOL = 120,
    WS_EXTENSIONS = 121,
    WS_ORIGIN = 122,
    WS_MAX_MESSAGE_SIZE = 123,
    WS_PING_INTERVAL = 124,
    WS_PONG_TIMEOUT = 125,
    WS_COMPRESSION = 126,
    WS_SUBPROTOCOL = 127,
    
    // MQTT options: 130-139
    MQTT_BROKER = 130,
    MQTT_CLIENT_ID = 131,
    MQTT_USERNAME = 132,
    MQTT_PASSWORD = 133,
    MQTT_KEEP_ALIVE = 134,
    MQTT_QOS = 135,
    MQTT_RETAIN = 136,
    MQTT_CLEAN_SESSION = 137,
    MQTT_WILL_TOPIC = 138,
    MQTT_WILL_MESSAGE = 139,
    
    // External options: 140-149
    EXTERNAL_COMMAND = 140,
    EXTERNAL_ARGS = 141,
    EXTERNAL_ENV = 142,
    EXTERNAL_WATCH_PATH = 143,
    EXTERNAL_WORKING_DIR = 144,
    EXTERNAL_USER = 145,
    EXTERNAL_GROUP = 146,
    EXTERNAL_TIMEOUT = 147,
    EXTERNAL_BUFFER_SIZE = 148,
    EXTERNAL_ASYNC = 149,
    
    // Internal mode flags: 150-159
    HTTP_SERVER_MODE = 150,
    HTTP_CLIENT_MODE = 151,
    REST_MODE = 152,
    WS_MODE = 153,
    MQTT_MODE = 154,
    EXTERNAL_MODE = 155,
    
    // Cache options: 200-219
    CACHE_ENABLE = 200,
    CACHE_TTL = 201,
    CACHE_MAX_SIZE = 202,
    CACHE_KEY_PATTERN = 203,
    CACHE_HEADERS = 204,
    CACHE_METHODS = 205,
    CACHE_EXCLUDE_PATTERNS = 206,
    CACHE_THREAD_POOL_SIZE = 207,
    CACHE_INVALIDATE_PATTERN = 208,
    CACHE_STATISTICS = 209,
    
    // Apache integration: 300-319
    APACHE_FASTCGI_ENABLE = 300,
    APACHE_FASTCGI_SOCKET = 301,
    APACHE_PROXY_ENABLE = 302,
    APACHE_PROXY_UPSTREAM = 303,
    APACHE_STATIC_HANDOFF = 304,
    APACHE_DYNAMIC_HANDOFF = 305,
    APACHE_CGI_ENV_VARS = 306,
    APACHE_AUTH_PASSTHROUGH = 307,
    APACHE_SSL_TERMINATION = 308,
    APACHE_LOAD_BALANCE = 309
};
```

### Two-Layer Implementation

#### Layer 1: FluffOS Driver (C++)
The driver registers socket mode handlers during package initialization:

```cpp
// File: src/packages/http/http.cc
static void init_http_socket_handlers() {
    static int initialized = 0;
    if (initialized) return;
    
    // Register C++ handlers for HTTP/REST modes
    register_socket_create_handler(HTTP_SERVER, http_socket_create_handler);
    register_socket_create_handler(REST_SERVER, rest_socket_create_handler);
    
    // Register C++ handlers for WebSocket modes
    register_socket_create_handler(WEBSOCKET_SERVER, websocket_socket_create_handler);
    register_socket_create_handler(MQTT_CLIENT, mqtt_socket_create_handler);
    
    initialized = 1;
}
```

#### Layer 2: MUD Library (LPC)
MUD developers use the unified socket API in their LPC code:

```lpc
// File: /lib/secure/daemon/web_d.c
void create() {
    setup_rest_api();
    setup_websocket_server();
    setup_mqtt_client();
}

void setup_rest_api() {
    int rest_socket = socket_create(REST_SERVER, "handle_rest_request", "rest_connection_closed");
    
    // Configure REST API with OpenAPI
    mapping api_config = ([
        "title": "MUD REST API",
        "version": "1.0.0",
        "base_path": "/api/v1"
    ]);
    socket_set_option(rest_socket, REST_OPENAPI_INFO, api_config);
    
    // Add documented routes
    mapping user_route = ([
        "method": "GET",
        "pattern": "/users/{id}",
        "handler": "get_user",
        "openapi": ([
            "summary": "Get user by ID",
            "parameters": ([
                "id": (["type": "integer", "description": "User ID"])
            ]),
            "responses": ([
                "200": ([
                    "description": "User found",
                    "schema": user_schema
                ])
            ])
        ])
    ]);
    socket_set_option(rest_socket, REST_ADD_ROUTE, user_route);
    
    socket_bind(rest_socket, 8080);
    socket_listen(rest_socket, "accept_rest_connection");
}
```

## Usage Examples

### REST API with OpenAPI Documentation

```lpc
// Create REST server with full OpenAPI support
int rest_socket = socket_create(REST_SERVER, "rest_request", "rest_close");

// Set OpenAPI metadata
mapping api_info = ([
    "title": "MUD REST API",
    "version": "1.0.0",
    "description": "FluffOS MUD REST API with OpenAPI documentation",
    "servers": ({ (["url": "https://mud.example.com/api"]) })
]);
socket_set_option(rest_socket, REST_OPENAPI_INFO, api_info);

// Add routes with documentation
mapping user_route = ([
    "method": "GET",
    "pattern": "/users/{id}",
    "handler": "get_user_handler",
    "openapi": ([
        "summary": "Get user by ID",
        "parameters": ([
            "id": (["type": "integer", "description": "User ID"])
        ]),
        "responses": ([
            "200": ([
                "description": "User found",
                "schema": (["type": "object", "properties": user_schema])
            ])
        ])
    ])
]);
socket_set_option(rest_socket, REST_ADD_ROUTE, user_route);

// Set JWT authentication
socket_set_option(rest_socket, REST_JWT_SECRET, "secret_key");

// Enable automatic documentation at /docs
socket_set_option(rest_socket, REST_DOCS_PATH, "/docs");

socket_bind(rest_socket, 8080);
socket_listen(rest_socket, "rest_accept");

// Unified callback handles all REST requests
void rest_request(int socket, mapping request, string addr) {
    // request mapping contains everything:
    // - HTTP: method, path, headers, body, query_params
    // - REST: path_params, route_match, validation_results
    // - OpenAPI: schema_validation, auth_status
    
    string method = request["method"];
    string path = request["path"];
    mapping path_params = request["path_params"];
    
    // Automatic routing and validation
    if(!request["validation_passed"]) {
        socket_write(socket, ([
            "status": 400,
            "body": json_encode((["error": "Invalid request"]))
        ]));
        return;
    }
    
    // Handle authenticated request
    switch(path) {
        case "/users/*":
            handle_users_api(socket, request);
            break;
        case "/docs":
            serve_openapi_docs(socket);
            break;
        default:
            send_404(socket);
    }
}
```

### WebSocket Server with Protocol Detection

```lpc
// WebSocket server with automatic protocol detection
int ws_socket = socket_create(WEBSOCKET_SERVER, "ws_message", "ws_close");

// Set supported subprotocols
socket_set_option(ws_socket, WS_PROTOCOL, ({"chat", "game", "admin"}));

// Enable compression
socket_set_option(ws_socket, WS_EXTENSIONS, ({"permessage-deflate"}));

socket_bind(ws_socket, 8080);
socket_listen(ws_socket, "ws_accept");

void ws_message(int socket, mapping message, string addr) {
    string protocol = message["protocol"];
    mixed data = message["data"];
    
    switch(protocol) {
        case "chat":
            broadcast_chat(data["message"], data["user"]);
            break;
        case "game":
            process_game_command(socket, data);
            break;
        case "admin":
            if(message["auth_valid"]) {
                process_admin_command(socket, data);
            }
            break;
    }
}
```

### External Process Monitoring

```lpc
// File system monitoring with automatic configuration reload
int watcher = socket_create(EXTERNAL_INOTIFY, "file_changed", "watch_error");

// Set watch path with filtering
socket_set_option(watcher, EXTERNAL_WATCH_PATH, "/mud/lib/secure/cfg/");

socket_bind(watcher, 0);  // Auto-bind for external modes

void file_changed(int socket, mapping event, string addr) {
    string filename = event["filename"];
    string action = event["action"];
    
    // Auto-reload configuration on changes
    if(filename == "quantumscape.cfg" && action == "modified") {
        log_system("Configuration file changed, scheduling reload");
        call_out("reload_config", 2);  // Debounced reload
    }
}
```

### MQTT Client Integration

```lpc
// MQTT client with automatic reconnection
int mqtt_socket = socket_create(MQTT_CLIENT, "mqtt_message", "mqtt_close");

// Configure MQTT connection
socket_set_option(mqtt_socket, MQTT_BROKER, "mqtt://broker.example.com:1883");
socket_set_option(mqtt_socket, MQTT_CLIENT_ID, "mud_server_" + random(10000));
socket_set_option(mqtt_socket, MQTT_USERNAME, "muduser");
socket_set_option(mqtt_socket, MQTT_PASSWORD, "mudpass");
socket_set_option(mqtt_socket, MQTT_KEEP_ALIVE, 60);

socket_connect(mqtt_socket, "broker.example.com", "mqtt_connected", "mqtt_error");

void mqtt_message(int socket, mapping message, string addr) {
    string topic = message["topic"];
    mixed payload = message["payload"];
    int qos = message["qos"];
    
    // Handle different message types
    switch(topic) {
        case "mud/players/login":
            handle_player_login_notification(payload);
            break;
        case "mud/system/status":
            update_system_status(payload);
            break;
    }
}
```

## Benefits

### Consistency
- **Single API**: `socket_create()` for all network protocols
- **Unified callbacks**: Same signature across all protocols  
- **Standard options**: `socket_set_option()` for all configuration
- **Predictable patterns**: Same workflow for all socket types

### Simplicity
- **No mode confusion**: Clear numeric organization by package
- **No redundant efuns**: One way to create sockets
- **Auto-configuration**: Intelligent defaults with option overrides
- **Unified documentation**: Single reference for all protocols

### Functionality
- **Full protocol support**: HTTP/HTTPS, REST, WebSocket, MQTT, External processes
- **Advanced features**: OpenAPI generation, JWT authentication, compression, TLS
- **Monitoring capabilities**: File watching, process coordination, log monitoring
- **Extensibility**: Easy to add new protocols and options

### Maintainability
- **Package isolation**: Each package handles its own modes
- **Handler registration**: Clean separation of concerns
- **Option-driven config**: No hardcoded behavior
- **Consistent error handling**: Same patterns across all protocols

## Migration Path

### Phase 1: Remove Redundant EFUNs (Critical)
1. Mark `websocket_socket_create()` and `mqtt_socket_create()` as deprecated
2. Update documentation to use `socket_create()` with appropriate modes
3. Eventually remove redundant wrapper efuns

### Phase 2: Integrate REST Package (High Priority)
1. Implement socket option handlers for REST configuration
2. Add REST socket mode handlers that use existing REST implementation
3. Migrate REST efuns to use socket options internally
4. Provide backward compatibility layer
5. Deprecate separate REST efuns in favor of socket API

### Phase 3: Enhanced Options System (Medium Priority)
1. Implement new socket options (REST_*, WS_*, MQTT_*, etc.)
2. Add option validation and error handling
3. Update socket_set_option() and socket_get_option() implementations

### Phase 4: Package Structure Optimization (Low Priority)
1. Simplify package dependencies
2. Optimize handler registration system
3. Improve build system integration

## Backward Compatibility

All existing code continues to work:
- Current `socket_create()` calls remain unchanged
- Existing REST efuns can be maintained as compatibility wrappers
- WebSocket and MQTT wrapper efuns can delegate to socket handlers
- Migration can happen gradually over multiple releases

## Implementation Notes

### Handler Registration
Each package registers its socket mode handlers during initialization:

```cpp
// HTTP package registers modes 20-25 and 30-37
void init_http_package() {
    register_socket_create_handler(HTTP_SERVER, http_socket_create_handler);
    register_socket_create_handler(REST_SERVER, rest_socket_create_handler);
    register_socket_create_handler(WEBSOCKET_SERVER, websocket_socket_create_handler);
    register_socket_create_handler(MQTT_CLIENT, mqtt_socket_create_handler);
}

// External package registers modes 40-44
void init_external_package() {
    register_socket_create_handler(EXTERNAL_PIPE, external_socket_create_handler);
    register_socket_create_handler(EXTERNAL_INOTIFY, external_socket_create_handler);
}
```

### Socket Option Implementation
Socket options are handled through a dispatch system:

```cpp
int socket_set_option_impl(int fd, int option, svalue_t *value) {
    switch(option) {
        case REST_ADD_ROUTE:
            return rest_add_route_option(fd, value);
        case REST_OPENAPI_INFO:
            return rest_set_openapi_info(fd, value);
        case MQTT_BROKER:
            return mqtt_set_broker(fd, value);
        // ... other options
    }
}
```

### Unified Callback Interface
All protocols use the same callback signature with protocol-specific data in the mapping:

```lpc
void protocol_callback(int socket, mapping data, string addr) {
    // data mapping contains protocol-specific information:
    // HTTP: method, path, headers, body, query_params
    // REST: path_params, route_match, validation_results, openapi_docs
    // WebSocket: message_type, protocol, binary_data, frame_info
    // MQTT: topic, payload, qos, retain_flag
    // External: event_type, file_path, process_id, signal_number
}
```

## Conclusion

This unified socket architecture provides a comprehensive, elegant solution that:

1. **Eliminates redundancy** by removing duplicate APIs
2. **Maintains consistency** with a single socket creation pattern
3. **Provides advanced functionality** through the socket options system
4. **Ensures maintainability** through clean package separation
5. **Enables extensibility** for future protocols and features

The architecture builds on FluffOS's proven socket handler system while modernizing the API to support complex protocols like REST with OpenAPI, WebSocket with subprotocols, and external process integration.

**Result**: A unified, powerful, and elegant network programming interface that makes FluffOS competitive with modern application frameworks while maintaining the simplicity that MUD developers expect.

## References

- [socket_create_extension_proposal.md](socket_create_extension_proposal.md) - Original socket extension proposal
- FluffOS Socket Package Documentation
- libwebsockets Documentation
- OpenAPI 3.0 Specification