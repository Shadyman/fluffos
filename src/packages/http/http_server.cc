#include "http.h"
#include "http_parser.h"
#include "packages/sockets/socket_option_manager.h"
#include "packages/sockets/socket_error_handler.h"
#include <algorithm>
#include <sstream>
#include <cctype>
#include <ctime>
#include <iomanip>

// Static HTTP handler registry for socket integration
static std::unordered_map<int, std::unique_ptr<HTTPHandler>> http_handlers_;

/*
 * HTTPHandler Implementation
 */

HTTPHandler::HTTPHandler(int socket_id) 
    : socket_id_(socket_id) {
    
    // Initialize option manager with socket ID
    option_manager_ = std::make_unique<SocketOptionManager>(socket_id);
    
    // Initialize connection state
    connection_ = std::make_unique<HTTPConnection>(socket_id);
    
    // Set default HTTP options
    svalue_t default_val;
    
    // Set HTTP/1.1 as default version
    default_val.type = T_STRING;
    default_val.u.string = "HTTP/1.1";
    option_manager_->set_option(SO_PROTOCOL_VERSION, &default_val);
    
    // Set default User-Agent
    default_val.u.string = DEFAULT_HTTP_USER_AGENT;
    option_manager_->set_option(SO_HTTP_USER_AGENT, &default_val);
    
    // Set default timeout
    default_val.type = T_NUMBER;
    default_val.u.number = DEFAULT_HTTP_TIMEOUT;
    option_manager_->set_option(SO_HTTP_TIMEOUT, &default_val);
}

HTTPHandler::~HTTPHandler() {
    // Cleanup is handled by unique_ptr destructors
}

bool HTTPHandler::process_incoming_data(const char* data, size_t length) {
    if (!data || length == 0) {
        last_error_ = "Invalid input data";
        return false;
    }
    
    // Append new data to buffer
    connection_->buffer.append(data, length);
    
    // Check buffer size limits
    if (connection_->buffer.size() > MAX_HEADER_SIZE && connection_->parsing_headers) {
        last_error_ = "HTTP headers too large";
        return false;
    }
    
    // If we're still parsing headers
    if (connection_->parsing_headers) {
        return parse_headers();
    }
    
    // If we're reading body content
    return parse_body();
}

bool HTTPHandler::parse_headers() {
    size_t header_end = connection_->buffer.find("\r\n\r\n");
    if (header_end == std::string::npos) {
        // Headers not complete yet
        return true;
    }
    
    // Extract headers section
    std::string headers_section = connection_->buffer.substr(0, header_end);
    std::istringstream header_stream(headers_section);
    std::string line;
    bool first_line = true;
    
    // Parse each header line
    while (std::getline(header_stream, line)) {
        // Remove \r if present
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        
        if (first_line) {
            if (!parse_request_line(line, &connection_->current_request)) {
                last_error_ = "Invalid HTTP request line";
                return false;
            }
            first_line = false;
        } else if (!line.empty()) {
            if (!parse_header_line(line, &connection_->current_request)) {
                last_error_ = "Invalid HTTP header line: " + line;
                return false;
            }
        }
    }
    
    // Remove headers from buffer
    connection_->buffer.erase(0, header_end + 4);
    connection_->parsing_headers = false;
    
    // Process connection-specific headers
    process_connection_headers(&connection_->current_request);
    
    // Apply socket options to request
    apply_socket_options_to_request(&connection_->current_request);
    
    // Check if we need to read body
    if (connection_->current_request.content_length > 0) {
        if (connection_->current_request.content_length > MAX_BODY_SIZE) {
            last_error_ = "HTTP body too large";
            return false;
        }
        connection_->bytes_needed = connection_->current_request.content_length;
        return parse_body();
    }
    
    // Request is complete
    connection_->current_request.is_complete = true;
    return true;
}

bool HTTPHandler::parse_body() {
    if (connection_->bytes_needed == 0) {
        connection_->current_request.is_complete = true;
        return true;
    }
    
    if (connection_->buffer.size() >= connection_->bytes_needed) {
        // Extract body content
        connection_->current_request.body = connection_->buffer.substr(0, connection_->bytes_needed);
        connection_->buffer.erase(0, connection_->bytes_needed);
        connection_->bytes_needed = 0;
        connection_->current_request.is_complete = true;
        return true;
    }
    
    // Still need more data
    return true;
}

