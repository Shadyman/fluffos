---
layout: doc
title: general / rest_verify_jwt
---
# rest_verify_jwt

### NAME

    rest_verify_jwt

### SYNOPSIS

    mapping rest_verify_jwt(string, string)

### DESCRIPTION

    Verify and decode a JSON Web Token (JWT) using PACKAGE_REST.

    Validates the JWT signature and expiration, returning the decoded payload if valid.
    Uses HMAC-SHA256 for signature verification. Verification includes token format 
    validation, HMAC-SHA256 signature verification, expiration time checking, and 
    token structure validation. Returns decoded JWT payload mapping on success, 
    or empty mapping on failure.

### EXAMPLES

    // Verify token from Authorization header
    string auth_header = request["headers"]["Authorization"];
    string token;
    if (sscanf(auth_header, "Bearer %s", token) == 1) {
        mapping claims = rest_verify_jwt(token, SECRET_KEY);
        if (!sizeof(claims)) {
            // Invalid or expired token
            http_send_response(request["id"], (["status": 401]));
            return;
        }
        
        string user_id = claims["sub"];
        string user_role = claims["role"];
        // Continue with authenticated request
    }

### SEE ALSO

    rest_create_jwt(3), rest_validate_schema(3)

