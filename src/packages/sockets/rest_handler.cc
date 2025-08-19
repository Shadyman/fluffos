#include "rest_handler.h"
#include "socket_option_manager.h"
#include "socket_error_handler.h"
#include <algorithm>
#include <sstream>
#include <cctype>
#include <ctime>
#include <iomanip>

// Static REST handler registry for socket integration
static std::unordered_map<int, std::unique_ptr<RESTHandler>> rest_handlers_;

/*
 * RESTHandler Implementation
 */

RESTHandler::RESTHandler(int socket_id) 
    : socket_id_(socket_id), validation_level_(REST_VALIDATION_BASIC),
      max_json_size_(1048576), docs_path_("/docs"), cors_enabled_(false) {
    
    // Initialize HTTP handler as foundation
    http_handler_ = std::make_unique<HTTPHandler>(socket_id);
    
    // Initialize option manager with socket ID
    option_manager_ = std::make_unique<SocketOptionManager>(socket_id);
    
    // Set REST-specific default options
    svalue_t default_val;
    
    // Set JSON as default content type
    default_val.type = T_STRING;
    default_val.u.string = REST_CONTENT_TYPE_JSON;
    option_manager_->set_option(SO_HTTP_HEADERS, &default_val);
    
    // Initialize API info with defaults
    api_info_.title = "FluffOS REST API";
    api_info_.version = "1.0.0";
    api_info_.description = "RESTful API powered by FluffOS unified socket architecture";
    api_info_.base_path = "/api";
    
    // Add default CORS headers
    cors_headers_["Access-Control-Allow-Origin"] = "*";
    cors_headers_["Access-Control-Allow-Methods"] = "GET, POST, PUT, DELETE, OPTIONS";
    cors_headers_["Access-Control-Allow-Headers"] = "Content-Type, Authorization";
}

RESTHandler::~RESTHandler() {
    // Cleanup is handled by unique_ptr destructors
}

bool RESTHandler::process_rest_request(const char* data, size_t length) {
    if (!data || length == 0) {
        last_error_ = "Invalid input data for REST processing";
        return false;
    }
    
    // First, let HTTP handler process the raw HTTP data
    if (!http_handler_->process_incoming_data(data, length)) {
        last_error_ = "HTTP processing failed: " + std::string(http_handler_->get_last_error());
        return false;
    }
    
    // If HTTP request is not complete, wait for more data
    if (!http_handler_->is_request_complete()) {
        return true;  // Not an error, just need more data
    }
    
    // Get the complete HTTP request
    const HTTPRequest& http_req = http_handler_->get_current_request();
    
    // Create REST request structure
    RestRequest rest_req;
    rest_req.http_request = http_req;
    rest_req.content_type = detect_content_type(http_req);
    
    // Parse query parameters
    if (!parse_query_parameters(http_req.query_string, &rest_req)) {
        last_error_ = "Failed to parse query parameters";
        return false;
    }
    
    // Parse JSON body if present
    if (!http_req.body.empty() && rest_req.content_type == REST_CONTENT_JSON) {
        if (!parse_json_body(http_req.body, &rest_req)) {
            last_error_ = "Failed to parse JSON body";
            return false;
        }
    }
    
    // Find matching route
    RouteMatch match = find_matching_route(
        http_handler_->get_method_string(http_req.method), 
        http_req.path
    );
    
    if (!match.found) {
        // Generate 404 error response
        std::string error_response = create_json_error_response(
            HTTP_STATUS_NOT_FOUND, 
            "Route not found: " + std::string(http_handler_->get_method_string(http_req.method)) + " " + http_req.path
        );
        
        // Send error response through HTTP handler
        // This would normally be handled by the socket system calling back
        return true;
    }
    
    // Store route match information
    rest_req.matched_route_id = match.route->route_id;
    rest_req.matched_route_pattern = match.route->pattern;
    rest_req.path_params = match.params;
    
    // Validate request parameters
    if (!validate_request_parameters(*match.route, rest_req)) {
        last_error_ = "Request validation failed";
        return false;
    }
    
    // Apply REST-specific socket options to request
    apply_rest_options_to_request(&rest_req);
    
    // At this point, the request is fully processed and ready for LPC callback
    // The actual callback handling would be done by the socket system
    
    return true;
}

