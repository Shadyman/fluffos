/*
 * GraphQL Package Header
 * 
 * FluffOS GraphQL package for unified socket architecture.
 * Provides GraphQL server and client functionality with MUD-specific features.
 */

#ifndef PACKAGES_GRAPHQL_H_
#define PACKAGES_GRAPHQL_H_

#include "base/package_api.h"
#include "packages/sockets/socket_options.h"
#include "packages/sockets/socket_option_manager.h"

#include <memory>
#include <string>
#include <vector>
#include <map>
#include <functional>

// Forward declarations
class GraphQLServer;
class GraphQLSchema;
class GraphQLResolver;
class GraphQLSubscriptionManager;

/*
 * GraphQL Query Types
 */
enum GraphQLOperationType {
    GRAPHQL_QUERY = 0,
    GRAPHQL_MUTATION = 1,
    GRAPHQL_SUBSCRIPTION = 2
};

/*
 * GraphQL Response Status
 */
enum GraphQLStatus {
    GRAPHQL_SUCCESS = 0,
    GRAPHQL_ERROR = 1,
    GRAPHQL_VALIDATION_ERROR = 2,
    GRAPHQL_EXECUTION_ERROR = 3,
    GRAPHQL_TIMEOUT_ERROR = 4
};

/*
 * GraphQL Request Structure
 */
struct GraphQLRequest {
    std::string query;
    std::map<std::string, std::string> variables;
    std::string operation_name;
    GraphQLOperationType operation_type;
    int socket_fd;
    object_t* requester;
};

/*
 * GraphQL Response Structure
 */
struct GraphQLResponse {
    GraphQLStatus status;
    std::string data;
    std::vector<std::string> errors;
    std::map<std::string, std::string> extensions;
};

/*
 * GraphQL Resolver Function Type
 */
typedef std::function<std::string(const std::map<std::string, std::string>&, object_t*)> GraphQLResolverFunc;

/*
 * GraphQL Server Manager
 * Handles GraphQL operations for unified socket architecture
 */
class GraphQLManager {
public:
    static GraphQLManager* getInstance();
    
    // Server management
    bool initialize();
    void shutdown();
    
    // Socket integration
    int create_graphql_socket(int socket_fd, const std::string& mode);
    bool handle_graphql_request(int socket_fd, const std::string& data);
    void close_graphql_socket(int socket_fd);
    
    // Schema management
    bool load_schema(int socket_fd, const std::string& schema_text);
    bool validate_schema(const std::string& schema_text);
    std::string get_schema_sdl(int socket_fd);
    
    // Query execution
    GraphQLResponse execute_query(int socket_fd, const GraphQLRequest& request);
    bool validate_query(const std::string& query, const std::string& schema);
    
    // Subscription management
    bool subscribe(int socket_fd, const std::string& subscription, 
                   const std::map<std::string, std::string>& variables);
    void unsubscribe(int socket_fd, const std::string& subscription_id);
    void broadcast_event(const std::string& event_type, const std::string& data);
    
    // Resolver registration
    void register_resolver(const std::string& type_name, const std::string& field_name,
                          GraphQLResolverFunc resolver);
    void register_mud_resolvers();
    
    // Error handling
    void log_error(int socket_fd, const std::string& error, const std::string& context);
    
private:
    GraphQLManager();
    ~GraphQLManager();
    
    static GraphQLManager* instance_;
    
    std::map<int, std::unique_ptr<GraphQLServer>> servers_;
    std::map<std::string, GraphQLResolverFunc> resolvers_;
    std::unique_ptr<GraphQLSubscriptionManager> subscription_manager_;
    
    bool initialized_;
    
    // Internal methods
    bool parse_graphql_request(const std::string& data, GraphQLRequest& request);
    std::string format_graphql_response(const GraphQLResponse& response);
    void setup_cors_headers(int socket_fd, const std::string& origin);
};

/*
 * GraphQL Server Implementation
 * Handles individual GraphQL server instances per socket
 */
class GraphQLServer {
public:
    GraphQLServer(int socket_fd);
    ~GraphQLServer();
    
    // Configuration
    bool configure(std::unique_ptr<SocketOptionManager> option_manager);
    void set_schema(const std::string& schema_text);
    void enable_introspection(bool enabled);
    void enable_playground(bool enabled);
    void set_max_query_depth(int depth);
    void set_max_query_complexity(int complexity);
    void set_timeout(int timeout_ms);
    
    // Request handling
    GraphQLResponse handle_request(const GraphQLRequest& request);
    bool handle_introspection_query(std::string& response);
    bool handle_playground_request(std::string& response);
    
    // Subscription support
    bool supports_subscriptions() const;
    void enable_subscriptions(bool enabled);
    
