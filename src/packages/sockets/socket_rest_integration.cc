#include "socket_rest_integration.h"
#include "rest_handler.h"
#include "socket_option_manager.h"
#include "socket_error_handler.h"
#include <algorithm>
#include <sstream>

// External registry from rest_handler.cc
extern std::unordered_map<int, std::unique_ptr<RESTHandler>> rest_handlers_;

// REST mode tracking
static bool rest_mode_registered = false;
static int rest_socket_mode_number = REST_SERVER_MODE;

/*
 * REST Mode Registration
 */

bool register_rest_socket_mode() {
    if (rest_mode_registered) {
        return true;
    }
    
    // Register REST_SERVER mode with socket system
    // This would integrate with the socket mode registration system
    rest_mode_registered = true;
    
    return true;
}

bool is_rest_mode_available() {
    return rest_mode_registered;
}

int get_rest_socket_mode() {
    return rest_socket_mode_number;
}

/*
 * Socket Event Processing
 */

bool socket_rest_process_read_data(int socket_fd, const char* data, size_t length) {
    // First ensure HTTP processing succeeds
    if (!socket_http_process_read_data(socket_fd, data, length)) {
        return false;
    }
    
    // Get REST handler
    RESTHandler* handler = get_rest_handler(socket_fd);
    if (!handler) {
        return false;
    }
    
    // Process as REST request
    return handler->process_rest_request(data, length);
}

bool socket_rest_trigger_request_callback(int socket_fd) {
    RESTHandler* handler = get_rest_handler(socket_fd);
    if (!handler) {
        return false;
    }
    
    // Check if request is complete
    if (!handler->is_rest_request_complete()) {
        return true;  // Not ready yet, but not an error
    }
    
    // Here we would trigger the LPC callback for the matched route
    // This involves finding the route handler and calling the appropriate function
    
    return true;
}

bool socket_rest_setup_options(int socket_fd, const mapping_t* options) {
    RESTHandler* handler = get_rest_handler(socket_fd);
    if (!handler) {
        return false;
    }
    
    if (!options) {
        return true;  // No options to set
    }
    
    // Process REST-specific options from the mapping
    // This would iterate through the mapping and apply each option
    
    return true;
}

void socket_rest_cleanup(int socket_fd) {
    // Clean up REST handler
    auto it = rest_handlers_.find(socket_fd);
    if (it != rest_handlers_.end()) {
        rest_handlers_.erase(it);
    }
    
    // Clean up HTTP resources too
    socket_http_cleanup(socket_fd);
}

/*
 * Mode Compatibility and Validation
 */

bool socket_rest_validate_mode_compatibility(int socket_fd, socket_mode mode) {
    // REST mode is compatible with HTTP modes
    return (mode == REST_SERVER_MODE || 
            mode == REST_CLIENT_MODE ||
            socket_http_validate_mode_compatibility(socket_fd, mode));
}

bool socket_rest_process_option_change(int socket_fd, int option_id, const svalue_t* value) {
    RESTHandler* handler = get_rest_handler(socket_fd);
    if (!handler) {
        return false;
    }
    
    // Process REST-specific options
    switch (option_id) {
        case REST_ADD_ROUTE:
            return socket_rest_process_add_route_option(socket_fd, value);
            
        case REST_OPENAPI_INFO:
            return socket_rest_process_openapi_info_option(socket_fd, value);
            
        case REST_JWT_SECRET:
            return socket_rest_process_jwt_secret_option(socket_fd, value);
            
        case REST_DOCS_PATH:
            return socket_rest_process_docs_path_option(socket_fd, value);
            
        case REST_CORS_CONFIG:
            return socket_rest_process_cors_config_option(socket_fd, value);
            
        case REST_VALIDATION_SCHEMA:
            return socket_rest_process_validation_schema_option(socket_fd, value);
            
        default:
            // Not a REST option, let HTTP handler process it
            return socket_http_process_option_change(socket_fd, option_id, value);
    }
}

