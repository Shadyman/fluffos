/*
 * GraphQL Package Implementation
 * 
 * FluffOS GraphQL package for unified socket architecture.
 * Provides GraphQL server and client functionality with MUD-specific features.
 */

#include "graphql.h"
#include "packages/sockets/socket_option_manager.h"

#include <sstream>
#include <algorithm>
#include <chrono>
#include <regex>
#include <random>

// Static instance pointer
GraphQLManager* GraphQLManager::instance_ = nullptr;

/*
 * GraphQL Manager Implementation
 */

GraphQLManager* GraphQLManager::getInstance() {
    if (!instance_) {
        instance_ = new GraphQLManager();
    }
    return instance_;
}

GraphQLManager::GraphQLManager() : initialized_(false) {
    subscription_manager_ = std::make_unique<GraphQLSubscriptionManager>();
}

GraphQLManager::~GraphQLManager() {
    shutdown();
}

bool GraphQLManager::initialize() {
    if (initialized_) {
        return true;
    }
    
    GRAPHQL_DEBUG("Initializing GraphQL Manager");
    
    // Register default MUD resolvers
    register_mud_resolvers();
    
    initialized_ = true;
    GRAPHQL_DEBUG("GraphQL Manager initialized successfully");
    return true;
}

void GraphQLManager::shutdown() {
    if (!initialized_) {
        return;
    }
    
    GRAPHQL_DEBUG("Shutting down GraphQL Manager");
    
    // Close all servers
    servers_.clear();
    resolvers_.clear();
    subscription_manager_.reset();
    
    initialized_ = false;
    GRAPHQL_DEBUG("GraphQL Manager shutdown complete");
}

int GraphQLManager::create_graphql_socket(int socket_fd, const std::string& mode) {
    if (!initialized_) {
        if (!initialize()) {
            log_error(socket_fd, "Failed to initialize GraphQL Manager", "create_socket");
            return -1;
        }
    }
    
    GRAPHQL_DEBUG_F("Creating GraphQL socket for fd %d with mode %s", socket_fd, mode.c_str());
    
    // Create new server instance
    auto server = std::make_unique<GraphQLServer>(socket_fd);
    if (!server) {
        log_error(socket_fd, "Failed to create GraphQL server instance", "create_socket");
        return -1;
    }
    
    // Store server instance
    servers_[socket_fd] = std::move(server);
    
    // Register connection with subscription manager
    subscription_manager_->register_connection(socket_fd);
    
    GRAPHQL_DEBUG_F("GraphQL socket created successfully for fd %d", socket_fd);
    return socket_fd;
}

bool GraphQLManager::handle_graphql_request(int socket_fd, const std::string& data) {
    auto server_it = servers_.find(socket_fd);
    if (server_it == servers_.end()) {
        log_error(socket_fd, "GraphQL server not found for socket", "handle_request");
        return false;
    }
    
    GRAPHQL_DEBUG_F("Handling GraphQL request for fd %d", socket_fd);
    
    // Parse request
    GraphQLRequest request;
    if (!parse_graphql_request(data, request)) {
        log_error(socket_fd, "Failed to parse GraphQL request", "handle_request");
        return false;
    }
    
    request.socket_fd = socket_fd;
    
    // Handle request based on type
    GraphQLResponse response;
    
    if (request.operation_type == GRAPHQL_SUBSCRIPTION) {
        // Handle subscription
        if (!subscribe(socket_fd, request.query, request.variables)) {
            response.status = GRAPHQL_ERROR;
            response.errors.push_back("Failed to create subscription");
        } else {
            response.status = GRAPHQL_SUCCESS;
            response.data = "{\"data\": {\"subscription\": \"created\"}}";
        }
    } else {
        // Handle query/mutation
        response = server_it->second->handle_request(request);
    }
    
    // Format and send response
    std::string response_text = format_graphql_response(response);
    
    // In a real implementation, this would write to the socket
    // For now, we'll log the response
    GRAPHQL_DEBUG_F("GraphQL response for fd %d: %s", socket_fd, response_text.c_str());
    
    return response.status == GRAPHQL_SUCCESS;
}

void GraphQLManager::close_graphql_socket(int socket_fd) {
    GRAPHQL_DEBUG_F("Closing GraphQL socket for fd %d", socket_fd);
    
    // Remove all subscriptions for this socket
    subscription_manager_->remove_all_subscriptions(socket_fd);
    subscription_manager_->unregister_connection(socket_fd);
    
    // Remove server instance
    servers_.erase(socket_fd);
    
    GRAPHQL_DEBUG_F("GraphQL socket closed for fd %d", socket_fd);
}

