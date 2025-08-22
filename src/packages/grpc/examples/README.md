# gRPC Package Examples

This directory contains example Protocol Buffer service definitions and LPC test files demonstrating how to use the FluffOS unified socket system with gRPC services.

## Protocol Buffer Service Definitions

### mud_player.proto
Defines the `PlayerService` for player management operations:
- **GetPlayer** - Retrieve player information
- **UpdatePlayer** - Update player data  
- **CreatePlayer** - Create new player accounts
- **AuthenticatePlayer** - Player authentication
- **FindPlayers** - Search for players with filters
- **GetPlayerStats** - Retrieve detailed player statistics
- **GetOnlinePlayers** - List currently online players

### mud_world.proto  
Defines the `WorldService` for world/room management:
- **GetRoom** - Retrieve room information
- **UpdateRoom** - Modify room properties
- **CreateRoom** - Create new rooms dynamically
- **FindRooms** - Search rooms by area, terrain, etc.
- **GetRoomObjects** - List objects in a room
- **MovePlayer** - Move players between rooms
- **AddObjectToRoom/RemoveObjectFromRoom** - Manage room inventory

### mud_command.proto
Defines the `CommandService` for command execution:
- **ExecuteCommand** - Execute MUD commands remotely
- **ValidateCommand** - Validate command syntax
- **ParseCommand** - Parse command into components
- **GetAvailableCommands** - List available commands
- **ExecuteBatch** - Execute multiple commands in sequence

### intermud.proto
Defines the `IntermudService` for inter-MUD communication:
- **RegisterMud** - Register MUD in network
- **SendTell/SendWho/SendFinger** - Player communication
- **Channel operations** - Join/leave/send to channels
- **File transfers** - Send/receive files between MUDs
- **GameEvent syncing** - Synchronize game events

## LPC Test Files

### test_grpc_player.lpc
Demonstrates gRPC client usage for player services:
- Creates gRPC client socket
- Configures connection parameters
- Tests GetPlayer, AuthenticatePlayer, FindPlayers, GetPlayerStats
- Shows proper error handling and cleanup

### test_grpc_world.lpc  
Demonstrates world service client operations:
- Tests room retrieval and search
- Demonstrates room object management
- Shows player movement via gRPC
- Tests dynamic room creation

### test_grpc_server.lpc
Shows how to implement a gRPC server:
- Creates and configures gRPC server socket
- Registers service method handlers
- Implements actual service logic
- Handles client connections and requests
- Demonstrates health check and reflection services

## Usage Instructions

### Prerequisites
1. gRPC C++ libraries installed (`libgrpc++-dev libprotobuf-dev`)
2. FluffOS compiled with gRPC package enabled
3. Protocol Buffer compiler (`protoc`) available

### Running the Examples

1. **Start a gRPC server:**
   ```lpc
   // In your MUD
   load_object("/packages/grpc/examples/test_grpc_server");
   ```

2. **Test client operations:**
   ```lpc  
   // Load and run client tests
   load_object("/packages/grpc/examples/test_grpc_player");
   load_object("/packages/grpc/examples/test_grpc_world");
   ```

3. **Monitor server status:**
   ```lpc
   object server = load_object("/packages/grpc/examples/test_grpc_server");
   write(server->query_server_status());
   ```

### Configuration Options

Key socket options for gRPC services:

```lpc
mapping grpc_options = ([
    GRPC_TARGET_ADDRESS: "localhost:50051",     // Server address
    GRPC_PROTO_FILE: "/path/to/service.proto", // Service definition
    GRPC_SERVICE_CONFIG: "package.ServiceName", // Service to use
    GRPC_DEADLINE: 5000,                        // Request timeout (ms)
    GRPC_MAX_MESSAGE_SIZE: 1048576,            // Max message size
    GRPC_COMPRESSION: "gzip",                   // Compression algorithm
    GRPC_KEEPALIVE_TIME: 30000,                // Keepalive interval
    GRPC_TLS_ENABLED: 1,                       // Enable SSL/TLS
    GRPC_DEBUG_MODE: 1                         // Enable debug logging
]);
```

### Error Handling

All gRPC operations return standard gRPC status codes:
- `GRPC_OK` (0) - Success
- `GRPC_CANCELLED` (1) - Operation cancelled  
- `GRPC_INVALID_ARGUMENT` (3) - Invalid request
- `GRPC_DEADLINE_EXCEEDED` (4) - Request timeout
- `GRPC_NOT_FOUND` (5) - Resource not found
- `GRPC_UNAUTHENTICATED` (16) - Authentication failed
- `GRPC_INTERNAL` (13) - Internal server error

### Integration with MUD Systems

The gRPC services are designed to integrate with existing MUD systems:

- **Player Service** - Works with user objects and player databases
- **World Service** - Integrates with room objects and area management  
- **Command Service** - Uses existing command parser and execution system
- **Intermud Service** - Enables communication between multiple MUDs

### Security Considerations

- Use TLS encryption for production deployments
- Implement proper authentication and authorization
- Validate all incoming requests
- Rate limit client connections
- Use the master daemon's `valid_socket()` function for security validation

### Performance Notes

- gRPC uses HTTP/2 for efficient multiplexing
- Protocol Buffers provide compact binary serialization
- Connection pooling reduces overhead for multiple requests
- Compression can reduce bandwidth usage significantly

### Troubleshooting

1. **Build Issues:**
   - Ensure gRPC libraries are installed
   - Check CMake detected gRPC correctly (`HAVE_GRPC=1`)
   - Verify protoc compiler is available

2. **Runtime Errors:**
   - Check debug logs when `GRPC_DEBUG_MODE=1`
   - Verify service definitions are loaded correctly
   - Ensure proper socket option configuration

3. **Connection Problems:**
   - Check network connectivity and firewalls
   - Verify server is bound to correct address/port
   - Test with simple unary calls before streaming

For more information, see the main gRPC package documentation and FluffOS socket system reference.