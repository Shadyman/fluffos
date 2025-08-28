//
// GraphQL Package Function Specifications
//
// This file defines the LPC-callable functions provided by the GraphQL package
// for the FluffOS unified socket system.
//

#ifdef PACKAGE_GRAPHQL

// GraphQL server and client management
int graphql_execute_query(int fd, string query, string variables);
int graphql_subscribe(int fd, string subscription, string variables);
int graphql_set_schema(int fd, string schema);

// Event broadcasting for real-time subscriptions  
void graphql_broadcast_event(string event_type, string data);
void graphql_broadcast_player_event(string player_id, string event_type, string data);
void graphql_broadcast_room_event(string room_id, string event_type, string data);

// GraphQL socket type constants
#define GRAPHQL_SERVER 400
#define GRAPHQL_CLIENT 401

// GraphQL socket options
#define GRAPHQL_SCHEMA 420
#define GRAPHQL_INTROSPECTION 421
#define GRAPHQL_PLAYGROUND 422
#define GRAPHQL_MAX_QUERY_DEPTH 423
#define GRAPHQL_MAX_QUERY_COMPLEXITY 424
#define GRAPHQL_TIMEOUT 425
#define GRAPHQL_SUBSCRIPTIONS 426
#define GRAPHQL_ENDPOINT_PATH 427
#define GRAPHQL_CORS_ORIGINS 428
#define GRAPHQL_AUTH_REQUIRED 429

// GraphQL operation types
#define GRAPHQL_QUERY 0
#define GRAPHQL_MUTATION 1  
#define GRAPHQL_SUBSCRIPTION 2

// GraphQL status codes
#define GRAPHQL_SUCCESS 0
#define GRAPHQL_ERROR 1
#define GRAPHQL_VALIDATION_ERROR 2
#define GRAPHQL_EXECUTION_ERROR 3
#define GRAPHQL_TIMEOUT_ERROR 4

#endif /* PACKAGE_GRAPHQL */