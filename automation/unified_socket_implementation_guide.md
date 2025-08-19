# Unified Socket Architecture Implementation Guide

**A comprehensive guide for implementing the Unified Socket Architecture in FluffOS**

## Overview

This document provides detailed implementation instructions for building the Unified Socket Architecture into FluffOS packages. The implementation involves a ground-up rewrite based on GitHub master, incorporating lessons learned from existing socket handler patterns while eliminating architectural inconsistencies.

## Project Setup

### 1. Create New Development Branch

```bash
# Clone fresh FluffOS from GitHub master
git clone https://github.com/fluffos/fluffos.git fluffos-socket-dev
cd fluffos-socket-dev

# Create development branch
git checkout -b unified-socket-architecture

# Preserve specific packages from current implementation
# Copy json package (keep existing implementation)
cp -r /home/mud/current/ds3.9/fluffos/src/packages/json ./src/packages/

# Copy db package changes (unpushed improvements)
cp -r /home/mud/current/ds3.9/fluffos/src/packages/db ./src/packages/

# Commit preserved packages
git add src/packages/json src/packages/db
git commit -m "Preserve json and db packages from current implementation"
```

### 2. Implementation Phases

The implementation is divided into distinct phases to ensure stability and testability:

1. **Phase 1**: Enhanced Socket Options System
2. **Phase 2**: HTTP/REST Integration 
3. **Phase 3**: WebSocket/MQTT Integration
4. **Phase 4**: External Process Integration
5. **Phase 5**: Advanced Features (Caching, Apache Integration)

## Phase 1: Enhanced Socket Options System

### 1.1 Socket Options Definition

**File**: `src/packages/sockets/socket_options.h`

```cpp
#ifndef SOCKET_OPTIONS_H_
#define SOCKET_OPTIONS_H_

// Existing socket options (0-99)
enum socket_options {
    SO_TLS_VERIFY_PEER = 0,
    SO_TLS_SNI_HOSTNAME = 1,
    
    // HTTP/REST options: 100-119
    SO_HTTP_HEADERS = 100,
    SO_HTTP_METHOD = 101,
    SO_HTTP_URL = 102,
    SO_HTTP_BODY = 103,
    SO_HTTP_TIMEOUT = 104,
    SO_HTTP_USER_AGENT = 105,
    SO_HTTP_FOLLOW_REDIRECTS = 106,
    SO_HTTP_MAX_REDIRECTS = 107,
    SO_HTTP_CONNECT_TIMEOUT = 108,
    SO_HTTP_READ_TIMEOUT = 109,
    
    // REST configuration: 110-119
    REST_ROUTER_CONFIG = 110,
    REST_ADD_ROUTE = 111,
    REST_OPENAPI_INFO = 112,
    REST_JWT_SECRET = 113,
    REST_DOCS_PATH = 114,
    REST_VALIDATION_SCHEMA = 115,
    REST_MIDDLEWARE = 116,
    REST_ERROR_HANDLER = 117,
    REST_CORS_CONFIG = 118,
    REST_RATE_LIMIT = 119,
    
    // WebSocket options: 120-129
    WS_PROTOCOL = 120,
    WS_EXTENSIONS = 121,
    WS_ORIGIN = 122,
    WS_MAX_MESSAGE_SIZE = 123,
    WS_PING_INTERVAL = 124,
    WS_PONG_TIMEOUT = 125,
    WS_COMPRESSION = 126,
    WS_SUBPROTOCOL = 127,
    
    // MQTT options: 130-139
    MQTT_BROKER = 130,
    MQTT_CLIENT_ID = 131,
    MQTT_USERNAME = 132,
    MQTT_PASSWORD = 133,
    MQTT_KEEP_ALIVE = 134,
    MQTT_QOS = 135,
    MQTT_RETAIN = 136,
    MQTT_CLEAN_SESSION = 137,
    MQTT_WILL_TOPIC = 138,
    MQTT_WILL_MESSAGE = 139,
    
    // External options: 140-149
    EXTERNAL_COMMAND = 140,
    EXTERNAL_ARGS = 141,
    EXTERNAL_ENV = 142,
    EXTERNAL_WATCH_PATH = 143,
    EXTERNAL_WORKING_DIR = 144,
    EXTERNAL_USER = 145,
    EXTERNAL_GROUP = 146,
    EXTERNAL_TIMEOUT = 147,
    EXTERNAL_BUFFER_SIZE = 148,
    EXTERNAL_ASYNC = 149,
    
    // Cache options: 200-219
    SO_CACHE_ENABLE = 200,
    SO_CACHE_TTL = 201,
    SO_CACHE_MAX_SIZE = 202,
    SO_CACHE_KEY_PATTERN = 203,
    SO_CACHE_HEADERS = 204,
    SO_CACHE_METHODS = 205,
    SO_CACHE_EXCLUDE_PATTERNS = 206,
    SO_CACHE_THREAD_POOL_SIZE = 207,
    SO_CACHE_INVALIDATE_PATTERN = 208,
    SO_CACHE_STATISTICS = 209,
    
    // Apache integration: 300-319
    SO_APACHE_FASTCGI_ENABLE = 300,
    SO_APACHE_FASTCGI_SOCKET = 301,
    SO_APACHE_PROXY_ENABLE = 302,
    SO_APACHE_PROXY_UPSTREAM = 303,
    SO_APACHE_STATIC_HANDOFF = 304,
    SO_APACHE_DYNAMIC_HANDOFF = 305,
    SO_APACHE_CGI_ENV_VARS = 306,
    SO_APACHE_AUTH_PASSTHROUGH = 307,
    SO_APACHE_SSL_TERMINATION = 308,
    SO_APACHE_LOAD_BALANCE = 309
};

#endif  // SOCKET_OPTIONS_H_
```

