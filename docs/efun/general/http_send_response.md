---
layout: doc
title: general / http_send_response
---
# http_send_response

### NAME

    http_send_response

### SYNOPSIS

    int http_send_response(int, mapping)

### DESCRIPTION

    Send an HTTP response to a client request using PACKAGE_HTTP.

    Completes an HTTP transaction by sending the response back to the client.
    This function must be called for every request received through http_start_server().
    
    PARAMETERS:
    - request_id (int): Request identifier from the callback mapping
    - response (mapping): Response data containing status, headers, and body
    
    The response mapping must contain:
    - "status" (int): HTTP status code (200, 404, 500, etc.)
    - "headers" (mapping): HTTP response headers
    - "body" (string): Response body content
    
    RETURNS:
    - int: 1 on successful send, 0 on failure
    
    EXAMPLES:
    ```lpc
    // Send JSON response
    mapping json_response = ([
        "status": 200,
        "headers": (["Content-Type": "application/json"]),
        "body": json_encode((["message": "Success", "data": users_data]))
    ]);
    http_send_response(request_id, json_response);
    
    // Send error response
    mapping error_response = ([
        "status": 404,
        "headers": (["Content-Type": "text/plain"]),
        "body": "Resource not found"
    ]);
    http_send_response(request_id, error_response);
    
    // Send redirect response
    mapping redirect = ([
        "status": 302,
        "headers": (["Location": "/new-path"]),
        "body": ""
    ]);
    http_send_response(request_id, redirect);
    ```
    
    SEE ALSO:
    http_start_server(3), http_stop_server(3)

