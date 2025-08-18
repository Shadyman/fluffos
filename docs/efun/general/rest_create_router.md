---
layout: doc
title: general / rest_create_router
---
# rest_create_router

### NAME

    rest_create_router

### SYNOPSIS

    int rest_create_router()

### DESCRIPTION

    Create a new REST router for URL pattern matching using PACKAGE_REST.

    Initializes a REST router that can match URL patterns, extract path parameters,
    and route HTTP requests to appropriate handler functions.
    
    RETURNS:
    - int: Router ID (positive integer) on success, -1 on failure
    
    The router supports:
    - URL pattern matching with parameter extraction using {param} syntax
    - All HTTP methods (GET, POST, PUT, DELETE, PATCH, etc.)
    - Route-specific metadata for documentation
    - Integration with OpenAPI documentation generation
    
    EXAMPLES:
    ```lpc
    // Create a new router
    int router = rest_create_router();
    if (router == -1) {
        error("Failed to create REST router");
    }
    
    // Add routes with parameter extraction
    rest_add_route(router, "GET", "/users/{id}", (: get_user_handler :));
    rest_add_route(router, "POST", "/users", (: create_user_handler :));
    rest_add_route(router, "PUT", "/users/{id}/posts/{post_id}", (: update_post_handler :));
    rest_add_route(router, "DELETE", "/users/{id}", (: delete_user_handler :));
    
    // Integrate with HTTP server
    void http_request_handler(mapping request) {
        mapping route_result = rest_process_route(router, request);
        if (!sizeof(route_result)) {
            // No matching route found
            http_send_response(request["id"], (["status": 404]));
            return;
        }
        
        // Call the matched handler with extracted parameters
        evaluate(route_result["handler"], request, 
                route_result["path_params"], route_result["query_params"]);
    }
    ```
    
    SEE ALSO:
    rest_add_route(3), rest_process_route(3), rest_generate_openapi(3)

