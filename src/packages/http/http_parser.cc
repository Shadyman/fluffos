#include "http_parser.h"
#include <algorithm>
#include <sstream>
#include <cctype>
#include <cstring>

/*
 * HTTPParser Implementation
 * 
 * Core HTTP message parsing functionality extracted from original http_handler.cc
 * as part of architecture correction to proper package structure.
 */

HTTPParser::HTTPParser() {
    // Initialize parser state
    clear_error();
}

HTTPParser::~HTTPParser() {
    // Cleanup handled automatically
}

bool HTTPParser::parse_http_request(const char* data, size_t length, HTTPRequest* request) {
    if (!data || length == 0 || !request) {
        last_error_ = "Invalid input parameters";
        return false;
    }
    
    std::string input(data, length);
    std::istringstream stream(input);
    std::string line;
    bool headers_complete = false;
    
    // Parse request line (first line)
    if (!std::getline(stream, line)) {
        last_error_ = "Failed to read request line";
        return false;
    }
    
    // Remove carriage return if present
    if (!line.empty() && line.back() == '\r') {
        line.pop_back();
    }
    
    if (!parse_request_line(line, request)) {
        return false;
    }
    
    // Parse headers
    while (std::getline(stream, line)) {
        // Remove carriage return if present
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        
        // Empty line indicates end of headers
        if (line.empty()) {
            headers_complete = true;
            break;
        }
        
        if (!parse_header_line(line, request)) {
            return false;
        }
    }
    
    if (!headers_complete) {
        last_error_ = "Headers not complete";
        return false;
    }
    
    // Parse body if present
    std::string body;
    std::string body_line;
    while (std::getline(stream, body_line)) {
        body += body_line + "\n";
    }
    
    if (!body.empty()) {
        // Remove trailing newline
        if (body.back() == '\n') {
            body.pop_back();
        }
        request->body = body;
    }
    
    // Validate content length
    if (!validate_content_length(*request)) {
        return false;
    }
    
    // Process connection headers
    process_connection_headers(request);
    
    request->is_complete = true;
    return true;
}

bool HTTPParser::parse_request_line(const std::string& line, HTTPRequest* request) {
    std::istringstream iss(line);
    std::string method_str, uri_str, version_str;
    
    if (!(iss >> method_str >> uri_str >> version_str)) {
        last_error_ = "Invalid request line format";
        return false;
    }
    
    // Parse method
    request->method = string_to_method(method_str);
    if (request->method == HTTP_UNKNOWN) {
        last_error_ = "Unknown HTTP method: " + method_str;
        return false;
    }
    
    // Parse URI
    request->uri = uri_str;
    if (!parse_uri(uri_str, &request->path, &request->query_string)) {
        return false;
    }
    
    // Parse version
    request->version = string_to_version(version_str);
    if (request->version == HTTP_VERSION_UNKNOWN) {
        last_error_ = "Unknown HTTP version: " + version_str;
        return false;
    }
    
    return true;
}

bool HTTPParser::parse_header_line(const std::string& line, HTTPRequest* request) {
    size_t colon_pos = line.find(':');
    if (colon_pos == std::string::npos) {
        last_error_ = "Invalid header format: " + line;
        return false;
    }
    
    std::string name = line.substr(0, colon_pos);
    std::string value = line.substr(colon_pos + 1);
    
    // Trim whitespace
    name.erase(0, name.find_first_not_of(" \t"));
    name.erase(name.find_last_not_of(" \t") + 1);
    value.erase(0, value.find_first_not_of(" \t"));
    value.erase(value.find_last_not_of(" \t") + 1);
    
    if (!is_valid_header_name(name)) {
        last_error_ = "Invalid header name: " + name;
        return false;
    }
    
    if (!is_valid_header_value(value)) {
        last_error_ = "Invalid header value for " + name;
        return false;
    }
    
    // Normalize header name to lowercase for consistent lookup
    std::string normalized_name = normalize_header_name(name);
    request->headers[normalized_name] = value;
    
    return true;
}