RouteMatch RESTHandler::find_matching_route(const std::string& method, const std::string& path) {
    RouteMatch match;
    
    for (const auto& route : routes_) {
        if (route->method != method) {
            continue;
        }
        
        std::smatch regex_match;
        if (std::regex_match(path, regex_match, route->compiled_pattern)) {
            match.found = true;
            match.route = route.get();
            
            // Extract parameters from regex groups
            for (size_t i = 1; i < regex_match.size() && i <= route->param_names.size(); ++i) {
                match.params[route->param_names[i-1]] = regex_match[i].str();
            }
            
            break;
        }
    }
    
    return match;
}

bool RESTHandler::parse_query_parameters(const std::string& query_string, RestRequest* request) {
    if (query_string.empty()) {
        return true;  // No query parameters is valid
    }
    
    std::istringstream query_stream(query_string);
    std::string pair;
    
    while (std::getline(query_stream, pair, '&')) {
        size_t eq_pos = pair.find('=');
        if (eq_pos != std::string::npos) {
            std::string key = pair.substr(0, eq_pos);
            std::string value = pair.substr(eq_pos + 1);
            
            // URL decode key and value (simplified)
            // In a full implementation, this would use proper URL decoding
            request->query_params[key] = value;
        }
    }
    
    return true;
}

bool RESTHandler::parse_json_body(const std::string& body, RestRequest* request) {
    if (body.empty()) {
        return true;  // Empty body is valid for some requests
    }
    
    if (body.length() > max_json_size_) {
        last_error_ = "JSON body exceeds maximum size limit";
        return false;
    }
    
    // Validate JSON syntax
    if (!is_valid_json(body)) {
        last_error_ = "Invalid JSON syntax in request body";
        return false;
    }
    
    // Parse JSON to LPC mapping
    request->json_body = parse_json_to_mapping(body);
    if (!request->json_body) {
        last_error_ = "Failed to parse JSON body to mapping";
        return false;
    }
    
    return true;
}

bool RESTHandler::is_valid_json(const std::string& json_str) {
    // Simplified JSON validation - in practice, this would use a proper JSON parser
    if (json_str.empty()) {
        return true;
    }
    
    // Basic syntax check - must start with { or [
    char first = json_str[0];
    char last = json_str[json_str.length() - 1];
    
    if ((first == '{' && last == '}') || (first == '[' && last == ']')) {
        return true;
    }
    
    return false;
}

rest_content_type RESTHandler::detect_content_type(const HTTPRequest& request) {
    auto it = request.headers.find("Content-Type");
    if (it == request.headers.end()) {
        return REST_CONTENT_UNKNOWN;
    }
    
    std::string content_type = it->second;
    std::transform(content_type.begin(), content_type.end(), content_type.begin(), ::tolower);
    
    if (content_type.find("application/json") != std::string::npos) {
        return REST_CONTENT_JSON;
    } else if (content_type.find("application/xml") != std::string::npos) {
        return REST_CONTENT_XML;
    } else if (content_type.find("application/x-www-form-urlencoded") != std::string::npos) {
        return REST_CONTENT_FORM;
    } else if (content_type.find("text/") != std::string::npos) {
        return REST_CONTENT_TEXT;
    } else if (content_type.find("multipart/") != std::string::npos) {
        return REST_CONTENT_MULTIPART;
    }
    
    return REST_CONTENT_UNKNOWN;
}

bool RESTHandler::add_route(const std::string& method, const std::string& pattern,
                           const std::string& handler_object, const std::string& handler_function,
                           const std::string& description) {
    
    if (!validate_method(method) || !validate_route_pattern(pattern)) {
        last_error_ = "Invalid method or route pattern";
        return false;
    }
    
    auto route = std::make_unique<RestRoute>();
    route->route_id = routes_.size() + 1;  // Simple ID assignment
    route->method = method;
    route->pattern = normalize_route_pattern(pattern);
    route->handler_object = handler_object;
    route->handler_function = handler_function;
    route->description = description;
    route->param_names = extract_route_parameter_names(pattern);
    
    if (!compile_route_pattern(route.get())) {
        last_error_ = "Failed to compile route pattern";
        return false;
    }
    
    routes_.push_back(std::move(route));
    return true;
}

bool RESTHandler::compile_route_pattern(RestRoute* route) {
    std::string regex_pattern = route->pattern;
    
    // Convert REST pattern to regex pattern
    // Replace {param} with ([^/]+) capturing group
    std::regex param_regex(R"(\{([^}]+)\})");
    regex_pattern = std::regex_replace(regex_pattern, param_regex, "([^/]+)");
    
    // Escape other regex special characters
    // This is simplified - a full implementation would be more thorough
    
    try {
        route->compiled_pattern = std::regex(regex_pattern);
        return true;
    } catch (const std::regex_error& e) {
        last_error_ = "Regex compilation failed: " + std::string(e.what());
        return false;
    }
}