### 1.2 Socket Option Storage System

**File**: `src/packages/sockets/socket_option_manager.h`

```cpp
#ifndef SOCKET_OPTION_MANAGER_H_
#define SOCKET_OPTION_MANAGER_H_

#include "base/package_api.h"
#include "socket_options.h"
#include <unordered_map>
#include <memory>
#include <variant>

class SocketOptionManager {
public:
    using OptionValue = std::variant<int, double, std::string, mapping_t*, array_t*>;
    
private:
    // Per-socket option storage
    std::unordered_map<int, std::unordered_map<int, OptionValue>> socket_options_;
    
    // Option validation functions
    std::unordered_map<int, std::function<bool(const OptionValue&)>> validators_;
    
    // Option change notifications
    std::unordered_map<int, std::function<void(int, int, const OptionValue&)>> notifiers_;
    
public:
    static SocketOptionManager& instance();
    
    // Set socket option with validation
    bool set_option(int socket_fd, int option, const OptionValue& value);
    
    // Get socket option
    OptionValue get_option(int socket_fd, int option) const;
    
    // Check if option is set
    bool has_option(int socket_fd, int option) const;
    
    // Remove all options for socket (cleanup on close)
    void clear_socket_options(int socket_fd);
    
    // Register option validator
    void register_validator(int option, std::function<bool(const OptionValue&)> validator);
    
    // Register option change notifier
    void register_notifier(int option, std::function<void(int, int, const OptionValue&)> notifier);
    
private:
    SocketOptionManager() = default;
    void initialize_validators();
};

#endif  // SOCKET_OPTION_MANAGER_H_
```

**File**: `src/packages/sockets/socket_option_manager.cc`

```cpp
#include "socket_option_manager.h"
#include "base/internal/log.h"

SocketOptionManager& SocketOptionManager::instance() {
    static SocketOptionManager instance;
    static bool initialized = false;
    if (!initialized) {
        instance.initialize_validators();
        initialized = true;
    }
    return instance;
}

bool SocketOptionManager::set_option(int socket_fd, int option, const OptionValue& value) {
    // Validate option if validator exists
    auto validator_it = validators_.find(option);
    if (validator_it != validators_.end() && !validator_it->second(value)) {
        debug(sockets, "Invalid option value for option %d on socket %d", option, socket_fd);
        return false;
    }
    
    // Store option
    socket_options_[socket_fd][option] = value;
    
    // Notify if notifier exists
    auto notifier_it = notifiers_.find(option);
    if (notifier_it != notifiers_.end()) {
        notifier_it->second(socket_fd, option, value);
    }
    
    debug(sockets, "Set option %d on socket %d", option, socket_fd);
    return true;
}

SocketOptionManager::OptionValue SocketOptionManager::get_option(int socket_fd, int option) const {
    auto socket_it = socket_options_.find(socket_fd);
    if (socket_it != socket_options_.end()) {
        auto option_it = socket_it->second.find(option);
        if (option_it != socket_it->second.end()) {
            return option_it->second;
        }
    }
    
    // Return appropriate default value based on option type
    switch (option) {
        case SO_HTTP_TIMEOUT:
        case SO_HTTP_CONNECT_TIMEOUT:
        case SO_HTTP_READ_TIMEOUT:
            return 30000; // 30 seconds default
            
        case SO_HTTP_MAX_REDIRECTS:
            return 5;
            
        case SO_HTTP_FOLLOW_REDIRECTS:
        case SO_CACHE_ENABLE:
            return 0; // Disabled by default
            
        case SO_CACHE_TTL:
            return 300; // 5 minutes default
            
        case SO_WS_MAX_MESSAGE_SIZE:
            return 65536; // 64KB default
            
        case SO_MQTT_KEEP_ALIVE:
            return 60; // 60 seconds default
            
        default:
            return std::string(""); // Empty string for unknown options
    }
}

bool SocketOptionManager::has_option(int socket_fd, int option) const {
    auto socket_it = socket_options_.find(socket_fd);
    if (socket_it != socket_options_.end()) {
        return socket_it->second.find(option) != socket_it->second.end();
    }
    return false;
}

void SocketOptionManager::clear_socket_options(int socket_fd) {
    socket_options_.erase(socket_fd);
    debug(sockets, "Cleared all options for socket %d", socket_fd);
}

void SocketOptionManager::register_validator(int option, std::function<bool(const OptionValue&)> validator) {
    validators_[option] = validator;
}

void SocketOptionManager::register_notifier(int option, std::function<void(int, int, const OptionValue&)> notifier) {
    notifiers_[option] = notifier;
}

void SocketOptionManager::initialize_validators() {
    // HTTP timeout validation
    register_validator(SO_HTTP_TIMEOUT, [](const OptionValue& value) {
        if (std::holds_alternative<int>(value)) {
            int timeout = std::get<int>(value);
            return timeout >= 1000 && timeout <= 300000; // 1 second to 5 minutes
        }
        return false;
    });
    
    // HTTP max redirects validation
    register_validator(SO_HTTP_MAX_REDIRECTS, [](const OptionValue& value) {
        if (std::holds_alternative<int>(value)) {
            int max_redirects = std::get<int>(value);
            return max_redirects >= 0 && max_redirects <= 20;
        }
        return false;
    });
    
    // Cache TTL validation
    register_validator(SO_CACHE_TTL, [](const OptionValue& value) {
        if (std::holds_alternative<int>(value)) {
            int ttl = std::get<int>(value);
            return ttl >= 1 && ttl <= 86400; // 1 second to 24 hours
        }
        return false;
    });
    
    // MQTT keep alive validation
    register_validator(MQTT_KEEP_ALIVE, [](const OptionValue& value) {
        if (std::holds_alternative<int>(value)) {
            int keep_alive = std::get<int>(value);
            return keep_alive >= 10 && keep_alive <= 3600; // 10 seconds to 1 hour
        }
        return false;
    });
}
```

