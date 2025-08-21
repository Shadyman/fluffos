// WebSocket Package - Unified Socket Architecture Integration
// 
// WebSocket functionality is provided through the unified socket system:
//   int socket = socket_create(WEBSOCKET_SERVER, "callback", "close");
//   socket_set_option(socket, WS_PROTOCOL, "chat");
//   socket_bind(socket, port, address);
//
// This package provides socket mode handlers for WebSocket protocols.
// No separate efuns are exported - everything goes through socket_*() functions.