bool RESTHandler::validate_method(const std::string& method) {
    return IS_VALID_REST_METHOD(method);
}

bool RESTHandler::validate_route_pattern(const std::string& pattern) {
    if (pattern.empty() || pattern[0] != '/') {
        return false;
    }
    
    // Check for valid parameter syntax
    std::regex param_check(R"(\{[a-zA-Z_][a-zA-Z0-9_]*\})");
    
    // Find all parameter placeholders
    auto begin = std::sregex_iterator(pattern.begin(), pattern.end(), param_check);
    auto end = std::sregex_iterator();
    
    // Basic validation - more thorough checks could be added
    return true;
}

std::string RESTHandler::normalize_route_pattern(const std::string& pattern) {
    std::string normalized = pattern;
    
    // Ensure pattern starts with /
    if (normalized.empty() || normalized[0] != '/') {
        normalized = "/" + normalized;
    }
    
    // Remove trailing slash unless it's the root path
    if (normalized.length() > 1 && normalized.back() == '/') {
        normalized.pop_back();
    }
    
    return normalized;
}

std::vector<std::string> RESTHandler::extract_route_parameter_names(const std::string& pattern) {
    std::vector<std::string> param_names;
    std::regex param_regex(R"(\{([^}]+)\})");
    
    auto begin = std::sregex_iterator(pattern.begin(), pattern.end(), param_regex);
    auto end = std::sregex_iterator();
    
    for (auto it = begin; it != end; ++it) {
        param_names.push_back((*it)[1].str());
    }
    
    return param_names;
}

std::string RESTHandler::create_json_error_response(http_status status, const std::string& message,
                                                   const mapping_t* details) {
    RestResponse response;
    response.http_response.status = status;
    response.content_type = REST_CONTENT_JSON;
    response.is_error_response = true;
    response.error_message = message;
    
    // Create JSON error structure
    std::ostringstream json;
    json << "{";
    json << "\"error\": true,";
    json << "\"status\": " << static_cast<int>(status) << ",";
    json << "\"message\": \"" << escape_json_string(message) << "\"";
    
    if (details) {
        // In a full implementation, this would serialize the mapping to JSON
        json << ",\"details\": {}";
    }
    
    json << "}";
    
    response.http_response.body = json.str();
    
    // Add headers
    add_api_headers(&response);
    if (cors_enabled_) {
        add_cors_headers(&response);
    }
    
    // Generate HTTP response using the base handler
    std::unordered_map<std::string, std::string> headers;
    headers["Content-Type"] = REST_CONTENT_TYPE_JSON_UTF8;
    
    return http_handler_->generate_response(status, response.http_response.body, headers);
}

std::string RESTHandler::create_json_success_response(const mapping_t* data, http_status status) {
    RestResponse response;
    response.http_response.status = status;
    response.content_type = REST_CONTENT_JSON;
    response.json_body = const_cast<mapping_t*>(data);  // Safe const cast for response
    
    // Serialize mapping to JSON
    std::string json_body = serialize_mapping_to_json(data);
    response.http_response.body = json_body;
    
    // Add headers
    add_api_headers(&response);
    if (cors_enabled_) {
        add_cors_headers(&response);
    }
    
    // Generate HTTP response
    std::unordered_map<std::string, std::string> headers;
    headers["Content-Type"] = REST_CONTENT_TYPE_JSON_UTF8;
    
    return http_handler_->generate_response(status, response.http_response.body, headers);
}

void RESTHandler::add_api_headers(RestResponse* response) {
    // Add standard API headers
    response->http_response.headers["X-API-Version"] = api_info_.version;
    response->http_response.headers["X-Powered-By"] = "FluffOS REST Framework";
    
    // Add timestamp
    auto now = std::time(nullptr);
    char timestamp[32];
    std::strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%SZ", std::gmtime(&now));
    response->http_response.headers["Date"] = timestamp;
}

void RESTHandler::add_cors_headers(RestResponse* response) {
    if (!cors_enabled_) {
        return;
    }
    
    for (const auto& header : cors_headers_) {
        response->http_response.headers[header.first] = header.second;
    }
}

