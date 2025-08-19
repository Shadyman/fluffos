#include "socket_http_integration.h"
#include "http_handler.h"
#include "socket_efuns.h"
#include "socket_option_manager.h"

/*
 * Socket HTTP Integration - Connect HTTP handlers to socket system
 * 
 * This file provides integration between HTTP handlers and the FluffOS
 * socket system. It hooks into socket events and processes HTTP data
 * as it arrives, maintaining the unified socket architecture.
 */

// External socket functions from socket_efuns.cc
extern lpc_socket_t* lpc_socks_get(int fd);
extern int lpc_socks_num();

// HTTP integration state
static std::unordered_map<int, bool> http_enabled_sockets_;

/*
 * Socket event processing integration
 */

bool socket_http_process_read_data(int socket_fd, const char* data, size_t length) {
    // Check if socket has HTTP mode enabled
    if (!socket_is_http_mode(socket_fd)) {
        return false;  // Not an HTTP socket, let normal processing handle it
    }
    
    // Process HTTP data
    int result = socket_process_http_data(socket_fd, data, length);
    
    if (result < 0) {
        // HTTP processing error - generate error response
        HTTPHandler* handler = get_http_handler(socket_fd);
        if (handler) {
            std::string error_response = handler->create_error_response(
                HTTP_STATUS_BAD_REQUEST, 
                "Invalid HTTP request"
            );
            
            // Send error response
            svalue_t error_val;
            error_val.type = T_STRING;
            error_val.u.string = make_shared_string(error_response.c_str());
            socket_write(socket_fd, &error_val, nullptr);
            
            // Close connection if not keep-alive
            if (!handler->should_keep_alive()) {
                socket_close(socket_fd, 0);
            }
        }
        return true;  // We handled the error
    }
    
    if (result == 1) {
        // HTTP request is complete - trigger LPC callback
        return socket_http_trigger_request_callback(socket_fd);
    }
    
    return true;  // HTTP processing in progress
}

bool socket_http_trigger_request_callback(int socket_fd) {
    lpc_socket_t* socket = lpc_socks_get(socket_fd);
    if (!socket || !socket->owner_ob) {
        return false;
    }
    
    // Get HTTP handler
    HTTPHandler* handler = get_http_handler(socket_fd);
    if (!handler) {
        return false;
    }
    
    // Call the socket's read callback with HTTP request data
    if (socket->read_callback.str) {
        // Create svalue for the callback
        svalue_t callback_args[2];
        
        // Socket ID
        callback_args[0].type = T_NUMBER;
        callback_args[0].u.number = socket_fd;
        
        // HTTP request data (simplified - real implementation would
        // create proper LPC mapping)
        callback_args[1].type = T_STRING;
        callback_args[1].u.string = make_shared_string("HTTP_REQUEST");
        
        // Execute callback
        if (socket->read_callback.f) {
            // Function callback
            try {
                call_function_pointer(socket->read_callback.f, 2, callback_args);
            } catch (...) {
                // Handle callback errors
                std::string error_response = handler->create_error_response(
                    HTTP_STATUS_INTERNAL_SERVER_ERROR,
                    "Server callback error"
                );
                
                svalue_t error_val;
                error_val.type = T_STRING;
                error_val.u.string = make_shared_string(error_response.c_str());
                socket_write(socket_fd, &error_val, nullptr);
            }
        } else if (socket->read_callback.str) {
            // String callback
            try {
                apply(socket->read_callback.str, socket->owner_ob, 2, callback_args);
            } catch (...) {
                // Handle callback errors
                std::string error_response = handler->create_error_response(
                    HTTP_STATUS_INTERNAL_SERVER_ERROR,
                    "Server callback error"
                );
                
                svalue_t error_val;
                error_val.type = T_STRING;
                error_val.u.string = make_shared_string(error_response.c_str());
                socket_write(socket_fd, &error_val, nullptr);
            }
        }
    }
    
    return true;
}

bool socket_http_setup_options(int socket_fd, const mapping_t* options) {
    HTTPHandler* handler = get_http_handler(socket_fd);
    if (!handler) {
        return false;
    }
    
    if (!options) {
        return true;  // No options to set
    }
    
    // Process each option in the mapping
    for (int i = 0; i < options->table_size; i++) {
        for (mapping_node_t* node = options->table[i]; node; node = node->next) {
            if (node->values[0].type == T_NUMBER) {
                int option_id = node->values[0].u.number;
                
                // Set the option using the handler
                if (!handler->set_http_option(option_id, &node->values[1])) {
                    return false;  // Option setting failed
                }
            }
        }
    }
    
    return true;
}

void socket_http_cleanup(int socket_fd) {
    // Remove socket from HTTP handlers registry
    auto it = http_handlers_.find(socket_fd);
    if (it != http_handlers_.end()) {
        http_handlers_.erase(it);
    }
    
    // Remove from enabled sockets tracking
    http_enabled_sockets_.erase(socket_fd);
}

bool socket_http_validate_mode_compatibility(int socket_fd, socket_mode mode) {
    // HTTP is compatible with stream modes
    switch (mode) {
        case STREAM:
        case STREAM_BINARY:
        case STREAM_TLS:
        case STREAM_TLS_BINARY:
            return true;
        default:
            return false;  // HTTP not compatible with MUD or datagram modes
    }
}