bool HTTPHandler::parse_request_line(const std::string& line, HTTPRequest* request) {
    std::istringstream iss(line);
    std::string method_str, uri, version_str;
    
    if (!(iss >> method_str >> uri >> version_str)) {
        return false;
    }
    
    // Parse method
    request->method = string_to_method(method_str);
    if (request->method == HTTP_UNKNOWN) {
        return false;
    }
    
    // Parse URI
    request->uri = uri;
    if (!parse_uri(uri, &request->path, &request->query_string)) {
        return false;
    }
    
    // Parse version
    request->version = string_to_version(version_str);
    if (request->version == HTTP_VERSION_UNKNOWN) {
        return false;
    }
    
    return true;
}

bool HTTPHandler::parse_header_line(const std::string& line, HTTPRequest* request) {
    size_t colon_pos = line.find(':');
    if (colon_pos == std::string::npos) {
        return false;
    }
    
    std::string name = line.substr(0, colon_pos);
    std::string value = line.substr(colon_pos + 1);
    
    // Trim whitespace
    name.erase(name.find_last_not_of(" \t") + 1);
    value.erase(0, value.find_first_not_of(" \t"));
    value.erase(value.find_last_not_of(" \t") + 1);
    
    // Validate header name and value
    if (!is_valid_header_name(name) || !is_valid_header_value(value)) {
        return false;
    }
    
    // Convert header name to lowercase for case-insensitive lookup
    std::transform(name.begin(), name.end(), name.begin(), ::tolower);
    
    // Store header
    request->headers[name] = value;
    
    // Process special headers
    if (name == "content-length") {
        request->content_length = static_cast<size_t>(std::stoul(value));
    } else if (name == "connection") {
        std::string conn_value = value;
        std::transform(conn_value.begin(), conn_value.end(), conn_value.begin(), ::tolower);
        request->keep_alive = (conn_value.find("keep-alive") != std::string::npos);
    }
    
    return true;
}

http_method HTTPHandler::string_to_method(const std::string& method_str) {
    if (method_str == "GET") return HTTP_GET;
    if (method_str == "POST") return HTTP_POST;
    if (method_str == "PUT") return HTTP_PUT;
    if (method_str == "DELETE") return HTTP_DELETE;
    if (method_str == "HEAD") return HTTP_HEAD;
    if (method_str == "OPTIONS") return HTTP_OPTIONS;
    if (method_str == "PATCH") return HTTP_PATCH;
    if (method_str == "TRACE") return HTTP_TRACE;
    if (method_str == "CONNECT") return HTTP_CONNECT;
    return HTTP_UNKNOWN;
}

http_version HTTPHandler::string_to_version(const std::string& version_str) {
    if (version_str == "HTTP/1.0") return HTTP_VERSION_1_0;
    if (version_str == "HTTP/1.1") return HTTP_VERSION_1_1;
    if (version_str == "HTTP/2.0") return HTTP_VERSION_2_0;
    return HTTP_VERSION_UNKNOWN;
}

bool HTTPHandler::parse_uri(const std::string& uri, std::string* path, std::string* query) {
    if (uri.empty() || uri[0] != '/') {
        return false;
    }
    
    size_t query_pos = uri.find('?');
    if (query_pos == std::string::npos) {
        *path = decode_uri_component(uri);
        query->clear();
    } else {
        *path = decode_uri_component(uri.substr(0, query_pos));
        *query = uri.substr(query_pos + 1);
    }
    
    return true;
}

std::string HTTPHandler::decode_uri_component(const std::string& encoded) {
    std::string decoded;
    decoded.reserve(encoded.length());
    
    for (size_t i = 0; i < encoded.length(); ++i) {
        if (encoded[i] == '%' && i + 2 < encoded.length()) {
            // Decode percent-encoded character
            std::string hex = encoded.substr(i + 1, 2);
            char* end;
            long value = std::strtol(hex.c_str(), &end, 16);
            if (end == hex.c_str() + 2) {
                decoded += static_cast<char>(value);
                i += 2;
            } else {
                decoded += encoded[i];
            }
        } else if (encoded[i] == '+') {
            decoded += ' ';
        } else {
            decoded += encoded[i];
        }
    }
    
    return decoded;
}