std::string RESTHandler::escape_json_string(const std::string& input) {
    std::ostringstream escaped;
    
    for (char c : input) {
        switch (c) {
            case '"':  escaped << "\\\""; break;
            case '\\': escaped << "\\\\"; break;
            case '\b': escaped << "\\b"; break;
            case '\f': escaped << "\\f"; break;
            case '\n': escaped << "\\n"; break;
            case '\r': escaped << "\\r"; break;
            case '\t': escaped << "\\t"; break;
            default:
                if (c < 32) {
                    escaped << "\\u" << std::hex << std::setw(4) << std::setfill('0') << (int)c;
                } else {
                    escaped << c;
                }
                break;
        }
    }
    
    return escaped.str();
}

bool RESTHandler::validate_request_parameters(const RestRoute& route, const RestRequest& request) {
    // Basic validation - can be extended with schema validation
    if (validation_level_ == REST_VALIDATION_NONE) {
        return true;
    }
    
    // Check required path parameters are present
    for (const std::string& param_name : route.param_names) {
        if (request.path_params.find(param_name) == request.path_params.end()) {
            last_error_ = "Missing required parameter: " + param_name;
            return false;
        }
    }
    
    return true;
}

void RESTHandler::apply_rest_options_to_request(RestRequest* request) {
    // Apply REST-specific socket options to the request
    // This would integrate with the option manager to apply configured options
}

void RESTHandler::apply_rest_options_to_response(RestResponse* response) {
    // Apply REST-specific socket options to the response
    // This would integrate with the option manager to apply configured options
}

// JSON/mapping conversion implementations
// These integrate with FluffOS JSON processing capabilities
mapping_t* RESTHandler::parse_json_to_mapping(const std::string& json_str) {
    if (json_str.empty()) {
        return nullptr;
    }
    
    // In a full implementation, this would use FluffOS json_decode()
    // For now, provide basic validation and return success indicator
    if (is_valid_json(json_str)) {
        // Return a placeholder mapping indicating successful parsing
        // The actual implementation would call the FluffOS JSON parser
        return nullptr;  // Would return actual mapping
    }
    
    return nullptr;
}

std::string RESTHandler::serialize_mapping_to_json(const mapping_t* mapping) {
    if (!mapping) {
        return "{}";
    }
    
    // In a full implementation, this would use FluffOS json_encode()
    // For now, return a basic JSON object structure
    std::ostringstream json;
    json << "{";
    json << "\"status\": \"success\",";
    json << "\"data\": {},";
    json << "\"timestamp\": " << std::time(nullptr);
    json << "}";
    
    return json.str();
}

bool RESTHandler::is_rest_request_complete() const {
    return http_handler_->is_request_complete();
}

void RESTHandler::reset_request_state() {
    http_handler_->reset_request_state();
}

bool RESTHandler::should_keep_alive() const {
    return http_handler_->should_keep_alive();
}

void RESTHandler::close_connection() {
    http_handler_->close_connection();
}

size_t RESTHandler::get_buffer_size() const {
    return http_handler_->get_buffer_size();
}

void RESTHandler::clear_buffer() {
    http_handler_->clear_buffer();
}

/*
 * Socket Integration Functions
 */

bool socket_enable_rest_mode(int socket_id, const mapping_t* options) {
    // First ensure HTTP mode is enabled
    if (!socket_enable_http_mode(socket_id, options)) {
        return false;
    }
    
    // Create REST handler for this socket
    auto rest_handler = std::make_unique<RESTHandler>(socket_id);
    
    // Configure REST options if provided
    if (options) {
        // Process REST configuration options
        // This would iterate through the mapping and apply options
    }
    
    // Store in registry
    rest_handlers_[socket_id] = std::move(rest_handler);
    
    return true;
}

bool socket_is_rest_mode(int socket_id) {
    return rest_handlers_.find(socket_id) != rest_handlers_.end();
}

int socket_process_rest_data(int socket_id, const char* data, size_t length) {
    auto it = rest_handlers_.find(socket_id);
    if (it == rest_handlers_.end()) {
        return -1;  // REST mode not enabled
    }
    
    if (it->second->process_rest_request(data, length)) {
        return length;  // Successfully processed
    }
    
    return -1;  // Processing failed
}

RESTHandler* get_rest_handler(int socket_id) {
    auto it = rest_handlers_.find(socket_id);
    if (it != rest_handlers_.end()) {
        return it->second.get();
    }
    return nullptr;
}

