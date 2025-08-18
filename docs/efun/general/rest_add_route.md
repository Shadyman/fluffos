---
layout: doc
title: general / rest_add_route
---
# rest_add_route

### NAME

    rest_add_route

### SYNOPSIS

    int rest_add_route(int, string, string, string | function)

### DESCRIPTION

    Add a route pattern to a REST router using PACKAGE_REST.

    Registers a URL pattern with an HTTP method and handler function.
    Supports parameter extraction using {parameter_name} syntax in the URL pattern.
    URL patterns support parameter extraction like "/users/{id}" which extracts 
    the id parameter, or "/users/{user_id}/posts/{post_id}" for multiple parameters.

### EXAMPLES

    int router = rest_create_router();
    
    // Add various route patterns
    rest_add_route(router, "GET", "/users", (: list_users :));
    rest_add_route(router, "GET", "/users/{id}", (: get_user :));
    rest_add_route(router, "POST", "/users", (: create_user :));
    rest_add_route(router, "PUT", "/users/{id}", (: update_user :));
    rest_add_route(router, "DELETE", "/users/{id}", (: delete_user :));
    
    // Nested resource patterns
    rest_add_route(router, "GET", "/users/{user_id}/posts/{post_id}", (: get_specific_post :));

### SEE ALSO

    rest_create_router(3), rest_process_route(3), rest_set_route_docs(3)