bool socket_rest_validate_route_config(const mapping_t* route_config) {
    if (!route_config) {
        return false;
    }
    
    // Check for required fields in the route configuration
    // This would validate the mapping structure
    
    return true;
}

/*
 * Socket Lifecycle Integration
 */

void socket_rest_on_connect(int socket_fd) {
    // Initialize REST processing for the socket
    socket_rest_init_json_processing(socket_fd);
    
    // Call HTTP connect handler too
    socket_http_on_connect(socket_fd);
}

void socket_rest_on_disconnect(int socket_fd) {
    // Clean up REST resources
    socket_rest_cleanup(socket_fd);
    
    // Call HTTP disconnect handler
    socket_http_on_disconnect(socket_fd);
}

void socket_rest_on_error(int socket_fd, int error_code) {
    // Handle REST-specific errors
    socket_rest_handle_error(socket_fd, error_code, "Socket error occurred");
    
    // Call HTTP error handler
    socket_http_on_error(socket_fd, error_code);
}

/*
 * Request Processing Integration
 */

bool socket_rest_handle_request(int socket_fd, const RestRequest& request) {
    RESTHandler* handler = get_rest_handler(socket_fd);
    if (!handler) {
        return false;
    }
    
    // Route the request to the appropriate handler
    return socket_rest_route_request(socket_fd, request);
}

bool socket_rest_route_request(int socket_fd, const RestRequest& request) {
    RESTHandler* handler = get_rest_handler(socket_fd);
    if (!handler) {
        return false;
    }
    
    // Here we would:
    // 1. Find the matching route
    // 2. Extract parameters
    // 3. Call the LPC handler function
    // 4. Process the response
    
    return true;
}

bool socket_rest_send_response(int socket_fd, const mapping_t* response_data, int status_code) {
    RESTHandler* handler = get_rest_handler(socket_fd);
    if (!handler) {
        return false;
    }
    
    // Generate JSON response
    std::string json_response = handler->create_json_success_response(response_data, 
                                                                     static_cast<http_status>(status_code));
    
    // Send through socket system
    // This would integrate with the actual socket writing mechanism
    
    return true;
}

/*
 * JSON Processing Integration
 */

bool socket_rest_init_json_processing(int socket_fd) {
    RESTHandler* handler = get_rest_handler(socket_fd);
    if (!handler) {
        return false;
    }
    
    // Initialize JSON processing capabilities
    // Set validation level, size limits, etc.
    
    return true;
}

bool socket_rest_process_json_body(int socket_fd, const std::string& json_data) {
    RESTHandler* handler = get_rest_handler(socket_fd);
    if (!handler) {
        return false;
    }
    
    // Parse and validate JSON
    mapping_t* json_mapping = handler->parse_json_to_mapping(json_data);
    
    return (json_mapping != nullptr);
}

std::string socket_rest_generate_json_response(const mapping_t* data, int status_code) {
    // Use any available REST handler for JSON generation
    if (!rest_handlers_.empty()) {
        RESTHandler* handler = rest_handlers_.begin()->second.get();
        return handler->create_json_success_response(data, static_cast<http_status>(status_code));
    }
    
    return "{}";
}

/*
 * Auto-Detection and Configuration
 */

bool socket_should_enable_rest_processing(int socket_fd) {
    // Check if socket mode is REST or if REST routes are configured
    // This would check socket options and configuration
    
    return false;  // Placeholder
}

void socket_rest_auto_enable_if_needed(int socket_fd) {
    if (socket_should_enable_rest_processing(socket_fd)) {
        socket_enable_rest_mode(socket_fd);
    }
}

bool socket_rest_detect_rest_request(const char* data, size_t length) {
    if (!data || length == 0) {
        return false;
    }
    
    // Simple detection - look for REST patterns in the request
    std::string request_data(data, std::min(length, size_t(1024)));
    
    // Check for JSON content type or REST API patterns
    return (request_data.find("application/json") != std::string::npos ||
            request_data.find("/api/") != std::string::npos);
}

