/*
 * WebSocket Package LPC Efun Exports
 * 
 * This file defines the LPC efuns exported by the WebSocket package
 * for FluffOS unified socket architecture integration.
 */

// WebSocket Server Functions
mixed websocket_create_server(string address, int port, void|mapping options);
int websocket_bind_server(object server, string address, int port);
int websocket_close_server(object server);

// WebSocket Client Functions  
int websocket_connect(string url, void|mapping options);
int websocket_client_handshake(int socket, string url, void|mapping headers);

// WebSocket Message Operations
int websocket_send_text(int socket, string message);
int websocket_send_binary(int socket, bytes data);
int websocket_send_ping(int socket, void|string payload);
int websocket_send_pong(int socket, void|string payload);

// WebSocket Frame Operations
mapping websocket_parse_frame(bytes frame_data);
bytes websocket_build_frame(int opcode, mixed payload, void|int mask);

// WebSocket Connection Management
int websocket_upgrade_connection(int socket, void|mapping options);
int websocket_close_connection(int socket, void|int code, void|string reason);
mapping websocket_get_connection_info(int socket);

// WebSocket Protocol Operations
int websocket_set_subprotocol(int socket, string protocol);
string websocket_get_subprotocol(int socket);
int websocket_negotiate_extensions(int socket, string|array extensions);
array websocket_get_extensions(int socket);

// WebSocket Validation and Security
int websocket_validate_frame(bytes frame_data);
int websocket_check_origin(int socket, string origin);
string websocket_generate_key();
string websocket_compute_accept(string key);

// WebSocket State Management
int websocket_get_state(int socket);
int websocket_set_ping_interval(int socket, int interval);
int websocket_get_ping_interval(int socket);
int websocket_set_max_message_size(int socket, int size);

// WebSocket Compression Support
int websocket_enable_compression(int socket, void|mapping options);
int websocket_disable_compression(int socket);
int websocket_is_compression_enabled(int socket);

// WebSocket Statistics and Monitoring
mapping websocket_get_stats(int socket);
void websocket_reset_stats(int socket);
array websocket_list_connections();

/*
 * WebSocket Constants
 */

// WebSocket States
#define WS_STATE_CONNECTING    0
#define WS_STATE_OPEN         1
#define WS_STATE_CLOSING      2
#define WS_STATE_CLOSED       3

// WebSocket Opcodes
#define WS_OPCODE_CONTINUATION 0x0
#define WS_OPCODE_TEXT        0x1
#define WS_OPCODE_BINARY      0x2
#define WS_OPCODE_CLOSE       0x8
#define WS_OPCODE_PING        0x9
#define WS_OPCODE_PONG        0xA

// WebSocket Close Codes
#define WS_CLOSE_NORMAL       1000
#define WS_CLOSE_GOING_AWAY   1001
#define WS_CLOSE_PROTOCOL_ERROR 1002
#define WS_CLOSE_UNSUPPORTED  1003
#define WS_CLOSE_NO_STATUS    1005
#define WS_CLOSE_ABNORMAL     1006
#define WS_CLOSE_INVALID_DATA 1007
#define WS_CLOSE_POLICY_VIOLATION 1008
#define WS_CLOSE_TOO_LARGE    1009
#define WS_CLOSE_EXTENSION_REQUIRED 1010
#define WS_CLOSE_UNEXPECTED   1011

// WebSocket Frame Flags
#define WS_FRAME_FIN          0x80
#define WS_FRAME_MASK         0x80

// WebSocket Extension Names
#define WS_EXT_PERMESSAGE_DEFLATE "permessage-deflate"
#define WS_EXT_X_WEBKIT_DEFLATE   "x-webkit-deflate-frame"

// WebSocket Error Codes  
#define WS_ERROR_NONE         0
#define WS_ERROR_INVALID_FRAME -1
#define WS_ERROR_PROTOCOL     -2
#define WS_ERROR_CONNECTION   -3
#define WS_ERROR_HANDSHAKE    -4
#define WS_ERROR_COMPRESSION  -5
#define WS_ERROR_SECURITY     -6