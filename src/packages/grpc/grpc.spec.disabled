//
// gRPC Package Function Specifications
//
// This file defines the LPC-callable functions provided by the gRPC package
// for the FluffOS unified socket system.
//

#ifdef PACKAGE_GRPC

//
// Socket management functions
//

// Create a new gRPC socket (client or server mode)
// Returns: socket file descriptor on success, -1 on error
    int grpc_create_socket(int);

// Configure a gRPC socket with options
// Returns: 1 on success, 0 on failure
    int grpc_configure_socket(int, mapping);

// Close a gRPC socket
// Returns: 1 on success, 0 on failure  
    int grpc_close_socket(int);

// Get status of a gRPC socket
// Returns: status code (1=connected, 0=disconnected, -1=invalid)
    int grpc_socket_status(int);

//
// Service registration functions (for servers)
//

// Register a gRPC service with protocol definition
// Returns: 1 on success, 0 on failure
    int grpc_register_service(int, string, string);

// Register a method handler for a service
// Returns: 1 on success, 0 on failure
    int grpc_register_method(int, string, string, string);

// Unregister a gRPC service
// Returns: 1 on success, 0 on failure
    int grpc_unregister_service(int, string);

//
// Client method invocation functions
//

// Call a gRPC method synchronously
// Returns: mapping with response data and status
    mapping grpc_call_method(int, string, string, string, mapping);

// Call a gRPC method asynchronously
// Returns: 1 on success, 0 on failure
    int grpc_call_method_async(int, string, string, string, string);

//
// Streaming functions
//

// Start a client streaming RPC
// Returns: 1 on success, 0 on failure
    int grpc_start_client_stream(int, string, string);

// Start a server streaming RPC  
// Returns: 1 on success, 0 on failure
    int grpc_start_server_stream(int, string, string, string);

// Start a bidirectional streaming RPC
// Returns: 1 on success, 0 on failure
    int grpc_start_bidirectional_stream(int, string, string);

// Send a message on an active stream
// Returns: 1 on success, 0 on failure
    int grpc_send_stream_message(int, string);

// Read a message from an active stream
// Returns: mapping with message data, or 0 if no message available
    mixed grpc_read_stream_message(int);

// Finish a streaming RPC
// Returns: mapping with final status and any remaining data
    mapping grpc_finish_stream(int);

//
// Protocol Buffers functions
//

// Load protocol definition from file
// Returns: 1 on success, 0 on failure
    int grpc_load_proto_file(string);

// Load protocol definition from string
// Returns: 1 on success, 0 on failure
    int grpc_load_proto_string(string);

// Get names of all loaded services
// Returns: array of service names
    string *grpc_get_service_names();

// Get method names for a service
// Returns: array of method names
    string *grpc_get_method_names(string);

// Get detailed information about a method
// Returns: mapping with method details
    mapping grpc_get_method_details(string, string);

// Serialize a message using Protocol Buffers
// Returns: serialized data string
    string grpc_serialize_message(string, mapping);

// Deserialize a Protocol Buffers message
// Returns: mapping with deserialized data
    mapping grpc_deserialize_message(string, string);

// Validate message data against Protocol Buffers schema
// Returns: 1 if valid, 0 if invalid
    int grpc_validate_message(string, mapping);

//
// Channel management functions
//

// Create a gRPC channel to a target address
// Returns: channel ID string on success, 0 on failure
    string grpc_create_channel(string, mapping);

// Close a gRPC channel
// Returns: 1 on success, 0 on failure
    int grpc_close_channel(string);

// Check if a channel is ready for use
// Returns: 1 if ready, 0 if not ready
    int grpc_channel_ready(string);

// Get statistics for a channel
// Returns: mapping with channel statistics
    mapping grpc_channel_stats(string);

// Get list of all active channels
// Returns: array of channel ID strings
    string *grpc_active_channels();

//
// Configuration and utility functions
//

// Get current gRPC package configuration
// Returns: mapping with configuration settings
    mapping grpc_get_config();

// Enable or disable debug mode
// Returns: 1 on success, 0 on failure
    int grpc_set_debug_mode(int);

// Get gRPC package information
// Returns: mapping with package details
    mapping grpc_get_package_info();

// Get gRPC package version
// Returns: version string
    string grpc_version();

#endif /* PACKAGE_GRPC */