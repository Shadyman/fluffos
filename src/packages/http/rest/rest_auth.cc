#include "rest_router.h"
#include "../http_parser.h"

/*
 * REST Authentication - Authentication and Authorization Framework
 * 
 * Provides authentication and authorization functionality for REST APIs,
 * including JWT token validation, API key authentication, and role-based
 * access control. Moved from original rest_handler.cc as part of
 * architecture correction.
 */

#include <string>
#include <unordered_map>

class RestAuthenticator {
private:
    std::string jwt_secret_;
    std::unordered_map<std::string, std::string> api_keys_;
    mutable std::string last_error_;
    
public:
    RestAuthenticator() = default;
    
    bool authenticate_request(const HTTPRequest& request) {
        // Check for Authorization header
        auto auth_it = request.headers.find("authorization");
        if (auth_it == request.headers.end()) {
            last_error_ = "Missing Authorization header";
            return false;
        }
        
        const std::string& auth_header = auth_it->second;
        
        // Check for Bearer token (JWT)
        if (auth_header.find("Bearer ") == 0) {
            std::string token = auth_header.substr(7);
            return validate_jwt_token(token);
        }
        
        // Check for API key
        if (auth_header.find("ApiKey ") == 0) {
            std::string api_key = auth_header.substr(7);
            return validate_api_key(api_key);
        }
        
        // Check for Basic authentication
        if (auth_header.find("Basic ") == 0) {
            std::string credentials = auth_header.substr(6);
            return validate_basic_auth(credentials);
        }
        
        last_error_ = "Unsupported authentication method";
        return false;
    }
    
    bool validate_jwt_token(const std::string& token) {
        if (jwt_secret_.empty()) {
            last_error_ = "JWT secret not configured";
            return false;
        }
        
        // Basic JWT validation - in a real implementation, this would
        // properly decode and verify the JWT signature
        if (token.empty() || token.find('.') == std::string::npos) {
            last_error_ = "Invalid JWT token format";
            return false;
        }
        
        // TODO: Implement proper JWT validation with signature verification
        return true;
    }
    
    bool validate_api_key(const std::string& api_key) {
        if (api_keys_.empty()) {
            last_error_ = "No API keys configured";
            return false;
        }
        
        auto it = api_keys_.find(api_key);
        if (it == api_keys_.end()) {
            last_error_ = "Invalid API key";
            return false;
        }
        
        return true;
    }
    
    bool validate_basic_auth(const std::string& credentials) {
        // TODO: Implement Basic authentication validation
        // This would involve base64 decoding and user/password verification
        last_error_ = "Basic authentication not yet implemented";
        return false;
    }
    
    void set_jwt_secret(const std::string& secret) {
        jwt_secret_ = secret;
    }
    
    void add_api_key(const std::string& key, const std::string& description = "") {
        api_keys_[key] = description;
    }
    
    void remove_api_key(const std::string& key) {
        api_keys_.erase(key);
    }
    
    bool is_route_protected(const RestRoute& route) {
        return route.requires_auth;
    }
    
    const char* get_last_error() const {
        return last_error_.c_str();
    }
    
    void clear_error() {
        last_error_.clear();
    }
};

// Global authenticator instance
static RestAuthenticator global_authenticator;