bool GraphQLManager::load_schema(int socket_fd, const std::string& schema_text) {
    auto server_it = servers_.find(socket_fd);
    if (server_it == servers_.end()) {
        log_error(socket_fd, "GraphQL server not found for socket", "load_schema");
        return false;
    }
    
    GRAPHQL_DEBUG_F("Loading schema for fd %d", socket_fd);
    
    server_it->second->set_schema(schema_text);
    return true;
}

bool GraphQLManager::validate_schema(const std::string& schema_text) {
    auto schema = std::make_unique<GraphQLSchema>();
    bool valid = schema->load_from_string(schema_text) && schema->validate();
    
    GRAPHQL_DEBUG_F("Schema validation result: %s", valid ? "valid" : "invalid");
    return valid;
}

std::string GraphQLManager::get_schema_sdl(int socket_fd) {
    auto server_it = servers_.find(socket_fd);
    if (server_it == servers_.end()) {
        return "";
    }
    
    // This would return the SDL representation of the schema
    return "# GraphQL Schema Definition Language\ntype Query { hello: String }";
}

GraphQLResponse GraphQLManager::execute_query(int socket_fd, const GraphQLRequest& request) {
    auto server_it = servers_.find(socket_fd);
    if (server_it == servers_.end()) {
        GraphQLResponse response;
        response.status = GRAPHQL_ERROR;
        response.errors.push_back("GraphQL server not found for socket");
        return response;
    }
    
    return server_it->second->handle_request(request);
}

bool GraphQLManager::validate_query(const std::string& query, const std::string& schema) {
    // Basic query validation - in a real implementation this would be more sophisticated
    if (query.empty()) {
        return false;
    }
    
    // Check for basic GraphQL syntax
    std::regex query_pattern(R"(^\s*(query|mutation|subscription)\s+)");
    return std::regex_search(query, query_pattern) || query.find("{") != std::string::npos;
}

bool GraphQLManager::subscribe(int socket_fd, const std::string& subscription, 
                              const std::map<std::string, std::string>& variables) {
    if (!subscription_manager_->is_connection_active(socket_fd)) {
        log_error(socket_fd, "Connection not active for subscription", "subscribe");
        return false;
    }
    
    std::string subscription_id = subscription_manager_->create_subscription(
        socket_fd, subscription, variables);
    
    GRAPHQL_DEBUG_F("Created subscription %s for fd %d", subscription_id.c_str(), socket_fd);
    return !subscription_id.empty();
}

void GraphQLManager::unsubscribe(int socket_fd, const std::string& subscription_id) {
    subscription_manager_->remove_subscription(subscription_id);
    GRAPHQL_DEBUG_F("Removed subscription %s for fd %d", subscription_id.c_str(), socket_fd);
}

void GraphQLManager::broadcast_event(const std::string& event_type, const std::string& data) {
    subscription_manager_->broadcast_to_type(event_type, data);
    GRAPHQL_DEBUG_F("Broadcast event %s: %s", event_type.c_str(), data.c_str());
}

void GraphQLManager::register_resolver(const std::string& type_name, const std::string& field_name,
                                     GraphQLResolverFunc resolver) {
    std::string key = type_name + "." + field_name;
    resolvers_[key] = resolver;
    GRAPHQL_DEBUG_F("Registered resolver for %s", key.c_str());
}

void GraphQLManager::register_mud_resolvers() {
    GRAPHQL_DEBUG("Registering default MUD resolvers");
    
    // Register basic resolvers for MUD functionality
    register_resolver("Query", "hello", [](const std::map<std::string, std::string>& args, object_t* context) {
        return "{\"hello\": \"World from MUD!\"}";
    });
    
    register_resolver("Query", "players", [](const std::map<std::string, std::string>& args, object_t* context) {
        return "{\"players\": [{\"name\": \"TestPlayer\", \"level\": 1}]}";
    });
    
    register_resolver("Query", "rooms", [](const std::map<std::string, std::string>& args, object_t* context) {
        return "{\"rooms\": [{\"id\": \"room1\", \"title\": \"Test Room\"}]}";
    });
}

void GraphQLManager::log_error(int socket_fd, const std::string& error, const std::string& context) {
    // In a real implementation, this would integrate with FluffOS logging
    GRAPHQL_DEBUG_F("ERROR [%s] fd %d: %s", context.c_str(), socket_fd, error.c_str());
}

