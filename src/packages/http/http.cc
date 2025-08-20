#include "base/package_api.h"
#include "http.h"
#include "http_parser.h"
#include "packages/sockets/socket_efuns.h"
#include "vm/internal/base/mapping.h"

/*
 * HTTP efunctions for LPC integration
 * 
 * These functions expose HTTP functionality to LPC code through the
 * unified socket architecture. They integrate with Phase 1's socket
 * system and provide HTTP-specific operations.
 */

#ifdef F_SOCKET_SET_HTTP_MODE
void f_socket_set_http_mode() {
    int socket_id;
    mapping_t* options = nullptr;
    int num_args = st_num_arg;
    
    if (num_args < 1 || num_args > 2) {
        bad_arg(num_args, F_SOCKET_SET_HTTP_MODE);
    }
    
    socket_id = (sp - num_args + 1)->u.number;
    if (num_args == 2) {
        if ((sp)->type != T_MAPPING) {
            bad_arg(2, F_SOCKET_SET_HTTP_MODE);
        }
        options = (sp)->u.map;
    }
    
    // Validate socket exists and caller has permission
    char addr[ADDR_BUF_SIZE];
    int port;
    get_socket_address(socket_id, addr, &port, 0);
    
    if (!check_valid_socket("set_http_mode", socket_id, current_object, addr, port)) {
        pop_n_elems(num_args - 1);
        sp->u.number = EESECURITY;
        return;
    }
    
    // Enable HTTP mode for socket
    bool success = socket_enable_http_mode(socket_id, options);
    
    pop_n_elems(num_args - 1);
    sp->u.number = success ? 1 : 0;
}
#endif

#ifdef F_SOCKET_HTTP_REQUEST  // Re-enabled - HTTPHandler implementation complete
void f_socket_http_request() {
    int socket_id;
    int num_args = st_num_arg;
    
    if (num_args != 1) {
        bad_arg(num_args, F_SOCKET_HTTP_REQUEST);
    }
    
    socket_id = sp->u.number;
    
    // Validate socket and get HTTP handler
    HTTPHandler* handler = get_http_handler(socket_id);
    if (!handler) {
        pop_stack();
        push_number(0);
        return;
    }
    
    // Check if request is complete
    if (!handler->is_request_complete()) {
        pop_stack();
        push_number(0);
        return;
    }
    
    // Get current request
    const HTTPRequest& request = handler->get_current_request();
    
    // Create LPC mapping with request data
    mapping_t* request_mapping = allocate_mapping(8);
    if (!request_mapping) {
        pop_stack();
        push_number(0);
        return;
    }
    
    // Add method
    add_mapping_string(request_mapping, "method", handler->get_method_string(request.method));
    
    // Add URI
    add_mapping_string(request_mapping, "uri", request.uri.c_str());
    
    // Add path
    add_mapping_string(request_mapping, "path", request.path.c_str());
    
    // Add query string
    add_mapping_string(request_mapping, "query", request.query_string.c_str());
    
    // Add version
    add_mapping_string(request_mapping, "version", handler->get_version_string(request.version));
    
    // Add content length
    add_mapping_pair(request_mapping, "content_length", static_cast<int>(request.content_length));
    
    // Add body
    add_mapping_string(request_mapping, "body", request.body.c_str());
    
    // Add headers as nested mapping (simplified for now)
    // TODO: Implement proper mapping nesting once insert_in_mapping is available
    if (!request.headers.empty()) {
        // For now, just add a simple string representation
        std::string headers_str = "";
        for (const auto& header : request.headers) {
            if (!headers_str.empty()) headers_str += ", ";
            headers_str += header.first + ":" + header.second;
        }
        add_mapping_string(request_mapping, "headers_string", headers_str.c_str());
    }
    
    pop_stack();
    push_mapping(request_mapping);
}
#endif