### 1.3 Enhanced Socket EFUNs

**File**: `src/packages/sockets/socket_efuns.cc` (modifications)

```cpp
// Add to existing socket_efuns.cc file

#include "socket_option_manager.h"

// Enhanced socket_set_option implementation
void f_socket_set_option(void) {
    svalue_t *value = sp;
    int option = (sp-1)->u.number;
    int socket_fd = (sp-2)->u.number;
    
    sp -= 3; // Clean up stack
    
    // Validate socket
    if (socket_fd < 0 || socket_fd >= lpc_socks.size() || 
        lpc_socks[socket_fd].fd == -1) {
        push_number(0); // Failure
        return;
    }
    
    // Convert LPC value to OptionValue
    SocketOptionManager::OptionValue opt_value;
    
    switch (value->type) {
        case T_NUMBER:
            opt_value = value->u.number;
            break;
        case T_REAL:
            opt_value = value->u.real;
            break;
        case T_STRING:
            opt_value = std::string(value->u.string);
            break;
        case T_MAPPING:
            opt_value = value->u.map;
            break;
        case T_ARRAY:
            opt_value = value->u.arr;
            break;
        default:
            push_number(0); // Unsupported type
            return;
    }
    
    // Set option through manager
    bool success = SocketOptionManager::instance().set_option(socket_fd, option, opt_value);
    
    push_number(success ? 1 : 0);
}

// Enhanced socket_get_option implementation
void f_socket_get_option(void) {
    int option = sp->u.number;
    int socket_fd = (sp-1)->u.number;
    
    sp -= 2; // Clean up stack
    
    // Validate socket
    if (socket_fd < 0 || socket_fd >= lpc_socks.size() || 
        lpc_socks[socket_fd].fd == -1) {
        push_number(0);
        return;
    }
    
    // Get option through manager
    auto opt_value = SocketOptionManager::instance().get_option(socket_fd, option);
    
    // Convert OptionValue back to LPC value
    std::visit([](const auto& value) {
        using T = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<T, int>) {
            push_number(value);
        } else if constexpr (std::is_same_v<T, double>) {
            push_real(value);
        } else if constexpr (std::is_same_v<T, std::string>) {
            push_constant_string(value.c_str());
        } else if constexpr (std::is_same_v<T, mapping_t*>) {
            push_refed_mapping(value);
        } else if constexpr (std::is_same_v<T, array_t*>) {
            push_refed_array(value);
        }
    }, opt_value);
}

// Cleanup options when socket closes
void socket_cleanup_options(int socket_fd) {
    SocketOptionManager::instance().clear_socket_options(socket_fd);
}
```

## Phase 2: HTTP/REST Integration

### 2.1 HTTP Package Structure

Create new HTTP package with clean architecture:

**Directory Structure**:
```
src/packages/http/
├── CMakeLists.txt
├── http.spec
├── http.h
├── http.cc
├── http_client.cc
├── http_server.cc
├── http_parser.cc
├── http_response.cc
└── rest/
    ├── rest_router.h
    ├── rest_router.cc
    ├── rest_validation.cc
    ├── rest_auth.cc
    └── openapi_generator.cc
```

**File**: `src/packages/http/http.spec`

```c
/*
 * HTTP package efuns for FluffOS
 * 
 * Unified HTTP/REST functionality through socket system
 */

// Core HTTP functions (deprecated - use socket_create instead)
// These are kept for backward compatibility only
int http_start_server(int, string | function, mapping);
int http_stop_server(int);
int http_send_response(int, mapping);
int http_send_request(string, mapping, string | function | void);

// Utility functions that don't require socket modes
string http_encode_url(string);
string http_decode_url(string);
mapping http_parse_headers(string);
string http_format_headers(mapping);
string http_date(int);
int http_parse_date(string);
```

**File**: `src/packages/http/http.h`