bool GraphQLManager::parse_graphql_request(const std::string& data, GraphQLRequest& request) {
    // Basic JSON-like parsing - in a real implementation this would use a proper JSON parser
    
    // Set defaults
    request.operation_type = GRAPHQL_QUERY;
    request.requester = nullptr;
    
    // Look for query field
    size_t query_pos = data.find("\"query\":");
    if (query_pos != std::string::npos) {
        size_t start = data.find("\"", query_pos + 8);
        size_t end = data.find("\"", start + 1);
        if (start != std::string::npos && end != std::string::npos) {
            request.query = data.substr(start + 1, end - start - 1);
        }
    }
    
    // Determine operation type from query
    if (request.query.find("subscription") != std::string::npos) {
        request.operation_type = GRAPHQL_SUBSCRIPTION;
    } else if (request.query.find("mutation") != std::string::npos) {
        request.operation_type = GRAPHQL_MUTATION;
    }
    
    return !request.query.empty();
}

std::string GraphQLManager::format_graphql_response(const GraphQLResponse& response) {
    std::ostringstream oss;
    oss << "{";
    
    if (response.status == GRAPHQL_SUCCESS) {
        oss << "\"data\":" << response.data;
    }
    
    if (!response.errors.empty()) {
        if (response.status == GRAPHQL_SUCCESS) {
            oss << ",";
        }
        oss << "\"errors\":[";
        for (size_t i = 0; i < response.errors.size(); ++i) {
            if (i > 0) oss << ",";
            oss << "{\"message\":\"" << response.errors[i] << "\"}";
        }
        oss << "]";
    }
    
    if (!response.extensions.empty()) {
        oss << ",\"extensions\":{";
        bool first = true;
        for (const auto& ext : response.extensions) {
            if (!first) oss << ",";
            oss << "\"" << ext.first << "\":\"" << ext.second << "\"";
            first = false;
        }
        oss << "}";
    }
    
    oss << "}";
    return oss.str();
}

void GraphQLManager::setup_cors_headers(int socket_fd, const std::string& origin) {
    // CORS setup would be handled here
    GRAPHQL_DEBUG_F("Setting up CORS for fd %d with origin %s", socket_fd, origin.c_str());
}

/*
 * GraphQL Server Implementation
 */

GraphQLServer::GraphQLServer(int socket_fd) 
    : socket_fd_(socket_fd), configured_(false), introspection_enabled_(true),
      playground_enabled_(true), subscriptions_enabled_(true), max_query_depth_(15),
      max_query_complexity_(1000), timeout_ms_(30000) {
    
    GRAPHQL_DEBUG_F("Creating GraphQL server for socket %d", socket_fd);
}

GraphQLServer::~GraphQLServer() {
    GRAPHQL_DEBUG_F("Destroying GraphQL server for socket %d", socket_fd_);
}

bool GraphQLServer::configure(std::unique_ptr<SocketOptionManager> option_manager) {
    option_manager_ = std::move(option_manager);
    
    if (!option_manager_) {
        return false;
    }
    
    // Sync configuration from socket options
    std::string schema_text;
    if (option_manager_->get_option(GRAPHQL_SCHEMA, schema_text)) {
        set_schema(schema_text);
    }
    
    int introspection;
    if (option_manager_->get_option(GRAPHQL_INTROSPECTION, introspection)) {
        enable_introspection(introspection != 0);
    }
    
    int playground;
    if (option_manager_->get_option(GRAPHQL_PLAYGROUND, playground)) {
        enable_playground(playground != 0);
    }
    
    int max_depth;
    if (option_manager_->get_option(GRAPHQL_MAX_QUERY_DEPTH, max_depth)) {
        set_max_query_depth(max_depth);
    }
    
    int max_complexity;
    if (option_manager_->get_option(GRAPHQL_MAX_QUERY_COMPLEXITY, max_complexity)) {
        set_max_query_complexity(max_complexity);
    }
    
    int timeout;
    if (option_manager_->get_option(GRAPHQL_TIMEOUT, timeout)) {
        set_timeout(timeout);
    }
    
    configured_ = true;
    GRAPHQL_DEBUG_F("GraphQL server configured for socket %d", socket_fd_);
    return true;
}