http_method HTTPParser::string_to_method(const std::string& method_str) {
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

http_version HTTPParser::string_to_version(const std::string& version_str) {
    if (version_str == "HTTP/1.0") return HTTP_VERSION_1_0;
    if (version_str == "HTTP/1.1") return HTTP_VERSION_1_1;
    if (version_str == "HTTP/2.0") return HTTP_VERSION_2_0;
    return HTTP_VERSION_UNKNOWN;
}

bool HTTPParser::parse_uri(const std::string& uri, std::string* path, std::string* query) {
    if (uri.length() > MAX_HTTP_URI_LENGTH) {
        last_error_ = "URI too long";
        return false;
    }
    
    size_t query_pos = uri.find('?');
    if (query_pos != std::string::npos) {
        *path = uri.substr(0, query_pos);
        *query = uri.substr(query_pos + 1);
    } else {
        *path = uri;
        *query = "";
    }
    
    // URL decode path
    *path = decode_uri_component(*path);
    
    return true;
}

std::string HTTPParser::decode_uri_component(const std::string& encoded) {
    std::string decoded;
    for (size_t i = 0; i < encoded.length(); ++i) {
        if (encoded[i] == '%' && i + 2 < encoded.length()) {
            // Convert hex to char
            int hex_value;
            std::istringstream hex_stream(encoded.substr(i + 1, 2));
            if (hex_stream >> std::hex >> hex_value) {
                decoded += static_cast<char>(hex_value);
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

bool HTTPParser::validate_content_length(const HTTPRequest& request) {
    auto it = request.headers.find("content-length");
    if (it != request.headers.end()) {
        try {
            size_t content_length = std::stoull(it->second);
            if (content_length > MAX_BODY_SIZE) {
                last_error_ = "Content length exceeds maximum allowed size";
                return false;
            }
            if (content_length != request.body.length()) {
                last_error_ = "Content-Length header does not match body size";
                return false;
            }
        } catch (const std::exception& e) {
            last_error_ = "Invalid Content-Length header value";
            return false;
        }
    }
    return true;
}

void HTTPParser::process_connection_headers(HTTPRequest* request) {
    auto it = request.headers.find("connection");
    if (it != request.headers.end()) {
        std::string conn_value = it->second;
        std::transform(conn_value.begin(), conn_value.end(), conn_value.begin(), ::tolower);
        request->keep_alive = (conn_value == "keep-alive");
    } else {
        // Default based on HTTP version
        request->keep_alive = (request->version >= HTTP_VERSION_1_1);
    }
}

const char* HTTPParser::get_method_string(http_method method) const {
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

const char* HTTPParser::get_version_string(http_version version) const {
    switch (version) {
        case HTTP_VERSION_1_0: return "HTTP/1.0";
        case HTTP_VERSION_1_1: return "HTTP/1.1";
        case HTTP_VERSION_2_0: return "HTTP/2.0";
        default: return "HTTP/1.1";
    }
}

const char* HTTPParser::get_status_text(http_status status) const {
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

std::string HTTPParser::normalize_header_name(const std::string& name) {
    std::string normalized = name;
    std::transform(normalized.begin(), normalized.end(), normalized.begin(), ::tolower);
    return normalized;
}

bool HTTPParser::is_valid_header_name(const std::string& name) {
    if (name.empty() || name.length() > MAX_HTTP_HEADER_NAME_LENGTH) {
        return false;
    }
    
    for (char c : name) {
        if (!std::isalnum(c) && c != '-' && c != '_') {
            return false;
        }
    }
    
    return true;
}

bool HTTPParser::is_valid_header_value(const std::string& value) {
    if (value.length() > MAX_HTTP_HEADER_VALUE_LENGTH) {
        return false;
    }
    
    // Basic validation - no control characters except tab
    for (char c : value) {
        if (std::iscntrl(c) && c != '\t') {
            return false;
        }
    }
    
    return true;
}

bool HTTPParser::is_valid_method_for_request(http_method method) const {
    return method != HTTP_UNKNOWN;
}

size_t HTTPParser::get_max_request_size() const {
    return MAX_HEADER_SIZE + MAX_BODY_SIZE;
}

size_t HTTPParser::get_max_header_size() const {
    return MAX_HEADER_SIZE;
}

// Static utility implementations
std::string HTTPParser::url_encode(const std::string& input) {
    std::string encoded;
    for (char c : input) {
        if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            encoded += c;
        } else {
            std::ostringstream oss;
            oss << '%' << std::hex << std::uppercase << static_cast<unsigned char>(c);
            encoded += oss.str();
        }
    }
    return encoded;
}

std::string HTTPParser::url_decode(const std::string& input) {
    HTTPParser parser;  // Temporary instance for access to decode method
    return parser.decode_uri_component(input);
}

std::string HTTPParser::html_escape(const std::string& input) {
    std::string escaped;
    for (char c : input) {
        switch (c) {
            case '<': escaped += "&lt;"; break;
            case '>': escaped += "&gt;"; break;
            case '&': escaped += "&amp;"; break;
            case '"': escaped += "&quot;"; break;
            case '\'': escaped += "&#x27;"; break;
            default: escaped += c; break;
        }
    }
    return escaped;
}