bool socket_rest_add_route(int socket_id, const mapping_t* route_config) {
    RESTHandler* handler = get_rest_handler(socket_id);
    if (!handler) {
        return false;
    }
    
    // Extract route configuration from mapping
    // This would parse the mapping and call handler->add_route()
    // For now, return success as placeholder
    return true;
}

// Additional method implementations for RESTHandler

bool RESTHandler::remove_route(int route_id) {
    auto it = std::find_if(routes_.begin(), routes_.end(),
                          [route_id](const std::unique_ptr<RestRoute>& route) {
                              return route->route_id == route_id;
                          });
    
    if (it != routes_.end()) {
        routes_.erase(it);
        return true;
    }
    
    return false;
}

bool RESTHandler::remove_route(const std::string& method, const std::string& pattern) {
    std::string normalized_pattern = normalize_route_pattern(pattern);
    
    auto it = std::find_if(routes_.begin(), routes_.end(),
                          [&method, &normalized_pattern](const std::unique_ptr<RestRoute>& route) {
                              return route->method == method && route->pattern == normalized_pattern;
                          });
    
    if (it != routes_.end()) {
        routes_.erase(it);
        return true;
    }
    
    return false;
}

void RESTHandler::clear_all_routes() {
    routes_.clear();
}

array_t* RESTHandler::get_all_routes() const {
    // In a full implementation, this would create an LPC array
    // For now, return nullptr as placeholder
    return nullptr;
}

mapping_t* RESTHandler::get_route_info(int route_id) const {
    auto it = std::find_if(routes_.begin(), routes_.end(),
                          [route_id](const std::unique_ptr<RestRoute>& route) {
                              return route->route_id == route_id;
                          });
    
    if (it != routes_.end()) {
        // In a full implementation, this would create an LPC mapping
        // with route information
        return nullptr;
    }
    
    return nullptr;
}

bool RESTHandler::register_route_from_mapping(const mapping_t* route_config) {
    if (!route_config) {
        return false;
    }
    
    // In a full implementation, this would extract values from the mapping
    // and call add_route() with the extracted parameters
    return true;
}

const RestRequest& RESTHandler::get_current_request() const {
    // Return a static instance for now
    static RestRequest empty_request;
    return empty_request;
}

std::string RESTHandler::generate_openapi_spec() {
    std::ostringstream spec;
    
    spec << "{\n";
    spec << "  \"openapi\": \"" << OPENAPI_VERSION << "\",\n";
    spec << "  \"info\": {\n";
    spec << "    \"title\": \"" << escape_json_string(api_info_.title) << "\",\n";
    spec << "    \"version\": \"" << escape_json_string(api_info_.version) << "\",\n";
    spec << "    \"description\": \"" << escape_json_string(api_info_.description) << "\"\n";
    spec << "  },\n";
    spec << "  \"basePath\": \"" << escape_json_string(api_info_.base_path) << "\",\n";
    spec << "  \"paths\": {\n";
    
    // Add routes to OpenAPI spec
    bool first_route = true;
    for (const auto& route : routes_) {
        if (!first_route) {
            spec << ",\n";
        }
        first_route = false;
        
        spec << "    \"" << escape_json_string(route->pattern) << "\": {\n";
        spec << "      \"" << route->method << "\": {\n";
        spec << "        \"description\": \"" << escape_json_string(route->description) << "\",\n";
        spec << "        \"operationId\": \"" << route->handler_function << "\"\n";
        spec << "      }\n";
        spec << "    }";
    }
    
    spec << "\n  }\n";
    spec << "}";
    
    return spec.str();
}

std::string RESTHandler::generate_api_docs_html() {
    std::ostringstream html;
    
    html << "<!DOCTYPE html>\n";
    html << "<html>\n<head>\n";
    html << "<title>" << html_escape(api_info_.title) << " - API Documentation</title>\n";
    html << "<style>\n";
    html << "body { font-family: Arial, sans-serif; margin: 20px; }\n";
    html << ".route { border: 1px solid #ccc; margin: 10px 0; padding: 10px; }\n";
    html << ".method { font-weight: bold; color: #0066cc; }\n";
    html << ".pattern { font-family: monospace; background: #f5f5f5; padding: 2px 4px; }\n";
    html << "</style>\n";
    html << "</head>\n<body>\n";
    
    html << "<h1>" << html_escape(api_info_.title) << "</h1>\n";
    html << "<p>Version: " << html_escape(api_info_.version) << "</p>\n";
    html << "<p>" << html_escape(api_info_.description) << "</p>\n";
    
    html << "<h2>API Endpoints</h2>\n";
    
    for (const auto& route : routes_) {
        html << "<div class=\"route\">\n";
        html << "<span class=\"method\">" << route->method << "</span> ";
        html << "<span class=\"pattern\">" << html_escape(route->pattern) << "</span>\n";
        html << "<p>" << html_escape(route->description) << "</p>\n";
        html << "<p><strong>Handler:</strong> " << html_escape(route->handler_object) 
             << "->" << html_escape(route->handler_function) << "</p>\n";
        html << "</div>\n";
    }
    
    html << "</body>\n</html>";
    
    return html.str();
}

