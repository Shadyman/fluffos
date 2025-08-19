#ifndef SOCKET_REST_INTEGRATION_H_
#define SOCKET_REST_INTEGRATION_H_

#include "base/package_api.h"
#include "packages/sockets/socket_efuns.h"
#include "packages/sockets/rest_handler.h"
#include "packages/sockets/socket_http_integration.h"

/*
 * Socket REST Integration Header
 * 
 * This header defines the integration points between REST handlers
 * and the FluffOS socket system. It builds on the HTTP integration
 * to provide RESTful API capabilities with routing, JSON processing,
 * and advanced API features.
 */

/*
 * Socket Event Processing for REST
 */

// Process incoming data for REST sockets
bool socket_rest_process_read_data(int socket_fd, const char* data, size_t length);

// Trigger REST request callback when request is complete
bool socket_rest_trigger_request_callback(int socket_fd);

// Setup REST options for socket
bool socket_rest_setup_options(int socket_fd, const mapping_t* options);

// Cleanup REST resources for socket
void socket_rest_cleanup(int socket_fd);

/*
 * REST Mode Registration and Management
 */

// Register REST socket mode with the socket system
bool register_rest_socket_mode();

// Check if REST mode is available
bool is_rest_mode_available();

// Get REST socket mode number
int get_rest_socket_mode();

/*
 * Mode Compatibility and Validation
 */

// Check if socket mode is compatible with REST
bool socket_rest_validate_mode_compatibility(int socket_fd, socket_mode mode);

// Process REST-specific option changes
bool socket_rest_process_option_change(int socket_fd, int option_id, const svalue_t* value);

// Validate REST route configuration
bool socket_rest_validate_route_config(const mapping_t* route_config);

/*
 * Socket Lifecycle Integration
 */

// Called when socket connects in REST mode
void socket_rest_on_connect(int socket_fd);

// Called when socket disconnects from REST mode
void socket_rest_on_disconnect(int socket_fd);

// Called when REST socket encounters error
void socket_rest_on_error(int socket_fd, int error_code);

/*
 * Request Processing Integration
 */

// Process REST request and trigger LPC callback
bool socket_rest_handle_request(int socket_fd, const RestRequest& request);

// Generate and send REST response
bool socket_rest_send_response(int socket_fd, const mapping_t* response_data, 
                              int status_code = HTTP_STATUS_OK);

// Handle REST routing and endpoint matching
bool socket_rest_route_request(int socket_fd, const RestRequest& request);

/*
 * JSON Processing Integration
 */

// Initialize JSON processing for socket
bool socket_rest_init_json_processing(int socket_fd);

// Process JSON request body
bool socket_rest_process_json_body(int socket_fd, const std::string& json_data);

// Generate JSON response
std::string socket_rest_generate_json_response(const mapping_t* data, int status_code);

/*
 * Auto-Detection and Configuration
 */

// Check if socket should automatically enable REST processing
bool socket_should_enable_rest_processing(int socket_fd);

// Auto-enable REST mode if socket configuration indicates it
void socket_rest_auto_enable_if_needed(int socket_fd);

// Detect REST request from HTTP data
bool socket_rest_detect_rest_request(const char* data, size_t length);

/*
 * CORS Integration
 */

// Handle CORS preflight request
bool socket_rest_handle_cors_preflight(int socket_fd, const HTTPRequest& request);

// Add CORS headers to response
void socket_rest_add_cors_headers(int socket_fd, HTTPResponse* response);

// Validate CORS configuration
bool socket_rest_validate_cors_config(const mapping_t* cors_config);

/*
 * OpenAPI Documentation Integration
 */

// Generate OpenAPI specification for socket
std::string socket_rest_generate_openapi_spec(int socket_fd);

// Serve API documentation
bool socket_rest_serve_documentation(int socket_fd, const HTTPRequest& request);

// Update API documentation when routes change
void socket_rest_update_documentation(int socket_fd);

/*
 * Middleware Integration
 */

// Process middleware chain for request
bool socket_rest_process_middleware(int socket_fd, RestRequest* request);

// Apply response middleware
bool socket_rest_apply_response_middleware(int socket_fd, RestResponse* response);

// Register middleware function
bool socket_rest_register_middleware(int socket_fd, const std::string& name, 
                                    const std::string& function);

/*
 * Route Management Integration
 */

// Dynamic route registration from LPC
bool socket_rest_register_route_from_lpc(int socket_fd, const mapping_t* route_config);

// Remove route by ID or pattern
bool socket_rest_unregister_route(int socket_fd, int route_id);
bool socket_rest_unregister_route_by_pattern(int socket_fd, const std::string& method, 
                                            const std::string& pattern);

// Get route statistics
mapping_t* socket_rest_get_route_statistics(int socket_fd);

/*
 * Error Handling and Recovery
 */

// Handle REST-specific errors
bool socket_rest_handle_error(int socket_fd, int error_code, const std::string& message);