std::string HTTPHandler::generate_response(http_status status, const std::string& body,
                                          const std::unordered_map<std::string, std::string>& headers) {
    HTTPResponse response;
    response.status = status;
    response.status_text = get_status_text(status);
    response.body = body;
    response.headers = headers;
    
    // Apply socket options to response
    apply_socket_options_to_response(&response);
    
    // Add default headers
    add_default_headers(&response);
    
    return format_response(response);
}

std::string HTTPHandler::format_response(const HTTPResponse& response) {
    std::ostringstream oss;
    
    // Status line
    oss << get_version_string(response.version) << " " 
        << static_cast<int>(response.status) << " " 
        << response.status_text << "\r\n";
    
    // Headers
    for (const auto& header : response.headers) {
        oss << header.first << ": " << header.second << "\r\n";
    }
    
    // Empty line separating headers from body
    oss << "\r\n";
    
    // Body
    if (!response.body.empty()) {
        oss << response.body;
    }
    
    return oss.str();
}

std::string HTTPHandler::get_status_text(http_status status) {
    switch (status) {
        case HTTP_STATUS_CONTINUE: return "Continue";
        case HTTP_STATUS_OK: return "OK";
        case HTTP_STATUS_CREATED: return "Created";
        case HTTP_STATUS_ACCEPTED: return "Accepted";
        case HTTP_STATUS_NO_CONTENT: return "No Content";
        case HTTP_STATUS_MOVED_PERMANENTLY: return "Moved Permanently";
        case HTTP_STATUS_FOUND: return "Found";
        case HTTP_STATUS_NOT_MODIFIED: return "Not Modified";
        case HTTP_STATUS_BAD_REQUEST: return "Bad Request";
        case HTTP_STATUS_UNAUTHORIZED: return "Unauthorized";
        case HTTP_STATUS_FORBIDDEN: return "Forbidden";
        case HTTP_STATUS_NOT_FOUND: return "Not Found";
        case HTTP_STATUS_METHOD_NOT_ALLOWED: return "Method Not Allowed";
        case HTTP_STATUS_CONFLICT: return "Conflict";
        case HTTP_STATUS_INTERNAL_SERVER_ERROR: return "Internal Server Error";
        case HTTP_STATUS_NOT_IMPLEMENTED: return "Not Implemented";
        case HTTP_STATUS_BAD_GATEWAY: return "Bad Gateway";
        case HTTP_STATUS_SERVICE_UNAVAILABLE: return "Service Unavailable";
        default: return "Unknown";
    }
}

void HTTPHandler::add_default_headers(HTTPResponse* response) {
    // Add Content-Length if not present
    if (response->headers.find("Content-Length") == response->headers.end()) {
        response->headers["Content-Length"] = std::to_string(response->body.length());
    }
    
    // Add Date header
    if (response->headers.find("Date") == response->headers.end()) {
        std::time_t now = std::time(nullptr);
        std::tm* gmt = std::gmtime(&now);
        std::ostringstream date_stream;
        date_stream << std::put_time(gmt, "%a, %d %b %Y %H:%M:%S GMT");
        response->headers["Date"] = date_stream.str();
    }
    
    // Add Server header if not present
    if (response->headers.find("Server") == response->headers.end()) {
        response->headers["Server"] = "FluffOS/3.0-HTTP";
    }
    
    // Add Connection header based on keep-alive setting
    if (response->headers.find("Connection") == response->headers.end()) {
        response->headers["Connection"] = response->keep_alive ? "keep-alive" : "close";
    }
}

void HTTPHandler::process_connection_headers(HTTPRequest* request) {
    auto it = request->headers.find("connection");
    if (it != request->headers.end()) {
        std::string conn_value = it->second;
        std::transform(conn_value.begin(), conn_value.end(), conn_value.begin(), ::tolower);
        request->keep_alive = (conn_value.find("keep-alive") != std::string::npos);
        connection_->keep_alive = request->keep_alive;
    } else {
        // Default keep-alive behavior based on HTTP version
        request->keep_alive = (request->version >= HTTP_VERSION_1_1);
        connection_->keep_alive = request->keep_alive;
    }
}