```cpp
#ifndef PACKAGES_HTTP_H_
#define PACKAGES_HTTP_H_

#include "base/package_api.h"
#include "packages/sockets/socket_efuns.h"
#include "packages/sockets/socket_option_manager.h"
#include <memory>
#include <string>
#include <map>
#include <vector>

// HTTP request/response structures
struct HttpRequest {
    std::string method;
    std::string uri;
    std::string version;
    std::map<std::string, std::string> headers;
    std::string body;
    std::string remote_addr;
    
    // Parsed components
    std::string path;
    std::string query_string;
    std::map<std::string, std::string> query_params;
};

struct HttpResponse {
    int status_code;
    std::string status_text;
    std::map<std::string, std::string> headers;
    std::string body;
    
    HttpResponse(int status = 200) : status_code(status) {
        status_text = get_status_text(status);
        headers["Content-Type"] = "text/html; charset=utf-8";
        headers["Server"] = "FluffOS/3.0";
    }
    
    static std::string get_status_text(int status);
};

// HTTP socket handler
class HttpSocketHandler {
public:
    static int create_handler(enum socket_mode mode, svalue_t *read_callback, svalue_t *close_callback);
    static int bind_handler(int socket_fd, int port, const char *addr);
    static int connect_handler(int socket_fd, const char *addr, svalue_t *read_callback, svalue_t *write_callback);
    static void cleanup_handler(int socket_fd);
    
private:
    static void handle_http_request(int socket_fd, const HttpRequest& request);
    static void send_http_response(int socket_fd, const HttpResponse& response);
    static HttpRequest parse_http_request(const std::string& raw_request);
    static std::string format_http_response(const HttpResponse& response);
};

// REST router integration
class RestRouter {
public:
    struct Route {
        std::string method;
        std::string pattern;
        std::regex compiled_pattern;
        std::vector<std::string> param_names;
        std::string handler_name;
        mapping_t *metadata;
        mapping_t *openapi_docs;
    };
    
    static RestRouter& get_instance(int socket_fd);
    
    bool add_route(const std::string& method, const std::string& pattern, 
                   const std::string& handler, mapping_t *docs = nullptr);
    
    std::pair<bool, mapping_t*> match_route(const HttpRequest& request);
    
    mapping_t* generate_openapi_spec(mapping_t *api_info);
    
private:
    std::vector<std::shared_ptr<Route>> routes_;
    mapping_t *openapi_info_;
    std::string docs_path_;
    
    static std::map<int, std::unique_ptr<RestRouter>> routers_;
};

// Registration functions
void init_http_socket_handlers();
void cleanup_http_socket_handlers();

#endif  // PACKAGES_HTTP_H_
```

### 2.2 HTTP Socket Handler Implementation

**File**: `src/packages/http/http.cc`

```cpp
#include "http.h"
#include "base/internal/log.h"
#include <sstream>
#include <regex>

// Static member initialization
std::map<int, std::unique_ptr<RestRouter>> RestRouter::routers_;

int HttpSocketHandler::create_handler(enum socket_mode mode, svalue_t *read_callback, svalue_t *close_callback) {
    debug(http, "Creating HTTP socket handler for mode %d", mode);
    
    // Create standard socket first
    int socket_fd = socket_create_standard(STREAM, read_callback, close_callback);
    if (socket_fd < 0) {
        return socket_fd;
    }
    
    // Initialize HTTP-specific data structures
    switch (mode) {
        case HTTP_SERVER:
        case HTTPS_SERVER:
            // Set up server-specific options
            SocketOptionManager::instance().set_option(socket_fd, SO_HTTP_SERVER_MODE, 1);
            break;
            
        case HTTP_CLIENT:  
        case HTTPS_CLIENT:
            // Set up client-specific options
            SocketOptionManager::instance().set_option(socket_fd, SO_HTTP_CLIENT_MODE, 1);
            break;
            
        case REST_SERVER:
        case REST_CLIENT:
            // Initialize REST router for this socket
            RestRouter::routers_[socket_fd] = std::make_unique<RestRouter>();
            SocketOptionManager::instance().set_option(socket_fd, REST_MODE, 1);
            break;
            
        default:
            debug(http, "Unknown HTTP socket mode: %d", mode);
            socket_close_standard(socket_fd);
            return -1;
    }
    
    debug(http, "HTTP socket handler created successfully: fd=%d, mode=%d", socket_fd, mode);
    return socket_fd;
}

void HttpSocketHandler::handle_http_request(int socket_fd, const HttpRequest& request) {
    debug(http, "Processing HTTP request: %s %s", request.method.c_str(), request.uri.c_str());
    
    // Check if this is a REST socket
    if (SocketOptionManager::instance().has_option(socket_fd, REST_MODE)) {
        auto& router = RestRouter::get_instance(socket_fd);
        auto [matched, route_data] = router.match_route(request);
        
        if (matched) {
            // Add route match data to request mapping
            mapping_t *lpc_request = create_lpc_request_mapping(request, route_data);
            
            // Call LPC callback with enhanced request data
            call_lpc_callback(socket_fd, "rest_request", lpc_request);
        } else {
            // No route matched - send 404
            HttpResponse response(404);
            response.body = "Not Found";
            send_http_response(socket_fd, response);
        }
    } else {
        // Standard HTTP processing
        mapping_t *lpc_request = create_lpc_request_mapping(request, nullptr);
        call_lpc_callback(socket_fd, "http_request", lpc_request);
    }
}

HttpRequest HttpSocketHandler::parse_http_request(const std::string& raw_request) {
    HttpRequest request;
    std::istringstream iss(raw_request);
    std::string line;
    
    // Parse request line
    if (std::getline(iss, line)) {
        std::istringstream request_line(line);
        request_line >> request.method >> request.uri >> request.version;
        
        // Parse URI components
        size_t query_pos = request.uri.find('?');
        if (query_pos != std::string::npos) {
            request.path = request.uri.substr(0, query_pos);
            request.query_string = request.uri.substr(query_pos + 1);
            parse_query_parameters(request.query_string, request.query_params);
        } else {
            request.path = request.uri;
        }
    }
    
    // Parse headers
    while (std::getline(iss, line) && !line.empty() && line != "\r") {
        if (line.back() == '\r') line.pop_back();
        
        size_t colon_pos = line.find(':');
        if (colon_pos != std::string::npos) {
            std::string name = line.substr(0, colon_pos);
            std::string value = line.substr(colon_pos + 1);
            
            // Trim whitespace
            name.erase(0, name.find_first_not_of(" \t"));
            name.erase(name.find_last_not_of(" \t") + 1);
            value.erase(0, value.find_first_not_of(" \t"));
            value.erase(value.find_last_not_of(" \t") + 1);
            
            request.headers[name] = value;
        }
    }
    
    // Parse body if present
    if (request.headers.find("Content-Length") != request.headers.end()) {
        int content_length = std::stoi(request.headers["Content-Length"]);
        request.body.resize(content_length);
        iss.read(&request.body[0], content_length);
    }
    
    return request;
}

std::string HttpSocketHandler::format_http_response(const HttpResponse& response) {
    std::ostringstream oss;
    
    // Status line
    oss << "HTTP/1.1 " << response.status_code << " " << response.status_text << "\r\n";
    
    // Headers
    for (const auto& [name, value] : response.headers) {
        oss << name << ": " << value << "\r\n";
    }
    
    // Content-Length if not already set
    if (response.headers.find("Content-Length") == response.headers.end()) {
        oss << "Content-Length: " << response.body.size() << "\r\n";
    }
    
    // End of headers
    oss << "\r\n";
    
    // Body
    oss << response.body;
    
    return oss.str();
}

void HttpSocketHandler::send_http_response(int socket_fd, const HttpResponse& response) {
    std::string response_str = format_http_response(response);
    socket_write_raw(socket_fd, response_str.c_str(), response_str.size());
    
    debug(http, "Sent HTTP response: %d %s (%zu bytes)", 
          response.status_code, response.status_text.c_str(), response_str.size());
}

std::string HttpResponse::get_status_text(int status) {
    static const std::map<int, std::string> status_texts = {
        {200, "OK"}, {201, "Created"}, {204, "No Content"},
        {400, "Bad Request"}, {401, "Unauthorized"}, {403, "Forbidden"}, 
        {404, "Not Found"}, {405, "Method Not Allowed"}, {500, "Internal Server Error"}
    };
    
    auto it = status_texts.find(status);
    return (it != status_texts.end()) ? it->second : "Unknown";
}

// Package initialization
void init_http_socket_handlers() {
    static bool initialized = false;
    if (initialized) return;
    
    debug(http, "Initializing HTTP socket handlers");
    
    // Register handlers for HTTP modes
    register_socket_create_handler(HTTP_SERVER, HttpSocketHandler::create_handler);
    register_socket_create_handler(HTTPS_SERVER, HttpSocketHandler::create_handler);
    register_socket_create_handler(HTTP_CLIENT, HttpSocketHandler::create_handler);
    register_socket_create_handler(HTTPS_CLIENT, HttpSocketHandler::create_handler);
    register_socket_create_handler(REST_SERVER, HttpSocketHandler::create_handler);
    register_socket_create_handler(REST_CLIENT, HttpSocketHandler::create_handler);
    
    initialized = true;
    debug(http, "HTTP socket handlers initialized successfully");
}

void cleanup_http_socket_handlers() {
    RestRouter::routers_.clear();
    debug(http, "HTTP socket handlers cleaned up");
}
```