// Generate error response in REST format
std::string socket_rest_generate_error_response(int error_code, const std::string& message,
                                               const mapping_t* details = nullptr);

// Recover from REST processing errors
void socket_rest_recover_from_error(int socket_fd);

/*
 * Debug and Monitoring
 */

// Dump REST socket status to buffer
void socket_rest_dump_status(outbuffer_t* buffer);

// Get count of active REST sockets
size_t socket_rest_get_active_count();

// Check if socket is actively processing REST request
bool socket_rest_is_processing_request(int socket_fd);

// Get REST processing statistics
mapping_t* socket_rest_get_statistics();

// Dump route table for socket
void socket_rest_dump_routes(int socket_fd, outbuffer_t* buffer);

/*
 * Performance Monitoring
 */

// Start request timing
void socket_rest_start_request_timer(int socket_fd);

// End request timing and log metrics
void socket_rest_end_request_timer(int socket_fd, const std::string& route_pattern);

// Get performance metrics
mapping_t* socket_rest_get_performance_metrics(int socket_fd);

/*
 * Error Handling
 */

// Get last REST error for socket
const char* socket_rest_get_last_error(int socket_fd);

// Clear REST error state for socket
void socket_rest_clear_error(int socket_fd);

// Set REST error with context
void socket_rest_set_error(int socket_fd, const std::string& error, const std::string& context);

/*
 * Configuration Management
 */

// Apply REST configuration from mapping
bool socket_rest_apply_config(int socket_fd, const mapping_t* config);

// Get current REST configuration
mapping_t* socket_rest_get_config(int socket_fd);

// Validate REST configuration
bool socket_rest_validate_config(const mapping_t* config);

/*
 * HTTP Integration Bridge
 */

// Convert HTTP request to REST request
RestRequest* socket_rest_convert_http_request(const HTTPRequest& http_request);

// Convert REST response to HTTP response
HTTPResponse* socket_rest_convert_rest_response(const RestResponse& rest_response);

// Check if HTTP request should be handled as REST
bool socket_rest_should_handle_http_request(const HTTPRequest& request);

/*
 * Integration Macros
 */

// Check if socket has REST processing enabled
#define SOCKET_HAS_REST_PROCESSING(fd) socket_is_rest_mode(fd)

// Macro to safely call REST processing functions
#define SOCKET_REST_SAFE_CALL(fd, func, ...) \
    do { \
        if (socket_is_rest_mode(fd)) { \
            func(fd, ##__VA_ARGS__); \
        } \
    } while(0)

// Macro to check REST request validity
#define SOCKET_REST_REQUEST_VALID(request) \
    ((request) && (request)->http_request.is_complete)

// Macro to check if content type is JSON
#define SOCKET_REST_IS_JSON_REQUEST(request) \
    ((request) && (request)->content_type == REST_CONTENT_JSON)

/*
 * REST Status Constants
 */
#define REST_STATUS_INITIALIZED 1
#define REST_STATUS_PROCESSING 2
#define REST_STATUS_ERROR 3
#define REST_STATUS_COMPLETE 4

/*
 * REST Processing Flags
 */
#define REST_FLAG_CORS_ENABLED 0x01
#define REST_FLAG_AUTH_REQUIRED 0x02
#define REST_FLAG_DOCS_ENABLED 0x04
#define REST_FLAG_MIDDLEWARE_ENABLED 0x08
#define REST_FLAG_VALIDATION_STRICT 0x10

/*
 * Forward declarations for helper functions
 */
char* make_shared_string(const char* str);
void call_function_pointer(function_t* fp, int argc, svalue_t* argv);
void apply(const char* function, object_t* ob, int argc, svalue_t* argv);

/*
 * REST Integration Registry
 * 
 * External access to the REST handlers registry for socket system
 */
extern std::unordered_map<int, std::unique_ptr<RESTHandler>> rest_handlers_;

/*
 * REST Socket Mode Constants
 */
#ifndef REST_SERVER_MODE
#define REST_SERVER_MODE 14  // From socket_options.h
#endif

#ifndef REST_CLIENT_MODE  
#define REST_CLIENT_MODE 15  // From socket_options.h
#endif

/*
 * REST Option Processing
 */

// Process REST_ADD_ROUTE option
bool socket_rest_process_add_route_option(int socket_fd, const svalue_t* value);

// Process REST_OPENAPI_INFO option
bool socket_rest_process_openapi_info_option(int socket_fd, const svalue_t* value);

// Process REST_JWT_SECRET option
bool socket_rest_process_jwt_secret_option(int socket_fd, const svalue_t* value);

// Process REST_DOCS_PATH option
bool socket_rest_process_docs_path_option(int socket_fd, const svalue_t* value);

// Process REST_CORS_CONFIG option
bool socket_rest_process_cors_config_option(int socket_fd, const svalue_t* value);

// Process REST_VALIDATION_SCHEMA option
bool socket_rest_process_validation_schema_option(int socket_fd, const svalue_t* value);

#endif  // SOCKET_REST_INTEGRATION_H_