void HTTPHandler::apply_socket_options_to_request(HTTPRequest* request) {
    // Apply timeout from socket options
    svalue_t timeout_val;
    if (option_manager_->get_option(SO_HTTP_TIMEOUT, &timeout_val)) {
        // Timeout option retrieved - could be used for connection management
    }
    
    // Apply additional HTTP options as needed
    svalue_t method_val;
    if (option_manager_->get_option(SO_HTTP_METHOD, &method_val) && 
        method_val.type == T_STRING) {
        // Could validate or override method here if needed
    }
}

void HTTPHandler::apply_socket_options_to_response(HTTPResponse* response) {
    // Apply User-Agent header from options
    svalue_t user_agent_val;
    if (option_manager_->get_option(SO_HTTP_USER_AGENT, &user_agent_val) && 
        user_agent_val.type == T_STRING) {
        response->headers["Server"] = std::string(user_agent_val.u.string) + "-Server";
    }
    
    // Apply custom headers from socket options
    svalue_t headers_val;
    if (option_manager_->get_option(SO_HTTP_HEADERS, &headers_val) && 
        headers_val.type == T_MAPPING) {
        set_headers_from_mapping(response, headers_val.u.map);
    }
    
    // Set keep-alive based on connection state
    response->keep_alive = connection_->keep_alive;
}

std::string HTTPHandler::create_error_response(http_status status, const std::string& message) {
    std::string body;
    if (message.empty()) {
        body = "<html><head><title>" + std::to_string(static_cast<int>(status)) + " " + 
               get_status_text(status) + "</title></head><body><h1>" + 
               get_status_text(status) + "</h1></body></html>";
    } else {
        body = "<html><head><title>Error</title></head><body><h1>" + 
               get_status_text(status) + "</h1><p>" + html_escape(message) + "</p></body></html>";
    }
    
    std::unordered_map<std::string, std::string> headers;
    headers["Content-Type"] = "text/html; charset=utf-8";
    
    return generate_response(status, body, headers);
}

std::string HTTPHandler::create_success_response(const std::string& body, const std::string& content_type) {
    std::unordered_map<std::string, std::string> headers;
    headers["Content-Type"] = content_type + "; charset=utf-8";
    
    return generate_response(HTTP_STATUS_OK, body, headers);
}

std::string HTTPHandler::create_json_response(const std::string& json_body, http_status status) {
    std::unordered_map<std::string, std::string> headers;
    headers["Content-Type"] = "application/json; charset=utf-8";
    
    return generate_response(status, json_body, headers);
}

std::string HTTPHandler::create_redirect_response(const std::string& location, http_status status) {
    std::unordered_map<std::string, std::string> headers;
    headers["Location"] = location;
    
    return generate_response(status, "", headers);
}

bool HTTPHandler::is_request_complete() const {
    return connection_->current_request.is_complete;
}

const HTTPRequest& HTTPHandler::get_current_request() const {
    return connection_->current_request;
}

void HTTPHandler::reset_request_state() {
    connection_->current_request = HTTPRequest();
    connection_->parsing_headers = true;
    connection_->bytes_needed = 0;
    connection_->buffer.clear();
}

bool HTTPHandler::should_keep_alive() const {
    return connection_->keep_alive;
}

void HTTPHandler::close_connection() {
    connection_->keep_alive = false;
    reset_request_state();
}

size_t HTTPHandler::get_buffer_size() const {
    return connection_->buffer.size();
}

void HTTPHandler::clear_buffer() {
    connection_->buffer.clear();
}

// Option integration methods
bool HTTPHandler::set_http_option(int option, const svalue_t* value, object_t* caller) {
    // Handle special REST and HTTP option processing
    switch (option) {
        case REST_ADD_ROUTE:
            return process_rest_add_route_option(value, caller);
            
        case REST_OPENAPI_INFO:
            return process_rest_openapi_info_option(value, caller);
            
        case REST_JWT_SECRET:
            return process_rest_jwt_secret_option(value, caller);
            
        case SO_HTTP_HEADERS:
            return process_http_headers_option(value, caller);
            
        default:
            // Delegate to option manager for standard options
            return option_manager_->set_option(option, value, caller);
    }
}

bool HTTPHandler::get_http_option(int option, svalue_t* result, object_t* caller) {
    return option_manager_->get_option(option, result, caller);
}