### 2.3 REST Router Implementation

**File**: `src/packages/http/rest/rest_router.cc`

```cpp
#include "../http.h"
#include <algorithm>

RestRouter& RestRouter::get_instance(int socket_fd) {
    auto it = routers_.find(socket_fd);
    if (it != routers_.end()) {
        return *it->second;
    }
    
    // Create new router if it doesn't exist
    routers_[socket_fd] = std::make_unique<RestRouter>();
    return *routers_[socket_fd];
}

bool RestRouter::add_route(const std::string& method, const std::string& pattern, 
                          const std::string& handler, mapping_t *docs) {
    auto route = std::make_shared<Route>();
    route->method = method;
    route->pattern = pattern;
    route->handler_name = handler;
    route->metadata = nullptr;
    route->openapi_docs = docs;
    
    // Compile regex pattern for parameter extraction
    std::string regex_pattern = "^";
    std::string current_param;
    bool in_param = false;
    
    for (char c : pattern) {
        if (c == '{') {
            in_param = true;
            current_param.clear();
            regex_pattern += "([^/]+)"; // Match parameter value
        } else if (c == '}') {
            in_param = false;
            route->param_names.push_back(current_param);
        } else if (in_param) {
            current_param += c;
        } else {
            // Escape regex special characters
            if (c == '.' || c == '*' || c == '+' || c == '?' || c == '^' || 
                c == '$' || c == '(' || c == ')' || c == '[' || c == ']' ||
                c == '|' || c == '\\') {
                regex_pattern += '\\';
            }
            regex_pattern += c;
        }
    }
    regex_pattern += "$";
    
    try {
        route->compiled_pattern = std::regex(regex_pattern);
        routes_.push_back(route);
        
        debug(http, "Added REST route: %s %s -> %s", 
              method.c_str(), pattern.c_str(), handler.c_str());
        return true;
    } catch (const std::exception& e) {
        debug(http, "Failed to compile route pattern '%s': %s", pattern.c_str(), e.what());
        return false;
    }
}

std::pair<bool, mapping_t*> RestRouter::match_route(const HttpRequest& request) {
    for (const auto& route : routes_) {
        if (route->method != request.method) {
            continue;
        }
        
        std::smatch matches;
        if (std::regex_match(request.path, matches, route->compiled_pattern)) {
            // Create route data mapping
            mapping_t *route_data = allocate_mapping(5);
            
            // Add route information
            add_mapping_string(route_data, "handler", route->handler_name.c_str());
            add_mapping_string(route_data, "pattern", route->pattern.c_str());
            add_mapping_string(route_data, "method", route->method.c_str());
            
            // Extract path parameters
            mapping_t *path_params = allocate_mapping(route->param_names.size());
            for (size_t i = 0; i < route->param_names.size() && i + 1 < matches.size(); i++) {
                add_mapping_string(path_params, route->param_names[i].c_str(), 
                                 matches[i + 1].str().c_str());
            }
            add_mapping_mapping(route_data, "path_params", path_params);
            
            // Add OpenAPI documentation if present
            if (route->openapi_docs) {
                add_mapping_mapping(route_data, "openapi_docs", route->openapi_docs);
            }
            
            debug(http, "Matched route: %s %s -> %s", 
                  request.method.c_str(), request.path.c_str(), route->handler_name.c_str());
            
            return {true, route_data};
        }
    }
    
    debug(http, "No route matched for: %s %s", request.method.c_str(), request.path.c_str());
    return {false, nullptr};
}

mapping_t* RestRouter::generate_openapi_spec(mapping_t *api_info) {
    mapping_t *spec = allocate_mapping(10);
    
    // OpenAPI version
    add_mapping_string(spec, "openapi", "3.0.3");
    
    // API info
    if (api_info) {
        add_mapping_mapping(spec, "info", api_info);
    } else {
        mapping_t *default_info = allocate_mapping(3);
        add_mapping_string(default_info, "title", "FluffOS REST API");
        add_mapping_string(default_info, "version", "1.0.0");
        add_mapping_string(default_info, "description", "Generated REST API documentation");
        add_mapping_mapping(spec, "info", default_info);
    }
    
    // Generate paths from routes
    mapping_t *paths = allocate_mapping(routes_.size());
    
    for (const auto& route : routes_) {
        if (!route->openapi_docs) continue;
        
        // Convert route pattern to OpenAPI format
        std::string openapi_path = route->pattern;
        std::regex param_regex(R"(\{([^}]+)\})");
        openapi_path = std::regex_replace(openapi_path, param_regex, "{$1}");
        
        // Get or create path object
        mapping_t *path_obj = nullptr;
        svalue_t *existing = find_string_in_mapping(paths, openapi_path.c_str());
        if (existing && existing->type == T_MAPPING) {
            path_obj = existing->u.map;
        } else {
            path_obj = allocate_mapping(10);
            add_mapping_mapping(paths, openapi_path.c_str(), path_obj);
        }
        
        // Add method documentation
        std::string method_lower = route->method;
        std::transform(method_lower.begin(), method_lower.end(), method_lower.begin(), ::tolower);
        add_mapping_mapping(path_obj, method_lower.c_str(), route->openapi_docs);
    }
    
    add_mapping_mapping(spec, "paths", paths);
    
    debug(http, "Generated OpenAPI specification with %zu routes", routes_.size());
    return spec;
}
```