#ifdef F_SOCKET_HTTP_RESPONSE  // Re-enabled - HTTPHandler implementation complete
void f_socket_http_response() {
    int socket_id, status;
    char* body = nullptr;
    mapping_t* headers = nullptr;
    int num_args = st_num_arg;
    
    if (num_args < 2 || num_args > 4) {
        bad_arg(num_args, F_SOCKET_HTTP_RESPONSE);
    }
    
    socket_id = (sp - num_args + 1)->u.number;
    status = (sp - num_args + 2)->u.number;
    
    if (num_args >= 3) {
        if ((sp - num_args + 3)->type == T_STRING) {
            body = const_cast<char*>((sp - num_args + 3)->u.string);
        }
    }
    
    if (num_args == 4) {
        if ((sp)->type == T_MAPPING) {
            headers = (sp)->u.map;
        } else {
            bad_arg(4, F_SOCKET_HTTP_RESPONSE);
        }
    }
    
    // Generate HTTP response
    char* response = socket_generate_http_response(socket_id, status, body, headers);
    
    if (response) {
        // Write response to socket
        svalue_t write_val;
        write_val.type = T_STRING;
        write_val.u.string = response;
        
        int result = socket_write(socket_id, &write_val, nullptr);
        
        pop_n_elems(num_args - 1);
        sp->u.number = result;
    } else {
        pop_n_elems(num_args - 1);
        sp->u.number = -1;
    }
}
#endif

#ifdef F_SOCKET_HTTP_ERROR  // Re-enabled - HTTPHandler implementation complete
void f_socket_http_error() {
    int socket_id, status;
    char* message = nullptr;
    int num_args = st_num_arg;
    
    if (num_args < 2 || num_args > 3) {
        bad_arg(num_args, F_SOCKET_HTTP_ERROR);
    }
    
    socket_id = (sp - num_args + 1)->u.number;
    status = (sp - num_args + 2)->u.number;
    
    if (num_args == 3) {
        if ((sp)->type == T_STRING) {
            message = const_cast<char*>((sp)->u.string);
        } else {
            bad_arg(3, F_SOCKET_HTTP_ERROR);
        }
    }
    
    HTTPHandler* handler = get_http_handler(socket_id);
    if (!handler) {
        pop_n_elems(num_args - 1);
        sp->u.number = -1;
        return;
    }
    
    std::string error_response = handler->create_error_response(
        static_cast<http_status>(status), 
        message ? std::string(message) : std::string()
    );
    
    // Write error response to socket
    svalue_t write_val;
    write_val.type = T_STRING;
    write_val.u.string = make_shared_string(error_response.c_str());
    
    int result = socket_write(socket_id, &write_val, nullptr);
    
    pop_n_elems(num_args - 1);
    sp->u.number = result;
}
#endif

#ifdef F_SOCKET_HTTP_JSON  // Re-enabled - HTTPHandler implementation complete
void f_socket_http_json() {
    int socket_id;
    char* json_body;
    int status = HTTP_STATUS_OK;
    int num_args = st_num_arg;
    
    if (num_args < 2 || num_args > 3) {
        bad_arg(num_args, F_SOCKET_HTTP_JSON);
    }
    
    socket_id = (sp - num_args + 1)->u.number;
    
    if ((sp - num_args + 2)->type != T_STRING) {
        bad_arg(2, F_SOCKET_HTTP_JSON);
    }
    json_body = const_cast<char*>((sp - num_args + 2)->u.string);
    
    if (num_args == 3) {
        status = (sp)->u.number;
    }
    
    HTTPHandler* handler = get_http_handler(socket_id);
    if (!handler) {
        pop_n_elems(num_args - 1);
        sp->u.number = -1;
        return;
    }
    
    std::string json_response = handler->create_json_response(
        std::string(json_body), 
        static_cast<http_status>(status)
    );
    
    // Write JSON response to socket
    svalue_t write_val;
    write_val.type = T_STRING;
    write_val.u.string = make_shared_string(json_response.c_str());
    
    int result = socket_write(socket_id, &write_val, nullptr);
    
    pop_n_elems(num_args - 1);
    sp->u.number = result;
}
#endif