mapping_t* HTTPHandler::get_all_http_options(object_t* caller) const {
    return option_manager_->get_all_options(caller);
}

void HTTPHandler::set_headers_from_mapping(HTTPResponse* response, const mapping_t* headers) {
    if (!headers) return;
    
    for (int i = 0; i < headers->table_size; i++) {
        for (mapping_node_t* node = headers->table[i]; node; node = node->next) {
            if (node->values[0].type == T_STRING && node->values[1].type == T_STRING) {
                std::string name = node->values[0].u.string;
                std::string value = node->values[1].u.string;
                response->headers[name] = value;
            }
        }
    }
}

mapping_t* HTTPHandler::get_request_headers() const {
    mapping_t* result = allocate_mapping(connection_->current_request.headers.size());
    if (!result) return nullptr;
    
    // Implementation would require proper FluffOS mapping construction
    // This is a simplified placeholder
    return result;
}

// Utility methods
const char* HTTPHandler::get_method_string(http_method method) const {
    switch (method) {
        case HTTP_GET: return "GET";
        case HTTP_POST: return "POST";
        case HTTP_PUT: return "PUT";
        case HTTP_DELETE: return "DELETE";
        case HTTP_HEAD: return "HEAD";
        case HTTP_OPTIONS: return "OPTIONS";
        case HTTP_PATCH: return "PATCH";
        case HTTP_TRACE: return "TRACE";
        case HTTP_CONNECT: return "CONNECT";
        default: return "UNKNOWN";
    }
}

const char* HTTPHandler::get_version_string(http_version version) const {
    switch (version) {
        case HTTP_VERSION_1_0: return "HTTP/1.0";
        case HTTP_VERSION_1_1: return "HTTP/1.1";
        case HTTP_VERSION_2_0: return "HTTP/2.0";
        default: return "HTTP/1.1";
    }
}

bool HTTPHandler::is_valid_method_for_request(http_method method) const {
    // All HTTP methods are valid for requests
    return method != HTTP_UNKNOWN;
}

size_t HTTPHandler::get_max_request_size() const {
    return MAX_HEADER_SIZE + MAX_BODY_SIZE;
}

// Static utility functions
std::string HTTPHandler::url_encode(const std::string& input) {
    std::ostringstream encoded;
    encoded.fill('0');
    encoded << std::hex;
    
    for (unsigned char c : input) {
        if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            encoded << c;
        } else {
            encoded << '%' << std::setw(2) << static_cast<int>(c);
        }
    }
    
    return encoded.str();
}

std::string HTTPHandler::url_decode(const std::string& input) {
    std::string decoded;
    decoded.reserve(input.length());
    
    for (size_t i = 0; i < input.length(); ++i) {
        if (input[i] == '%' && i + 2 < input.length()) {
            std::string hex = input.substr(i + 1, 2);
            char* end;
            long value = std::strtol(hex.c_str(), &end, 16);
            if (end == hex.c_str() + 2) {
                decoded += static_cast<char>(value);
                i += 2;
            } else {
                decoded += input[i];
            }
        } else if (input[i] == '+') {
            decoded += ' ';
        } else {
            decoded += input[i];
        }
    }
    
    return decoded;
}

std::string HTTPHandler::html_escape(const std::string& input) {
    std::string escaped;
    escaped.reserve(input.length() * 2);
    
    for (char c : input) {
        switch (c) {
            case '<': escaped += "&lt;"; break;
            case '>': escaped += "&gt;"; break;
            case '&': escaped += "&amp;"; break;
            case '"': escaped += "&quot;"; break;
            case '\'': escaped += "&#39;"; break;
            default: escaped += c; break;
        }
    }
    
    return escaped;
}

bool HTTPHandler::is_valid_header_name(const std::string& name) {
    if (name.empty()) return false;
    
    for (char c : name) {
        if (!std::isalnum(c) && c != '-' && c != '_') {
            return false;
        }
    }
    
    return true;
}

bool HTTPHandler::is_valid_header_value(const std::string& value) {
    // Basic validation - could be enhanced based on HTTP spec
    for (char c : value) {
        if (c < 0x20 && c != 0x09) {  // Allow printable chars and tab
            return false;
        }
    }
    
    return true;
}

