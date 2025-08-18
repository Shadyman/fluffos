---
layout: doc
title: general / http_server_start
---
# http_server_start

### NAME

    http_server_start

### SYNOPSIS

    int http_server_start(int, string | function, mapping)

### DESCRIPTION

    Start an HTTP server on the specified port using PACKAGE_HTTP.

    Creates an HTTP server instance that integrates with FluffOS's libevent2 event
    system and libwebsockets for handling HTTP/1.1 and HTTP/2 connections with
    optional TLS/SSL support.
    
    PARAMETERS:
    - port (int): Port number to bind to (1-65535)
    - callback (string | function): Function to call when requests are received
    - options (mapping): Server configuration options
    
    The options mapping may contain:
    - "ssl_cert": Path to SSL certificate file for HTTPS
    - "ssl_key": Path to SSL private key file  
    - "interface": Network interface to bind to (default: all interfaces)
    
    RETURNS:
    - int: Server ID (positive integer) on success, -1 on failure
    
    The callback function receives a mapping with request details:
    - "id": Unique request identifier for http_response_send()
    - "method": HTTP method (GET, POST, PUT, DELETE, etc.)
    - "uri": Complete request URI including query parameters
    - "headers": Mapping of HTTP request headers
    - "body": Request body content (if any)
    
    EXAMPLES:
    ```lpc
    void handle_request(mapping request) {
        mapping response = ([
            "status": 200,
            "headers": (["Content-Type": "text/plain"]),
            "body": "Hello from FluffOS HTTP server"
        ]);
        http_response_send(request["id"], response);
    }
    
    // Start basic HTTP server
    int server_id = http_server_start(8080, (: handle_request :), ([]));
    if (server_id == -1) {
        error("Failed to start HTTP server on port 8080");
    }
    
    // Start HTTPS server with SSL
    mapping ssl_config = ([
        "ssl_cert": "/etc/ssl/certs/server.crt",
        "ssl_key": "/etc/ssl/private/server.key"
    ]);
    int https_id = http_server_start(443, (: handle_request :), ssl_config);
    ```
    
    SEE ALSO:
    http_response_send(3), http_server_stop(3)