## Phase 3: Socket Option Integration with EFUNs

### 3.1 Option-Based Route Management

**File**: `src/packages/http/rest/rest_options.cc`

```cpp
#include "../http.h"
#include "packages/sockets/socket_option_manager.h"

// Register REST-specific option handlers
void init_rest_option_handlers() {
    auto& manager = SocketOptionManager::instance();
    
    // REST_ADD_ROUTE handler
    manager.register_notifier(REST_ADD_ROUTE, [](int socket_fd, int option, const auto& value) {
        if (!std::holds_alternative<mapping_t*>(value)) {
            debug(http, "Invalid value type for REST_ADD_ROUTE");
            return;
        }
        
        mapping_t *route_def = std::get<mapping_t*>(value);
        auto& router = RestRouter::get_instance(socket_fd);
        
        // Extract route definition from mapping
        svalue_t *method_val = find_string_in_mapping(route_def, "method");
        svalue_t *pattern_val = find_string_in_mapping(route_def, "pattern");
        svalue_t *handler_val = find_string_in_mapping(route_def, "handler");
        svalue_t *docs_val = find_string_in_mapping(route_def, "openapi");
        
        if (method_val && pattern_val && handler_val &&
            method_val->type == T_STRING && pattern_val->type == T_STRING && 
            handler_val->type == T_STRING) {
            
            mapping_t *docs = (docs_val && docs_val->type == T_MAPPING) ? docs_val->u.map : nullptr;
            
            router.add_route(method_val->u.string, pattern_val->u.string, 
                           handler_val->u.string, docs);
        }
    });
    
    // REST_OPENAPI_INFO handler
    manager.register_notifier(REST_OPENAPI_INFO, [](int socket_fd, int option, const auto& value) {
        if (std::holds_alternative<mapping_t*>(value)) {
            auto& router = RestRouter::get_instance(socket_fd);
            // Store OpenAPI info for later spec generation
            debug(http, "Set OpenAPI info for socket %d", socket_fd);
        }
    });
}
```

## Phase 4: Build System Integration

### 4.1 CMake Configuration

**File**: `src/packages/http/CMakeLists.txt`