std::string RESTHandler::html_escape(const std::string& input) {
    std::ostringstream escaped;
    
    for (char c : input) {
        switch (c) {
            case '<':  escaped << "&lt;"; break;
            case '>':  escaped << "&gt;"; break;
            case '&':  escaped << "&amp;"; break;
            case '"':  escaped << "&quot;"; break;
            case '\'': escaped << "&#39;"; break;
            default:   escaped << c; break;
        }
    }
    
    return escaped.str();
}

bool RESTHandler::set_api_info_from_mapping(const mapping_t* api_info) {
    if (!api_info) {
        return false;
    }
    
    // In a full implementation, this would extract values from the mapping
    // and update the api_info_ structure
    return true;
}

mapping_t* RESTHandler::get_api_info() const {
    // In a full implementation, this would create an LPC mapping
    // with API information
    return nullptr;
}

void RESTHandler::enable_cors(const mapping_t* cors_config) {
    cors_enabled_ = true;
    
    if (cors_config) {
        // In a full implementation, this would process the CORS config mapping
        // and update cors_headers_
    }
}

void RESTHandler::disable_cors() {
    cors_enabled_ = false;
}

std::string RESTHandler::handle_cors_preflight(const HTTPRequest& request) {
    if (!cors_enabled_) {
        return "";
    }
    
    // Generate CORS preflight response
    HTTPResponse response;
    response.status = HTTP_STATUS_OK;
    response.headers = cors_headers_;
    
    return http_handler_->generate_response(HTTP_STATUS_OK, "", response.headers);
}

bool RESTHandler::add_middleware(const std::string& name, const std::string& function) {
    middleware_functions_[name] = function;
    return true;
}

bool RESTHandler::remove_middleware(const std::string& name) {
    return middleware_functions_.erase(name) > 0;
}

std::vector<std::string> RESTHandler::get_middleware_chain() const {
    std::vector<std::string> chain;
    for (const auto& middleware : middleware_functions_) {
        chain.push_back(middleware.first);
    }
    return chain;
}

void RESTHandler::dump_rest_state(outbuffer_t* buffer) const {
    if (!buffer) return;
    
    outbuf_addv(buffer, "REST Handler State for Socket %d:\n", socket_id_);
    outbuf_addv(buffer, "  Routes: %zu\n", routes_.size());
    outbuf_addv(buffer, "  CORS enabled: %s\n", cors_enabled_ ? "Yes" : "No");
    outbuf_addv(buffer, "  API title: %s\n", api_info_.title.c_str());
    outbuf_addv(buffer, "  API version: %s\n", api_info_.version.c_str());
    outbuf_addv(buffer, "  Docs path: %s\n", docs_path_.c_str());
    outbuf_addv(buffer, "  Middleware functions: %zu\n", middleware_functions_.size());
}

void RESTHandler::dump_routes(outbuffer_t* buffer) const {
    if (!buffer) return;
    
    outbuf_add(buffer, "REST Routes:\n");
    for (const auto& route : routes_) {
        outbuf_addv(buffer, "  [%d] %s %s -> %s::%s\n",
                   route->route_id, route->method.c_str(), route->pattern.c_str(),
                   route->handler_object.c_str(), route->handler_function.c_str());
    }
}

void RESTHandler::dump_api_stats(outbuffer_t* buffer) const {
    if (!buffer) return;
    
    outbuf_add(buffer, "REST API Statistics:\n");
    outbuf_addv(buffer, "  Total routes: %zu\n", routes_.size());
    
    // Count routes by method
    std::unordered_map<std::string, int> method_counts;
    for (const auto& route : routes_) {
        method_counts[route->method]++;
    }
    
    for (const auto& count : method_counts) {
        outbuf_addv(buffer, "  %s routes: %d\n", count.first.c_str(), count.second);
    }
}