/*
 * CORS Integration
 */

bool socket_rest_handle_cors_preflight(int socket_fd, const HTTPRequest& request) {
    RESTHandler* handler = get_rest_handler(socket_fd);
    if (!handler) {
        return false;
    }
    
    if (!handler->is_cors_enabled()) {
        return false;
    }
    
    // Handle OPTIONS request for CORS preflight
    if (request.method == HTTP_OPTIONS) {
        std::string cors_response = handler->handle_cors_preflight(request);
        // Send CORS preflight response
        return true;
    }
    
    return false;
}

void socket_rest_add_cors_headers(int socket_fd, HTTPResponse* response) {
    RESTHandler* handler = get_rest_handler(socket_fd);
    if (!handler || !handler->is_cors_enabled()) {
        return;
    }
    
    // Add CORS headers to response
    response->headers["Access-Control-Allow-Origin"] = "*";
    response->headers["Access-Control-Allow-Methods"] = "GET, POST, PUT, DELETE, OPTIONS";
    response->headers["Access-Control-Allow-Headers"] = "Content-Type, Authorization";
}

bool socket_rest_validate_cors_config(const mapping_t* cors_config) {
    if (!cors_config) {
        return true;  // No config is valid
    }
    
    // Validate CORS configuration mapping
    return true;  // Placeholder
}

/*
 * Debug and Monitoring
 */

void socket_rest_dump_status(outbuffer_t* buffer) {
    if (!buffer) return;
    
    outbuf_add(buffer, "REST Socket Status:\n");
    outbuf_addv(buffer, "Active REST sockets: %zu\n", rest_handlers_.size());
    outbuf_addv(buffer, "REST mode registered: %s\n", rest_mode_registered ? "Yes" : "No");
    
    for (const auto& handler_pair : rest_handlers_) {
        outbuf_addv(buffer, "Socket %d: REST mode active\n", handler_pair.first);
        handler_pair.second->dump_rest_state(buffer);
    }
}

size_t socket_rest_get_active_count() {
    return rest_handlers_.size();
}

bool socket_rest_is_processing_request(int socket_fd) {
    RESTHandler* handler = get_rest_handler(socket_fd);
    if (!handler) {
        return false;
    }
    
    return handler->is_rest_request_complete();
}

mapping_t* socket_rest_get_statistics() {
    // Create mapping with REST statistics
    // This would collect various metrics about REST processing
    
    return nullptr;  // Placeholder
}

void socket_rest_dump_routes(int socket_fd, outbuffer_t* buffer) {
    RESTHandler* handler = get_rest_handler(socket_fd);
    if (!handler || !buffer) {
        return;
    }
    
    handler->dump_routes(buffer);
}

/*
 * Error Handling
 */

const char* socket_rest_get_last_error(int socket_fd) {
    RESTHandler* handler = get_rest_handler(socket_fd);
    if (!handler) {
        return "REST mode not enabled";
    }
    
    return handler->get_last_error();
}

void socket_rest_clear_error(int socket_fd) {
    RESTHandler* handler = get_rest_handler(socket_fd);
    if (handler) {
        handler->clear_error();
    }
}

void socket_rest_set_error(int socket_fd, const std::string& error, const std::string& context) {
    // Set error state for REST processing
    // This would integrate with error handling system
}

bool socket_rest_handle_error(int socket_fd, int error_code, const std::string& message) {
    RESTHandler* handler = get_rest_handler(socket_fd);
    if (!handler) {
        return false;
    }
    
    // Generate error response
    std::string error_response = handler->create_json_error_response(
        static_cast<http_status>(error_code), message);
    
    // Send error response
    return true;
}

std::string socket_rest_generate_error_response(int error_code, const std::string& message,
                                               const mapping_t* details) {
    // Use any available REST handler for error response generation
    if (!rest_handlers_.empty()) {
        RESTHandler* handler = rest_handlers_.begin()->second.get();
        return handler->create_json_error_response(static_cast<http_status>(error_code), 
                                                  message, details);
    }
    
    // Fallback error response
    std::ostringstream json;
    json << "{\"error\": true, \"code\": " << error_code 
         << ", \"message\": \"" << message << "\"}";
    
    return json.str();
}