/*
 * HTTP option processing integration
 */

bool socket_http_process_option_change(int socket_fd, int option_id, const svalue_t* value) {
    HTTPHandler* handler = get_http_handler(socket_fd);
    if (!handler) {
        return false;  // Not an HTTP socket
    }
    
    // Handle HTTP-specific option changes
    if (IS_HTTP_OPTION(option_id)) {
        return handler->set_http_option(option_id, value);
    }
    
    return false;  // Not an HTTP option
}

/*
 * Socket lifecycle integration
 */

void socket_http_on_connect(int socket_fd) {
    if (!socket_is_http_mode(socket_fd)) {
        return;
    }
    
    // HTTP sockets might need special setup on connect
    HTTPHandler* handler = get_http_handler(socket_fd);
    if (handler) {
        // Reset request state for new connection
        handler->reset_request_state();
    }
}

void socket_http_on_disconnect(int socket_fd) {
    if (!socket_is_http_mode(socket_fd)) {
        return;
    }
    
    // Cleanup HTTP handler
    socket_http_cleanup(socket_fd);
}

void socket_http_on_error(int socket_fd, int error_code) {
    if (!socket_is_http_mode(socket_fd)) {
        return;
    }
    
    HTTPHandler* handler = get_http_handler(socket_fd);
    if (!handler) {
        return;
    }
    
    // Generate appropriate HTTP error response
    http_status status;
    std::string message;
    
    switch (error_code) {
        case ECONNRESET:
            status = HTTP_STATUS_BAD_REQUEST;
            message = "Connection reset";
            break;
        case ETIMEDOUT:
            status = HTTP_STATUS_REQUEST_TIMEOUT;
            message = "Request timeout";
            break;
        case ENOSPC:
            status = HTTP_STATUS_INTERNAL_SERVER_ERROR;
            message = "Server storage error";
            break;
        default:
            status = HTTP_STATUS_INTERNAL_SERVER_ERROR;
            message = "Internal server error";
            break;
    }
    
    std::string error_response = handler->create_error_response(status, message);
    
    // Try to send error response if possible
    svalue_t error_val;
    error_val.type = T_STRING;
    error_val.u.string = make_shared_string(error_response.c_str());
    socket_write(socket_fd, &error_val, nullptr);
}

/*
 * HTTP-specific socket mode detection and management
 */

bool socket_should_enable_http_processing(int socket_fd) {
    lpc_socket_t* socket = lpc_socks_get(socket_fd);
    if (!socket) {
        return false;
    }
    
    // Check if socket has HTTP-compatible mode
    if (!socket_http_validate_mode_compatibility(socket_fd, socket->mode)) {
        return false;
    }
    
    // Check if HTTP options are set
    SocketOptionManager option_manager(socket_fd);
    svalue_t http_mode_val;
    if (option_manager.get_option(SO_HTTP_SERVER_MODE, &http_mode_val) &&
        http_mode_val.type == T_NUMBER && http_mode_val.u.number) {
        return true;
    }
    
    return false;
}

void socket_http_auto_enable_if_needed(int socket_fd) {
    if (socket_should_enable_http_processing(socket_fd)) {
        socket_enable_http_mode(socket_fd, nullptr);
    }
}

/*
 * Debug and monitoring functions
 */

void socket_http_dump_status(outbuffer_t* buffer) {
    if (!buffer) return;
    
    outbuf_add(buffer, "HTTP Socket Status:\n");
    outbuf_addv(buffer, "  Active HTTP sockets: %zu\n", http_handlers_.size());
    
    for (const auto& pair : http_handlers_) {
        outbuf_addv(buffer, "  Socket %d: HTTP mode active\n", pair.first);
        
        // Dump handler state
        pair.second->dump_connection_state(buffer);
        outbuf_add(buffer, "\n");
    }
}

size_t socket_http_get_active_count() {
    return http_handlers_.size();
}

bool socket_http_is_processing_request(int socket_fd) {
    HTTPHandler* handler = get_http_handler(socket_fd);
    if (!handler) {
        return false;
    }
    
    return !handler->is_request_complete() && handler->get_buffer_size() > 0;
}

/*
 * Utility functions
 */

const char* socket_http_get_last_error(int socket_fd) {
    HTTPHandler* handler = get_http_handler(socket_fd);
    if (!handler) {
        return "Socket not in HTTP mode";
    }
    
    return handler->get_last_error();
}

void socket_http_clear_error(int socket_fd) {
    HTTPHandler* handler = get_http_handler(socket_fd);
    if (handler) {
        handler->clear_error();
    }
}

// Helper functions (these would need proper FluffOS implementation)
char* make_shared_string(const char* str) {
    // Placeholder - would use FluffOS string allocation
    size_t len = strlen(str);
    char* result = static_cast<char*>(DMALLOC(len + 1, TAG_STRING, "http_string"));
    if (result) {
        strcpy(result, str);
    }
    return result;
}

void call_function_pointer(function_t* fp, int argc, svalue_t* argv) {
    // Placeholder - would use FluffOS function call mechanism
}

void apply(const char* function, object_t* ob, int argc, svalue_t* argv) {
    // Placeholder - would use FluffOS apply mechanism
}