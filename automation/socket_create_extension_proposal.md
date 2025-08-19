# socket_create() Extension Proposal

## Current Socket Modes (0-6)

```c
enum socket_mode {
  MUD = 0,              // Telnet protocol
  STREAM = 1,           // Raw TCP
  DATAGRAM = 2,         // UDP
  STREAM_BINARY = 3,    // Binary TCP
  DATAGRAM_BINARY = 4,  // Binary UDP
  STREAM_TLS = 5,       // TLS TCP
  STREAM_TLS_BINARY = 6 // Binary TLS TCP
};
```

## Proposed Extended Socket Modes (with proper spacing)

```c
// Core FluffOS modes: 0-6 (existing)

// Core socket compression modes (PACKAGE_COMPRESS)
STREAM_COMPRESSED = 7,      // TCP with compression (for raw data transfer)
STREAM_TLS_COMPRESSED = 8,  // TLS with compression (for secure bulk data)
DATAGRAM_COMPRESSED = 9,    // UDP with compression (for log streaming, etc.)

// Reserved for future core expansion: 10-19

// Note: Core socket compression modes (7-9) handle raw protocol compression
// HTTP/HTTPS/WebSocket/REST have native compression support via libwebsockets

// HTTP-based modes (PACKAGE_HTTP - requires libwebsockets)
HTTP_SERVER = 20,       // HTTP server socket
HTTPS_SERVER = 21,      // HTTPS server socket
HTTP_CLIENT = 22,       // HTTP client socket
HTTPS_CLIENT = 23,      // HTTPS client socket
REST_SERVER = 24,       // REST API server (requires PACKAGE_REST)
REST_CLIENT = 25,       // REST API client (requires PACKAGE_REST)
// Reserved for HTTP/REST expansion: 26-29

// WebSocket-based modes (libwebsockets - existing integration)
WEBSOCKET_SERVER = 30,  // WebSocket server
WEBSOCKET_CLIENT = 31,  // WebSocket client
WEBSOCKET_SECURE_SERVER = 32, // WSS server (WebSocket over TLS)
WEBSOCKET_SECURE_CLIENT = 33, // WSS client (WebSocket over TLS)
WEBSOCKET_FILE_STREAM = 34,   // File streaming over WebSocket
WEBSOCKET_BINARY_STREAM = 35, // Binary data streaming
WEBSOCKET_COMPRESSED_NATIVE = 36, // WebSocket with permessage-deflate (native)
MQTT_CLIENT = 37,       // MQTT client (libwebsockets - client only)
// Reserved for libwebsockets expansion: 38-39

// External process integration (PACKAGE_EXTERNAL)
EXTERNAL_PIPE = 40,     // Unix pipe to external process
EXTERNAL_SOCKETPAIR = 41, // Unix socketpair for process communication
EXTERNAL_FIFO = 42,     // Named FIFO (named pipe) communication
EXTERNAL_EVENTFD = 43,  // Linux eventfd for signaling (safe, isolated)
EXTERNAL_INOTIFY = 44,  // File system event monitoring (safe with whitelist)
// Reserved for external integration: 45-49

// Advanced integration modes
// Reserved for complex multi-package features: 50-79

// Custom/Plugin modes (for external packages)
// Reserved: 90-99

// Future expansion: 100+
```

## Implementation Strategy

### Phase 1: HTTP Integration (Modes 10-13)

- Integrate libwebsockets HTTP functionality into socket_create()
- HTTP_SERVER/HTTPS_SERVER/HTTP_CLIENT/HTTPS_CLIENT modes
- Use existing libwebsockets context from websocket.cc
- Callback structure similar to existing socket callbacks
- Only available when PACKAGE_HTTP is compiled

### Phase 2: WebSocket Integration (Modes 20-21)

- WEBSOCKET_SERVER/WEBSOCKET_CLIENT modes use existing WebSocket implementation
- Bridge between socket_create() API and current websocket.cc
- Unified callback interface
- Available when websockets are enabled in build

### Phase 3: REST Integration (Modes 30-31)

- REST_SERVER/REST_CLIENT modes require PACKAGE_REST to be compiled
- Built on top of HTTP modes (10-13)
- Provides automatic URL routing and JSON handling
- Integrates with rest_* EFUNs

### Phase 4: Advanced Protocols (Modes 40-43)

- MQTT and CoAP client/server modes
- Requires PACKAGE_LIBWS_EXTENDED
- Uses advanced libwebsockets protocol support
- Provide high-level REST API over HTTP sockets
- Automatic JSON parsing/formatting

## Example Usage

### Core Socket Modes

```lpc
// Core compression modes
int stream_socket = socket_create(STREAM_COMPRESSED, "compressed_data", "stream_error");
int tls_socket = socket_create(STREAM_TLS_COMPRESSED, "secure_data", "tls_error");
int udp_socket = socket_create(DATAGRAM_COMPRESSED, "udp_data", "udp_error");
```

### HTTP/REST Modes

```lpc
// HTTP server
int http_socket = socket_create(HTTP_SERVER, "http_read_callback", "http_close_callback");

// REST API server
int rest_socket = socket_create(REST_SERVER, "rest_request_callback", "rest_close_callback");
```

### WebSocket Modes