std::string HTTPHandler::get_mime_type(const std::string& extension) {
    // Convert to lowercase for comparison
    std::string ext = extension;
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    
    if (ext == ".html" || ext == ".htm") return MIME_TYPE_TEXT_HTML;
    if (ext == ".txt") return MIME_TYPE_TEXT_PLAIN;
    if (ext == ".json") return MIME_TYPE_APPLICATION_JSON;
    if (ext == ".xml") return MIME_TYPE_APPLICATION_XML;
    if (ext == ".css") return MIME_TYPE_TEXT_CSS;
    if (ext == ".js") return MIME_TYPE_APPLICATION_JAVASCRIPT;
    if (ext == ".png") return MIME_TYPE_IMAGE_PNG;
    if (ext == ".jpg" || ext == ".jpeg") return MIME_TYPE_IMAGE_JPEG;
    
    return "application/octet-stream";  // Default binary type
}

std::string HTTPHandler::get_content_type_with_charset(const std::string& mime_type, 
                                                       const std::string& charset) {
    if (mime_type.find("text/") == 0 || mime_type.find("application/json") == 0 ||
        mime_type.find("application/xml") == 0) {
        return mime_type + "; charset=" + charset;
    }
    
    return mime_type;
}

/*
 * REST Option Processing Functions (Phase 2 Golf Implementation)
 */

bool HTTPHandler::process_rest_add_route_option(const svalue_t* value, object_t* caller) {
    if (!value || value->type != T_MAPPING) {
        return false;
    }
    
    mapping_t* route_config = value->u.map;
    
    // Extract route configuration from mapping
    // Expected keys: "method", "path", "handler", "validation" 
    svalue_t* method_val = find_string_in_mapping(route_config, "method");
    svalue_t* path_val = find_string_in_mapping(route_config, "path");
    svalue_t* handler_val = find_string_in_mapping(route_config, "handler");
    
    if (!method_val || method_val->type != T_STRING ||
        !path_val || path_val->type != T_STRING ||
        !handler_val || handler_val->type != T_STRING) {
        return false;
    }
    
    // Store in option manager for REST router processing
    return option_manager_->set_option(REST_ADD_ROUTE, value, caller);
}

bool HTTPHandler::process_rest_openapi_info_option(const svalue_t* value, object_t* caller) {
    if (!value || value->type != T_MAPPING) {
        return false;
    }
    
    mapping_t* api_info = value->u.map;
    
    // Expected keys: "title", "version", "description", "contact", "license"
    svalue_t* title_val = find_string_in_mapping(api_info, "title");
    svalue_t* version_val = find_string_in_mapping(api_info, "version");
    
    if (!title_val || title_val->type != T_STRING ||
        !version_val || version_val->type != T_STRING) {
        return false;
    }
    
    // Store in option manager for OpenAPI generator
    return option_manager_->set_option(REST_OPENAPI_INFO, value, caller);
}

bool HTTPHandler::process_rest_jwt_secret_option(const svalue_t* value, object_t* caller) {
    if (!value || value->type != T_STRING) {
        return false;
    }
    
    const char* secret = value->u.string;
    if (!secret || strlen(secret) < 32) {
        // JWT secrets should be at least 32 characters for security
        return false;
    }
    
    // Store in option manager for JWT authentication
    return option_manager_->set_option(REST_JWT_SECRET, value, caller);
}

bool HTTPHandler::process_http_headers_option(const svalue_t* value, object_t* caller) {
    if (!value || value->type != T_MAPPING) {
        return false;
    }
    
    mapping_t* headers = value->u.map;
    
    // Validate header names and values
    for (int i = 0; i < headers->table_size; i++) {
        for (mapping_node_t* node = headers->table[i]; node; node = node->next) {
            if (node->values[0].type != T_STRING || node->values[1].type != T_STRING) {
                return false;
            }
            
            std::string name = node->values[0].u.string;
            std::string value = node->values[1].u.string;
            
            if (!is_valid_header_name(name) || !is_valid_header_value(value)) {
                return false;
            }
        }
    }
    
    // Store in option manager for HTTP response generation
    return option_manager_->set_option(SO_HTTP_HEADERS, value, caller);
}

/*
 * Socket Integration Functions
 */