    // Validation
    bool validate_query_depth(const std::string& query, int max_depth);
    bool validate_query_complexity(const std::string& query, int max_complexity);
    
    // Status
    bool is_configured() const;
    std::string get_status() const;
    
private:
    int socket_fd_;
    std::unique_ptr<SocketOptionManager> option_manager_;
    std::unique_ptr<GraphQLSchema> schema_;
    
    // Configuration
    bool introspection_enabled_;
    bool playground_enabled_;
    bool subscriptions_enabled_;
    int max_query_depth_;
    int max_query_complexity_;
    int timeout_ms_;
    
    // CORS settings
    std::vector<std::string> cors_origins_;
    
    bool configured_;
};

/*
 * GraphQL Schema Management
 */
class GraphQLSchema {
public:
    GraphQLSchema();
    ~GraphQLSchema();
    
    // Schema loading
    bool load_from_string(const std::string& schema_text);
    bool load_from_file(const std::string& file_path);
    
    // Validation
    bool validate();
    std::vector<std::string> get_validation_errors() const;
    
    // Introspection
    std::string get_introspection_query() const;
    std::string get_schema_sdl() const;
    
    // Type system
    bool has_type(const std::string& type_name) const;
    std::vector<std::string> get_type_fields(const std::string& type_name) const;
    std::string get_field_type(const std::string& type_name, const std::string& field_name) const;
    
    // Query analysis
    int calculate_query_depth(const std::string& query) const;
    int calculate_query_complexity(const std::string& query) const;
    
private:
    std::string schema_text_;
    std::map<std::string, std::map<std::string, std::string>> types_;
    std::vector<std::string> validation_errors_;
    bool valid_;
    
    // Internal parsing
    bool parse_schema();
    void extract_types();
    void validate_type_references();
};

/*
 * GraphQL Subscription Manager
 * Handles real-time subscriptions via WebSocket integration
 */
class GraphQLSubscriptionManager {
public:
    GraphQLSubscriptionManager();
    ~GraphQLSubscriptionManager();
    
    // Subscription lifecycle
    std::string create_subscription(int socket_fd, const std::string& subscription,
                                   const std::map<std::string, std::string>& variables);
    bool remove_subscription(const std::string& subscription_id);
    void remove_all_subscriptions(int socket_fd);
    
    // Event broadcasting
    void broadcast_to_subscription(const std::string& subscription_id, const std::string& data);
    void broadcast_to_type(const std::string& event_type, const std::string& data);
    void broadcast_global(const std::string& data);
    
    // Connection management
    void register_connection(int socket_fd);
    void unregister_connection(int socket_fd);
    bool is_connection_active(int socket_fd);
    
    // Statistics
    int get_subscription_count() const;
    int get_connection_count() const;
    std::map<std::string, int> get_subscription_stats() const;
    
private:
    struct Subscription {
        std::string id;
        int socket_fd;
        std::string query;
        std::string event_type;
        std::map<std::string, std::string> variables;
        time_t created_at;
    };
    
    std::map<std::string, Subscription> subscriptions_;
    std::map<int, std::vector<std::string>> socket_subscriptions_;
    std::map<std::string, std::vector<std::string>> event_subscriptions_;
    
    std::string generate_subscription_id();
    std::string extract_event_type(const std::string& subscription);
    void cleanup_expired_subscriptions();
};

// C interface for LPC integration
extern "C" {
    void init_graphql_package();
    void clean_graphql_package();
    
    // Socket integration functions
    void graphql_socket_close(int fd);
    int graphql_socket_read(int fd, char* buf, int len);
    int graphql_socket_write(int fd, const char* buf, int len);
    
    // GraphQL operations
    int graphql_set_schema(int fd, const char* schema);
    int graphql_execute_query(int fd, const char* query, const char* variables);
    int graphql_subscribe(int fd, const char* subscription, const char* variables);
    
    // Event broadcasting
    void graphql_broadcast_event(const char* event_type, const char* data);
    void graphql_broadcast_player_event(const char* player_id, const char* event_type, const char* data);
    void graphql_broadcast_room_event(const char* room_id, const char* event_type, const char* data);
}

// Debug logging for GraphQL operations
#ifdef DEBUG
#define GRAPHQL_DEBUG(x) debug_message("[GRAPHQL] " x)
#define GRAPHQL_DEBUG_F(fmt, ...) debug_message("[GRAPHQL] " fmt, __VA_ARGS__)
#else
#define GRAPHQL_DEBUG(x)
#define GRAPHQL_DEBUG_F(fmt, ...)
#endif

#endif /* PACKAGES_GRAPHQL_H_ */