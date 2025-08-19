#ifndef SOCKET_HTTP_INTEGRATION_H_
#define SOCKET_HTTP_INTEGRATION_H_

#include "base/package_api.h"
#include "packages/sockets/socket_efuns.h"
#include "packages/sockets/http_handler.h"

/*
 * Socket HTTP Integration Header
 * 
 * This header defines the integration points between HTTP handlers
 * and the FluffOS socket system. It provides hooks for socket events,
 * lifecycle management, and HTTP-specific processing.
 */

/*
 * Socket Event Processing
 */

// Process incoming data for HTTP sockets
bool socket_http_process_read_data(int socket_fd, const char* data, size_t length);

// Trigger HTTP request callback when request is complete
bool socket_http_trigger_request_callback(int socket_fd);

// Setup HTTP options for socket
bool socket_http_setup_options(int socket_fd, const mapping_t* options);

// Cleanup HTTP resources for socket
void socket_http_cleanup(int socket_fd);

/*
 * Mode Compatibility and Validation
 */

// Check if socket mode is compatible with HTTP
bool socket_http_validate_mode_compatibility(int socket_fd, socket_mode mode);

// Process HTTP-specific option changes
bool socket_http_process_option_change(int socket_fd, int option_id, const svalue_t* value);

/*
 * Socket Lifecycle Integration
 */

// Called when socket connects
void socket_http_on_connect(int socket_fd);

// Called when socket disconnects
void socket_http_on_disconnect(int socket_fd);

// Called when socket encounters error
void socket_http_on_error(int socket_fd, int error_code);

/*
 * Auto-Detection and Management
 */

// Check if socket should automatically enable HTTP processing
bool socket_should_enable_http_processing(int socket_fd);

// Auto-enable HTTP mode if socket configuration indicates it
void socket_http_auto_enable_if_needed(int socket_fd);

/*
 * Debug and Monitoring
 */

// Dump HTTP socket status to buffer
void socket_http_dump_status(outbuffer_t* buffer);

// Get count of active HTTP sockets
size_t socket_http_get_active_count();

// Check if socket is actively processing HTTP request
bool socket_http_is_processing_request(int socket_fd);

/*
 * Error Handling
 */

// Get last HTTP error for socket
const char* socket_http_get_last_error(int socket_fd);

// Clear HTTP error state for socket
void socket_http_clear_error(int socket_fd);

/*
 * HTTP Status Constants for Error Handling
 */
#ifndef HTTP_STATUS_REQUEST_TIMEOUT
#define HTTP_STATUS_REQUEST_TIMEOUT 408
#endif

/*
 * Integration Macros
 */

// Check if socket has HTTP processing enabled
#define SOCKET_HAS_HTTP_PROCESSING(fd) socket_is_http_mode(fd)

// Macro to safely call HTTP processing functions
#define SOCKET_HTTP_SAFE_CALL(fd, func, ...) \
    do { \
        if (socket_is_http_mode(fd)) { \
            func(fd, ##__VA_ARGS__); \
        } \
    } while(0)

/*
 * Forward declarations for helper functions
 */
char* make_shared_string(const char* str);
void call_function_pointer(function_t* fp, int argc, svalue_t* argv);
void apply(const char* function, object_t* ob, int argc, svalue_t* argv);

/*
 * HTTP Integration Registry
 * 
 * External access to the HTTP handlers registry for socket system
 */
extern std::unordered_map<int, std::unique_ptr<HTTPHandler>> http_handlers_;

#endif  // SOCKET_HTTP_INTEGRATION_H_