bool socket_enable_http_mode(int socket_id, const mapping_t* options) {
    try {
        auto handler = std::make_unique<HTTPHandler>(socket_id);
        
        // Apply options if provided
        if (options) {
            for (int i = 0; i < options->table_size; i++) {
                for (mapping_node_t* node = options->table[i]; node; node = node->next) {
                    if (node->values[0].type == T_NUMBER) {
                        int option = node->values[0].u.number;
                        handler->set_http_option(option, &node->values[1], nullptr);
                    }
                }
            }
        }
        
        http_handlers_[socket_id] = std::move(handler);
        return true;
    } catch (const std::exception& e) {
        return false;
    }
}

bool socket_is_http_mode(int socket_id) {
    return http_handlers_.find(socket_id) != http_handlers_.end();
}

int socket_process_http_data(int socket_id, const char* data, size_t length) {
    auto it = http_handlers_.find(socket_id);
    if (it == http_handlers_.end()) {
        return -1;  // Socket not in HTTP mode
    }
    
    if (!it->second->process_incoming_data(data, length)) {
        return -2;  // Processing error
    }
    
    return it->second->is_request_complete() ? 1 : 0;
}

char* socket_generate_http_response(int socket_id, int status, const char* body, 
                                        const mapping_t* headers) {
    auto it = http_handlers_.find(socket_id);
    if (it == http_handlers_.end()) {
        return nullptr;
    }
    
    std::unordered_map<std::string, std::string> header_map;
    if (headers) {
        for (int i = 0; i < headers->table_size; i++) {
            for (mapping_node_t* node = headers->table[i]; node; node = node->next) {
                if (node->values[0].type == T_STRING && node->values[1].type == T_STRING) {
                    header_map[node->values[0].u.string] = node->values[1].u.string;
                }
            }
        }
    }
    
    std::string response = it->second->generate_response(
        static_cast<http_status>(status), 
        body ? std::string(body) : std::string(), 
        header_map
    );
    
    // Allocate and copy the response string
    size_t len = response.length();
    char* result = static_cast<char*>(DMALLOC(len + 1, TAG_STRING, "socket_generate_http_response"));
    if (result) {
        strcpy(result, response.c_str());
    }
    return result;
}

HTTPHandler* get_http_handler(int socket_id) {
    auto it = http_handlers_.find(socket_id);
    return (it != http_handlers_.end()) ? it->second.get() : nullptr;
}

void HTTPHandler::dump_request_state(outbuffer_t* buffer) const {
    if (!buffer) return;
    
    outbuf_add(buffer, "HTTP Request State:\n");
    outbuf_addv(buffer, "  Method: %s\n", get_method_string(connection_->current_request.method));
    outbuf_addv(buffer, "  URI: %s\n", connection_->current_request.uri.c_str());
    outbuf_addv(buffer, "  Path: %s\n", connection_->current_request.path.c_str());
    outbuf_addv(buffer, "  Query: %s\n", connection_->current_request.query_string.c_str());
    outbuf_addv(buffer, "  Version: %s\n", get_version_string(connection_->current_request.version));
    outbuf_addv(buffer, "  Content Length: %zu\n", connection_->current_request.content_length);
    outbuf_addv(buffer, "  Keep Alive: %s\n", connection_->current_request.keep_alive ? "Yes" : "No");
    outbuf_addv(buffer, "  Complete: %s\n", connection_->current_request.is_complete ? "Yes" : "No");
    outbuf_addv(buffer, "  Headers (%zu):\n", connection_->current_request.headers.size());
    
    for (const auto& header : connection_->current_request.headers) {
        outbuf_addv(buffer, "    %s: %s\n", header.first.c_str(), header.second.c_str());
    }
}

void HTTPHandler::dump_connection_state(outbuffer_t* buffer) const {
    if (!buffer) return;
    
    outbuf_add(buffer, "HTTP Connection State:\n");
    outbuf_addv(buffer, "  Socket ID: %d\n", connection_->socket_id);
    outbuf_addv(buffer, "  Keep Alive: %s\n", connection_->keep_alive ? "Yes" : "No");
    outbuf_addv(buffer, "  Buffer Size: %zu\n", connection_->buffer.size());
    outbuf_addv(buffer, "  Parsing Headers: %s\n", connection_->parsing_headers ? "Yes" : "No");
    outbuf_addv(buffer, "  Bytes Needed: %zu\n", connection_->bytes_needed);
}