```cmake
# HTTP Package CMakeLists.txt

set(HTTP_SOURCES
    http.cc
    http_client.cc
    http_server.cc
    http_parser.cc
    http_response.cc
    rest/rest_router.cc
    rest/rest_validation.cc
    rest/rest_auth.cc
    rest/rest_options.cc
    rest/openapi_generator.cc
)

set(HTTP_HEADERS
    http.h
    rest/rest_router.h
)

# Create HTTP package library
add_library(package_http ${HTTP_SOURCES} ${HTTP_HEADERS})

# Link dependencies
target_link_libraries(package_http 
    PRIVATE base_lib
    PRIVATE package_sockets
)

# Include directories
target_include_directories(package_http
    PRIVATE ${CMAKE_SOURCE_DIR}/src
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
)

# Compiler definitions
if(PACKAGE_HTTP)
    target_compile_definitions(package_http PRIVATE PACKAGE_HTTP=1)
endif()

# Add to main build
if(PACKAGE_HTTP)
    target_link_libraries(${DRIVER_BIN} package_http)
    message(STATUS "HTTP package enabled")
endif()
```

### 4.2 Package Specification

**File**: `src/packages/http/http.spec` (Complete)

```c
/*
 * HTTP package efuns for FluffOS
 * 
 * Core HTTP functionality - most features accessed via socket_create()
 * These efuns provide utility functions that don't require socket modes
 */

// Utility functions (always available)
string http_encode_url(string);
string http_decode_url(string); 
mapping http_parse_headers(string);
string http_format_headers(mapping);
string http_date(int);
int http_parse_date(string);
string http_status_text(int);

// Legacy compatibility functions (deprecated - use socket_create instead)
#ifdef LEGACY_HTTP_EFUNS
int http_start_server(int, string | function, mapping);
int http_stop_server(int);  
int http_send_response(int, mapping);
int http_send_request(string, mapping, string | function | void);
#endif
```

## Phase 5: Testing and Validation

### 5.1 Unit Test Framework

**File**: `src/packages/http/tests/http_test.cc`

```cpp
#include <gtest/gtest.h>
#include "../http.h"
#include "packages/sockets/socket_option_manager.h"

class HttpSocketTest : public ::testing::Test {
protected:
    void SetUp() override {
        init_http_socket_handlers();
    }
    
    void TearDown() override {
        cleanup_http_socket_handlers();
    }
};

TEST_F(HttpSocketTest, CreateHttpSocket) {
    // Mock LPC callbacks
    svalue_t read_cb = {T_STRING};
    read_cb.u.string = "http_request";
    svalue_t close_cb = {T_STRING};  
    close_cb.u.string = "http_close";
    
    int socket_fd = HttpSocketHandler::create_handler(HTTP_SERVER, &read_cb, &close_cb);
    EXPECT_GE(socket_fd, 0);
    
    // Verify socket options are set
    auto& manager = SocketOptionManager::instance();
    EXPECT_TRUE(manager.has_option(socket_fd, SO_HTTP_SERVER_MODE));
}

TEST_F(HttpSocketTest, RestRouteMatching) {
    svalue_t read_cb = {T_STRING};
    read_cb.u.string = "rest_request";
    svalue_t close_cb = {T_STRING};
    close_cb.u.string = "rest_close";
    
    int socket_fd = HttpSocketHandler::create_handler(REST_SERVER, &read_cb, &close_cb);
    EXPECT_GE(socket_fd, 0);
    
    auto& router = RestRouter::get_instance(socket_fd);
    
    // Add test route
    bool success = router.add_route("GET", "/users/{id}", "get_user");
    EXPECT_TRUE(success);
    
    // Test matching
    HttpRequest request;
    request.method = "GET";
    request.path = "/users/123";
    
    auto [matched, route_data] = router.match_route(request);
    EXPECT_TRUE(matched);
    EXPECT_NE(route_data, nullptr);
}

TEST_F(HttpSocketTest, SocketOptions) {
    svalue_t read_cb = {T_STRING};
    read_cb.u.string = "http_request";
    svalue_t close_cb = {T_STRING};
    close_cb.u.string = "http_close";
    
    int socket_fd = HttpSocketHandler::create_handler(HTTP_CLIENT, &read_cb, &close_cb);
    EXPECT_GE(socket_fd, 0);
    
    auto& manager = SocketOptionManager::instance();
    
    // Test setting and getting options
    bool success = manager.set_option(socket_fd, SO_HTTP_TIMEOUT, 30000);
    EXPECT_TRUE(success);
    
    auto value = manager.get_option(socket_fd, SO_HTTP_TIMEOUT);
    EXPECT_TRUE(std::holds_alternative<int>(value));
    EXPECT_EQ(std::get<int>(value), 30000);
}
```

### 5.2 Integration Test

**File**: `automation/tests/lpc/test_unified_sockets.c`

