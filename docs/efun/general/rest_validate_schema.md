---
layout: doc
title: general / rest_validate_schema
---
# rest_validate_schema

### NAME

    rest_validate_schema

### SYNOPSIS

    mapping rest_validate_schema(mixed, mapping)

### DESCRIPTION

    Validate data against a JSON schema using PACKAGE_REST.

    Performs comprehensive data validation using JSON Schema-like rules. Supports 
    validation of strings, numbers, arrays, objects, and nested structures. Returns 
    a validation result mapping containing "valid" flag and "errors" array. Supported 
    validation rules include string length and patterns, number ranges, array size, 
    and object property requirements.

### EXAMPLES

    // Validate user registration data
    mapping user_schema = ([
        "type": "object",
        "required": ({"username", "email", "password"}),
        "properties": ([
            "username": (["type": "string", "minLength": 3, "maxLength": 20]),
            "email": (["type": "string", "format": "email"]),
            "password": (["type": "string", "minLength": 8]),
            "age": (["type": "number", "minimum": 13])
        ])
    ]);
    
    mapping validation_result = rest_validate_schema(user_data, user_schema);
    if (!validation_result["valid"]) {
        // Send validation errors to client
        http_send_response(request_id, (["status": 400, "body": json_encode(validation_result["errors"])]));
    }

### SEE ALSO

    rest_create_jwt(3), rest_verify_jwt(3), rest_format_response(3)

