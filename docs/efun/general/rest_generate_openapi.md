---
layout: doc
title: general / rest_generate_openapi
---
# rest_generate_openapi

### NAME

    rest_generate_openapi

### SYNOPSIS

    mapping rest_generate_openapi(int, mapping)

### DESCRIPTION

    Generate OpenAPI 3.0 specification for REST routes using PACKAGE_OPENAPI.

    Creates comprehensive OpenAPI documentation by analyzing REST router routes and 
    generating a complete API specification with schemas and examples. Generated 
    documentation includes API information and metadata, all registered routes with 
    parameters, request/response schemas, security definitions with JWT Bearer tokens, 
    and interactive documentation support.

### EXAMPLES

    // Generate API documentation
    mapping api_info = ([
        "title": "My REST API",
        "version": "1.0.0", 
        "description": "A sample REST API built with FluffOS"
    ]);
    
    mapping spec = rest_generate_openapi(router_id, api_info);
    string json_spec = json_encode(spec);
    
    // Serve the specification
    http_send_response(request_id, ([
        "status": 200,
        "headers": (["Content-Type": "application/json"]),
        "body": json_spec
    ]));

### SEE ALSO

    rest_serve_docs(3), rest_set_route_docs(3), rest_create_router(3)