/*
 * Option Processing Functions
 */

bool socket_rest_process_add_route_option(int socket_fd, const svalue_t* value) {
    if (!value || value->type != T_MAPPING) {
        return false;
    }
    
    RESTHandler* handler = get_rest_handler(socket_fd);
    if (!handler) {
        return false;
    }
    
    return handler->register_route_from_mapping(value->u.map);
}

bool socket_rest_process_openapi_info_option(int socket_fd, const svalue_t* value) {
    if (!value || value->type != T_MAPPING) {
        return false;
    }
    
    RESTHandler* handler = get_rest_handler(socket_fd);
    if (!handler) {
        return false;
    }
    
    return handler->set_api_info_from_mapping(value->u.map);
}

bool socket_rest_process_jwt_secret_option(int socket_fd, const svalue_t* value) {
    if (!value || value->type != T_STRING) {
        return false;
    }
    
    // Process JWT secret configuration
    // This would set up JWT authentication
    
    return true;
}

bool socket_rest_process_docs_path_option(int socket_fd, const svalue_t* value) {
    if (!value || value->type != T_STRING) {
        return false;
    }
    
    // Set documentation path
    // This would configure where API docs are served
    
    return true;
}

bool socket_rest_process_cors_config_option(int socket_fd, const svalue_t* value) {
    if (!value || value->type != T_MAPPING) {
        return false;
    }
    
    RESTHandler* handler = get_rest_handler(socket_fd);
    if (!handler) {
        return false;
    }
    
    handler->enable_cors(value->u.map);
    return true;
}

bool socket_rest_process_validation_schema_option(int socket_fd, const svalue_t* value) {
    if (!value || value->type != T_MAPPING) {
        return false;
    }
    
    // Process validation schema configuration
    // This would set up request/response validation
    
    return true;
}

/*
 * Performance and Monitoring
 */

void socket_rest_start_request_timer(int socket_fd) {
    // Start timing for performance monitoring
}

void socket_rest_end_request_timer(int socket_fd, const std::string& route_pattern) {
    // End timing and record metrics
}

mapping_t* socket_rest_get_performance_metrics(int socket_fd) {
    // Return performance metrics mapping
    return nullptr;
}

/*
 * Configuration Management
 */

bool socket_rest_apply_config(int socket_fd, const mapping_t* config) {
    RESTHandler* handler = get_rest_handler(socket_fd);
    if (!handler || !config) {
        return false;
    }
    
    // Apply configuration from mapping
    return true;
}

mapping_t* socket_rest_get_config(int socket_fd) {
    RESTHandler* handler = get_rest_handler(socket_fd);
    if (!handler) {
        return nullptr;
    }
    
    return handler->get_all_rest_options();
}

bool socket_rest_validate_config(const mapping_t* config) {
    if (!config) {
        return false;
    }
    
    // Validate configuration mapping
    return true;
}

/*
 * HTTP Integration Bridge
 */

RestRequest* socket_rest_convert_http_request(const HTTPRequest& http_request) {
    // Convert HTTP request to REST request structure
    // This would create a RestRequest with proper field mapping
    
    return nullptr;  // Placeholder
}

HTTPResponse* socket_rest_convert_rest_response(const RestResponse& rest_response) {
    // Convert REST response to HTTP response structure
    // This would create an HTTPResponse with proper field mapping
    
    return nullptr;  // Placeholder
}

bool socket_rest_should_handle_http_request(const HTTPRequest& request) {
    // Check if HTTP request should be processed as REST
    return (request.path.find("/api/") == 0 ||
            request.headers.find("Content-Type") != request.headers.end() &&
            request.headers.at("Content-Type").find("application/json") != std::string::npos);
}