```lpc
// LPC integration test for unified socket architecture
#include <network.h>

void create() {
    write("Testing Unified Socket Architecture\n");
    write("==================================\n");
    
    test_http_server();
    test_rest_server();
    test_socket_options();
    
    write("\nAll tests completed.\n");
}

void test_http_server() {
    write("Testing HTTP server creation...\n");
    
    int http_socket = socket_create(HTTP_SERVER, "http_callback", "http_close");
    if (http_socket >= 0) {
        write("✓ HTTP server socket created: " + http_socket + "\n");
        
        // Test socket binding
        if (socket_bind(http_socket, 8080) == 0) {
            write("✓ HTTP server bound to port 8080\n");
            
            if (socket_listen(http_socket, "http_accept") == 0) {
                write("✓ HTTP server listening\n");
            }
        }
        
        socket_close(http_socket);
    } else {
        write("✗ Failed to create HTTP server socket\n");
    }
}

void test_rest_server() {
    write("Testing REST server with routes...\n");
    
    int rest_socket = socket_create(REST_SERVER, "rest_callback", "rest_close");
    if (rest_socket >= 0) {
        write("✓ REST server socket created: " + rest_socket + "\n");
        
        // Test adding routes via socket options
        mapping route_def = ([
            "method": "GET",
            "pattern": "/api/users/{id}",
            "handler": "get_user",
            "openapi": ([
                "summary": "Get user by ID",
                "parameters": ([
                    "id": (["type": "integer", "description": "User ID"])
                ])
            ])
        ]);
        
        if (socket_set_option(rest_socket, REST_ADD_ROUTE, route_def)) {
            write("✓ REST route added successfully\n");
        }
        
        // Test OpenAPI info
        mapping api_info = ([
            "title": "Test REST API",
            "version": "1.0.0",
            "description": "Testing REST API functionality"
        ]);
        
        if (socket_set_option(rest_socket, REST_OPENAPI_INFO, api_info)) {
            write("✓ OpenAPI info set successfully\n");
        }
        
        socket_close(rest_socket);
    } else {
        write("✗ Failed to create REST server socket\n");
    }
}

void test_socket_options() {
    write("Testing socket options system...\n");
    
    int http_socket = socket_create(HTTP_CLIENT, "client_callback", "client_close");
    if (http_socket >= 0) {
        // Test various socket options
        if (socket_set_option(http_socket, SO_HTTP_TIMEOUT, 30000)) {
            write("✓ HTTP timeout option set\n");
            
            int timeout = socket_get_option(http_socket, SO_HTTP_TIMEOUT);
            if (timeout == 30000) {
                write("✓ HTTP timeout option retrieved correctly\n");
            }
        }
        
        if (socket_set_option(http_socket, SO_HTTP_USER_AGENT, "FluffOS/3.0")) {
            write("✓ HTTP user agent option set\n");
            
            string agent = socket_get_option(http_socket, SO_HTTP_USER_AGENT);
            if (agent == "FluffOS/3.0") {
                write("✓ HTTP user agent option retrieved correctly\n");
            }
        }
        
        socket_close(http_socket);
    }
}

// Callback functions for testing
void http_callback(int socket, mapping request, string addr) {
    write("HTTP request received: " + request["method"] + " " + request["uri"] + "\n");
}

void rest_callback(int socket, mapping request, string addr) {
    write("REST request received: " + request["method"] + " " + request["path"] + "\n");
    if (request["path_params"]) {
        write("Path parameters: " + sprintf("%O", request["path_params"]) + "\n");
    }
}

void http_close(int socket) {
    write("HTTP socket closed: " + socket + "\n");
}

void rest_close(int socket) {
    write("REST socket closed: " + socket + "\n");
}

void client_callback(int socket, mapping response, string addr) {
    write("HTTP client response: " + response["status"] + "\n");
}

void client_close(int socket) {
    write("HTTP client socket closed: " + socket + "\n");
}
```

## Implementation Checklist

### Phase 1: Foundation ✓
- [ ] Create socket-development branch from GitHub master
- [ ] Preserve json and db packages
- [ ] Implement enhanced socket options system
- [ ] Add socket option manager with validation
- [ ] Update socket_set_option/socket_get_option efuns
- [ ] Create comprehensive unit tests

### Phase 2: HTTP Integration ✓  
- [ ] Create new HTTP package structure
- [ ] Implement HTTP socket handlers
- [ ] Add HTTP request/response parsing
- [ ] Create REST router with pattern matching
- [ ] Integrate socket options with REST routes
- [ ] Add OpenAPI specification generation

### Phase 3: Testing & Validation ✓
- [ ] Create unit test framework
- [ ] Add integration tests in LPC
- [ ] Performance benchmarking
- [ ] Memory leak testing
- [ ] Security validation

### Phase 4: Documentation ✓
- [ ] Update efun documentation
- [ ] Create migration guide
- [ ] Add example configurations
- [ ] Performance tuning guide

## Notes for Implementation Team

### Critical Design Principles

1. **Backward Compatibility**: Existing socket code must continue working
2. **Option-Driven Configuration**: Complex behavior configured via socket options
3. **Handler Registration**: Packages register socket mode handlers cleanly
4. **Resource Management**: Automatic cleanup when sockets close
5. **Thread Safety**: Consider future threading requirements

### Code Quality Standards

1. **Error Handling**: Comprehensive error checking and logging
2. **Memory Management**: Proper cleanup of LPC objects and C++ resources
3. **Documentation**: Inline comments and API documentation
4. **Testing**: Unit tests for all major functionality
5. **Performance**: Efficient implementations with minimal overhead

### Development Workflow

1. **Feature Branches**: Each phase in separate feature branch
2. **Code Reviews**: All code must be reviewed before merging
3. **Testing**: Tests must pass before merging to main branch
4. **Documentation**: Update docs with each feature
5. **Performance**: Benchmark major changes

This implementation guide provides the foundation for building the Unified Socket Architecture into FluffOS. The modular approach ensures that each phase can be implemented, tested, and validated independently while building toward the complete unified system.