#ifdef F_SOCKET_HTTP_REDIRECT  // Re-enabled - HTTPHandler implementation complete
void f_socket_http_redirect() {
    int socket_id;
    char* location;
    int status = HTTP_STATUS_FOUND;
    int num_args = st_num_arg;
    
    if (num_args < 2 || num_args > 3) {
        bad_arg(num_args, F_SOCKET_HTTP_REDIRECT);
    }
    
    socket_id = (sp - num_args + 1)->u.number;
    
    if ((sp - num_args + 2)->type != T_STRING) {
        bad_arg(2, F_SOCKET_HTTP_REDIRECT);
    }
    location = const_cast<char*>((sp - num_args + 2)->u.string);
    
    if (num_args == 3) {
        status = (sp)->u.number;
    }
    
    HTTPHandler* handler = get_http_handler(socket_id);
    if (!handler) {
        pop_n_elems(num_args - 1);
        sp->u.number = -1;
        return;
    }
    
    std::string redirect_response = handler->create_redirect_response(
        std::string(location), 
        static_cast<http_status>(status)
    );
    
    // Write redirect response to socket
    svalue_t write_val;
    write_val.type = T_STRING;
    write_val.u.string = make_shared_string(redirect_response.c_str());
    
    int result = socket_write(socket_id, &write_val, nullptr);
    
    pop_n_elems(num_args - 1);
    sp->u.number = result;
}
#endif

#ifdef F_SOCKET_HTTP_HEADERS  // Re-enabled - HTTPHandler implementation complete
void f_socket_http_headers() {
    int socket_id;
    int num_args = st_num_arg;
    
    if (num_args != 1) {
        bad_arg(num_args, F_SOCKET_HTTP_HEADERS);
    }
    
    socket_id = sp->u.number;
    
    HTTPHandler* handler = get_http_handler(socket_id);
    if (!handler) {
        pop_stack();
        push_number(0);
        return;
    }
    
    mapping_t* headers = handler->get_request_headers();
    if (headers) {
        pop_stack();
        push_mapping(headers);
    } else {
        pop_stack();
        push_number(0);
    }
}
#endif

#ifdef F_SOCKET_IS_HTTP_MODE
void f_socket_is_http_mode() {
    int socket_id;
    
    if (st_num_arg != 1) {
        bad_arg(st_num_arg, F_SOCKET_IS_HTTP_MODE);
    }
    
    socket_id = sp->u.number;
    bool is_http = socket_is_http_mode(socket_id);
    
    pop_stack();
    push_number(is_http ? 1 : 0);
}
#endif

#ifdef F_SOCKET_HTTP_RESET  // Re-enabled - HTTPHandler implementation complete
void f_socket_http_reset() {
    int socket_id;
    
    if (st_num_arg != 1) {
        bad_arg(st_num_arg, F_SOCKET_HTTP_RESET);
    }
    
    socket_id = sp->u.number;
    
    HTTPHandler* handler = get_http_handler(socket_id);
    if (!handler) {
        pop_stack();
        push_number(0);
        return;
    }
    
    handler->reset_request_state();
    
    pop_stack();
    push_number(1);
}
#endif

#ifdef F_SOCKET_HTTP_KEEPALIVE  // Re-enabled - HTTPHandler implementation complete
void f_socket_http_keepalive() {
    int socket_id;
    
    if (st_num_arg != 1) {
        bad_arg(st_num_arg, F_SOCKET_HTTP_KEEPALIVE);
    }
    
    socket_id = sp->u.number;
    
    HTTPHandler* handler = get_http_handler(socket_id);
    if (!handler) {
        pop_stack();
        push_number(0);
        return;
    }
    
    bool keep_alive = handler->should_keep_alive();
    
    pop_stack();
    push_number(keep_alive ? 1 : 0);
}
#endif


// Temporarily disabled - conflicts with FluffOS built-in functions
/*
// Helper function to create shared strings (placeholder)
static char* make_shared_string(const char* str) {
    // This would use the actual FluffOS string allocation mechanism
    // For now, this is a simplified placeholder
    size_t len = strlen(str);
    char* result = static_cast<char*>(DMALLOC(len + 1, TAG_STRING, "make_shared_string"));
    if (result) {
        strcpy(result, str);
    }
    return result;
}

// Helper function to push mappings (placeholder)
static void push_mapping(mapping_t* map) {
    // This would use the actual FluffOS stack manipulation functions
    push_undefinedp();  // Placeholder
}
*/