```lpc
// WebSocket server
int ws_socket = socket_create(WEBSOCKET_SERVER, "ws_message_callback", "ws_close_callback");

// MQTT client (client only - no server support in libwebsockets)
int mqtt_socket = socket_create(MQTT_CLIENT, "mqtt_message_callback", "mqtt_close_callback");
```

### External Process Integration

```lpc
// Unix pipe to external command
int pipe_socket = socket_create(EXTERNAL_PIPE, "pipe_data", "pipe_error");

// Unix socketpair for bidirectional communication
int pair_socket = socket_create(EXTERNAL_SOCKETPAIR, "pair_data", "pair_error");

// Named FIFO
int fifo_socket = socket_create(EXTERNAL_FIFO, "fifo_data", "fifo_error");
socket_bind(fifo_socket, "/tmp/mud_fifo");

// Linux eventfd for signaling
int event_socket = socket_create(EXTERNAL_EVENTFD, "event_notification", "event_error");

// File system monitoring
int watch_socket = socket_create(EXTERNAL_INOTIFY, "file_changed", "watch_error");
socket_bind(watch_socket, "/mud/lib/secure/cfg/");
```

### Practical Examples

**Real-time Configuration Monitoring:**

```lpc
void setup_config_watcher() {
    int watch_socket = socket_create(EXTERNAL_INOTIFY, "config_file_changed", "watch_error");

    // Monitor configuration directory for changes
    socket_bind(watch_socket, "/mud/lib/secure/cfg/");

    log_system("Configuration file monitoring started");
}

void config_file_changed(int socket, mapping event) {
    string filename = event["filename"];
    string action = event["action"];  // "modified", "created", "deleted"

    switch(filename) {
        case "quantumscape.cfg":
            if(action == "modified") {
                log_system("Main config file changed, scheduling reload");
                call_out("reload_main_config", 5);  // 5 second delay
            }
            break;

        case "external_commands.cfg":
            if(action == "modified") {
                log_system("External commands config changed");
                reload_external_commands();
            }
            break;
    }
}

void watch_error(int socket, string error) {
    log_error("Config file watching failed: " + error);
    call_out("setup_config_watcher", 60);  // Retry in 60 seconds
}
```

**Process Coordination with EventFD:**

```lpc
// Global coordination between MUD processes
int coordination_event;

void setup_process_coordination() {
    coordination_event = socket_create(EXTERNAL_EVENTFD, "coordination_signal", "coord_error");

    // Initialize counter to 0
    socket_write(coordination_event, 0);

    log_system("Process coordination initialized");
}

void signal_maintenance_mode() {
    // Increment event counter to signal all processes
    socket_write(coordination_event, 1);
    log_system("Maintenance mode signal sent to all processes");
}

void coordination_signal(int socket, int counter_value) {
    if(counter_value > 0) {
        log_system("Received maintenance mode signal, preparing for shutdown");

        // Notify all players
        broadcast_system("Server entering maintenance mode in 5 minutes");

        // Start graceful shutdown sequence
        call_out("begin_maintenance_shutdown", 300);  // 5 minutes

        // Reset counter after handling
        socket_write(coordination_event, 0);
    }
}

void coord_error(int socket, string error) {
    log_error("Process coordination error: " + error);
    call_out("setup_process_coordination", 30);
}
```

**Log File Monitoring:**

```lpc
void setup_log_monitoring() {
    int log_watcher = socket_create(EXTERNAL_INOTIFY, "log_file_updated", "log_watch_error");

    // Monitor log directory for new entries
    socket_bind(log_watcher, "/mud/lib/log/");

    log_system("Log file monitoring started");
}

void log_file_updated(int socket, mapping event) {
    string filename = event["filename"];
    string action = event["action"];

    // Look for critical error patterns in runtime logs
    if(filename == "runtime" && action == "modified") {
        check_for_critical_errors();
    }

    // Monitor security log for suspicious activity
    if(filename == "security" && action == "modified") {
        analyze_security_events();
    }

    // Watch for new crash dumps
    if(sscanf(filename, "crash_%*s") == 1 && action == "created") {
        alert_administrators("Crash dump created: " + filename);
    }
}

void check_for_critical_errors() {
    // Read recent log entries and check for critical patterns
    string log_content = read_file("/mud/lib/log/runtime", -100);  // Last 100 lines

    if(sizeof(regexp(({log_content}), "FATAL|PANIC|SEGFAULT"))) {
        alert_administrators("Critical error detected in runtime log");

        // Optionally trigger automatic backup
        call_out("emergency_backup", 1);
    }
}
```

## Benefits

1. **Unified API**: Single socket_create() interface for all protocols
2. **Leverage libwebsockets**: Use existing HTTP/2, WebSocket, MQTT support
3. **Backward Compatible**: Existing modes 0-6 unchanged
4. **Conditional Compilation**: Advanced modes only available if packages compiled
5. **Performance**: Native libwebsockets implementation vs pure LPC

## Implementation Notes

- Requires modifications to socket_efuns.cc and socket_efuns.h
- New socket modes would check for package availability at runtime
- libwebsockets context sharing between websocket.cc and socket implementation
- New callback signatures for HTTP/REST/MQTT specific events