void GraphQLServer::set_schema(const std::string& schema_text) {
    schema_ = std::make_unique<GraphQLSchema>();
    if (schema_->load_from_string(schema_text)) {
        GRAPHQL_DEBUG_F("Schema loaded for socket %d", socket_fd_);
    } else {
        GRAPHQL_DEBUG_F("Failed to load schema for socket %d", socket_fd_);
    }
}

void GraphQLServer::enable_introspection(bool enabled) {
    introspection_enabled_ = enabled;
    GRAPHQL_DEBUG_F("Introspection %s for socket %d", enabled ? "enabled" : "disabled", socket_fd_);
}

void GraphQLServer::enable_playground(bool enabled) {
    playground_enabled_ = enabled;
    GRAPHQL_DEBUG_F("Playground %s for socket %d", enabled ? "enabled" : "disabled", socket_fd_);
}

void GraphQLServer::set_max_query_depth(int depth) {
    max_query_depth_ = depth;
    GRAPHQL_DEBUG_F("Max query depth set to %d for socket %d", depth, socket_fd_);
}

void GraphQLServer::set_max_query_complexity(int complexity) {
    max_query_complexity_ = complexity;
    GRAPHQL_DEBUG_F("Max query complexity set to %d for socket %d", complexity, socket_fd_);
}

void GraphQLServer::set_timeout(int timeout_ms) {
    timeout_ms_ = timeout_ms;
    GRAPHQL_DEBUG_F("Timeout set to %d ms for socket %d", timeout_ms, socket_fd_);
}

GraphQLResponse GraphQLServer::handle_request(const GraphQLRequest& request) {
    GraphQLResponse response;
    response.status = GRAPHQL_SUCCESS;
    
    GRAPHQL_DEBUG_F("Handling request for socket %d", socket_fd_);
    
    // Validate query depth and complexity
    if (!validate_query_depth(request.query, max_query_depth_)) {
        response.status = GRAPHQL_VALIDATION_ERROR;
        response.errors.push_back("Query depth exceeds maximum allowed");
        return response;
    }
    
    if (!validate_query_complexity(request.query, max_query_complexity_)) {
        response.status = GRAPHQL_VALIDATION_ERROR;
        response.errors.push_back("Query complexity exceeds maximum allowed");
        return response;
    }
    
    // Handle introspection queries
    if (request.query.find("__schema") != std::string::npos || 
        request.query.find("__type") != std::string::npos) {
        
        if (!introspection_enabled_) {
            response.status = GRAPHQL_ERROR;
            response.errors.push_back("Introspection is disabled");
            return response;
        }
        
        std::string introspection_response;
        if (handle_introspection_query(introspection_response)) {
            response.data = introspection_response;
        } else {
            response.status = GRAPHQL_ERROR;
            response.errors.push_back("Introspection query failed");
        }
        return response;
    }
    
    // Execute normal query/mutation
    // In a real implementation, this would parse and execute the GraphQL query
    response.data = "{\"data\": {\"hello\": \"World\"}}";
    
    return response;
}

bool GraphQLServer::handle_introspection_query(std::string& response) {
    if (!schema_) {
        return false;
    }
    
    // Basic introspection response
    response = schema_->get_introspection_query();
    return true;
}

bool GraphQLServer::handle_playground_request(std::string& response) {
    if (!playground_enabled_) {
        return false;
    }
    
    // Return GraphQL Playground HTML
    response = R"(
<!DOCTYPE html>
<html>
<head>
    <title>GraphQL Playground</title>
    <link rel="stylesheet" href="//cdn.jsdelivr.net/npm/graphql-playground-react/build/static/css/index.css" />
    <link rel="shortcut icon" href="//cdn.jsdelivr.net/npm/graphql-playground-react/build/favicon.png" />
    <script src="//cdn.jsdelivr.net/npm/graphql-playground-react/build/static/js/middleware.js"></script>
</head>
<body>
    <div id="root">
        <style>
            body {
                background-color: rgb(23, 42, 58);
                font-family: Open Sans, sans-serif;
                height: 90vh;
            }
            #root {
                height: 100%;
                width: 100%;
                display: flex;
                align-items: center;
                justify-content: center;
            }
        </style>
        <div>Loading...</div>
    </div>
    <script>
        window.addEventListener('load', function (event) {
            GraphQLPlayground.init(document.getElementById('root'), {
                endpoint: '/graphql'
            })
        })
    </script>
</body>
</html>
)";
    
    return true;
}

bool GraphQLServer::supports_subscriptions() const {
    return subscriptions_enabled_;
}

