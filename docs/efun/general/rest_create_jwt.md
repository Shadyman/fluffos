---
layout: doc
title: general / rest_create_jwt
---
# rest_create_jwt

### NAME

    rest_create_jwt

### SYNOPSIS

    string rest_create_jwt(mapping, string)

### DESCRIPTION

    Create a JSON Web Token (JWT) for authentication using PACKAGE_REST.

    Generates a JWT token with HMAC-SHA256 signature for stateless authentication.
    The token contains encoded claims about the authenticated entity. Standard JWT 
    claims include "sub" (subject), "iat" (issued at), "exp" (expires), "aud" (audience), 
    and "iss" (issuer). Returns the JWT token string on success, or 0 on failure.

### EXAMPLES

    // Create user authentication token
    mapping user_claims = ([
        "sub": "user_12345",
        "name": "Alice Johnson", 
        "role": "admin",
        "exp": time() + 3600  // Expires in 1 hour
    ]);
    
    string jwt_token = rest_create_jwt(user_claims, SECRET_KEY);
    if (!jwt_token) {
        error("Failed to create JWT token");
    }

### SEE ALSO

    rest_verify_jwt(3), rest_validate_schema(3)