void GraphQLServer::enable_subscriptions(bool enabled) {
    subscriptions_enabled_ = enabled;
    GRAPHQL_DEBUG_F("Subscriptions %s for socket %d", enabled ? "enabled" : "disabled", socket_fd_);
}

bool GraphQLServer::validate_query_depth(const std::string& query, int max_depth) {
    // Simple depth calculation by counting nested braces
    int depth = 0;
    int max_found = 0;
    
    for (char c : query) {
        if (c == '{') {
            depth++;
            max_found = std::max(max_found, depth);
        } else if (c == '}') {
            depth--;
        }
    }
    
    return max_found <= max_depth;
}

bool GraphQLServer::validate_query_complexity(const std::string& query, int max_complexity) {
    // Simple complexity calculation by counting fields
    int complexity = 0;
    
    // Count field selections (simplified)
    for (size_t i = 0; i < query.length(); ++i) {
        if (std::isalpha(query[i])) {
            // Found a field name
            complexity++;
            // Skip to end of word
            while (i < query.length() && (std::isalnum(query[i]) || query[i] == '_')) {
                i++;
            }
        }
    }
    
    return complexity <= max_complexity;
}

bool GraphQLServer::is_configured() const {
    return configured_;
}

std::string GraphQLServer::get_status() const {
    std::ostringstream oss;
    oss << "GraphQL Server Status for socket " << socket_fd_ << ":\n";
    oss << "  Configured: " << (configured_ ? "Yes" : "No") << "\n";
    oss << "  Schema loaded: " << (schema_ ? "Yes" : "No") << "\n";
    oss << "  Introspection: " << (introspection_enabled_ ? "Enabled" : "Disabled") << "\n";
    oss << "  Playground: " << (playground_enabled_ ? "Enabled" : "Disabled") << "\n";
    oss << "  Subscriptions: " << (subscriptions_enabled_ ? "Enabled" : "Disabled") << "\n";
    oss << "  Max query depth: " << max_query_depth_ << "\n";
    oss << "  Max query complexity: " << max_query_complexity_ << "\n";
    oss << "  Timeout: " << timeout_ms_ << "ms";
    return oss.str();
}

/*
 * C Interface for LPC Integration
 */

extern "C" {
    void init_graphql_package() {
        GraphQLManager::getInstance()->initialize();
    }
    
    void clean_graphql_package() {
        auto* manager = GraphQLManager::getInstance();
        if (manager) {
            manager->shutdown();
        }
    }
    
    void graphql_socket_close(int fd) {
        GraphQLManager::getInstance()->close_graphql_socket(fd);
    }
    
    int graphql_socket_read(int fd, char* buf, int len) {
        // Socket read would be handled by the socket system
        return 0;
    }
    
    int graphql_socket_write(int fd, const char* buf, int len) {
        // Socket write would be handled by the socket system
        return len;
    }
    
    int graphql_set_schema(int fd, const char* schema) {
        return GraphQLManager::getInstance()->load_schema(fd, std::string(schema)) ? 1 : 0;
    }
    
    int graphql_execute_query(int fd, const char* query, const char* variables) {
        GraphQLRequest request;
        request.query = std::string(query);
        request.socket_fd = fd;
        
        // Parse variables if provided
        if (variables && strlen(variables) > 0) {
            // Simple variable parsing - in real implementation this would be JSON
            request.variables["input"] = std::string(variables);
        }
        
        GraphQLResponse response = GraphQLManager::getInstance()->execute_query(fd, request);
        return response.status == GRAPHQL_SUCCESS ? 1 : 0;
    }
    
    int graphql_subscribe(int fd, const char* subscription, const char* variables) {
        std::map<std::string, std::string> var_map;
        if (variables && strlen(variables) > 0) {
            var_map["input"] = std::string(variables);
        }
        
        return GraphQLManager::getInstance()->subscribe(fd, std::string(subscription), var_map) ? 1 : 0;
    }
    
    void graphql_broadcast_event(const char* event_type, const char* data) {
        GraphQLManager::getInstance()->broadcast_event(std::string(event_type), std::string(data));
    }
    
    void graphql_broadcast_player_event(const char* player_id, const char* event_type, const char* data) {
        std::string event = std::string("player.") + event_type;
        GraphQLManager::getInstance()->broadcast_event(event, std::string(data));
    }
    
    void graphql_broadcast_room_event(const char* room_id, const char* event_type, const char* data) {
        std::string event = std::string("room.") + event_type;
        GraphQLManager::getInstance()->broadcast_event(event, std::string(data));
    }
}