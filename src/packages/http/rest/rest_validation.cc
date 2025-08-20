#include "rest_router.h"
#include "../http_parser.h"

/*
 * REST Validation - Request/Response Validation Framework
 * 
 * Provides validation functionality for REST requests and responses,
 * including JSON schema validation, parameter validation, and content
 * type validation. Moved from original rest_handler.cc as part of
 * architecture correction.
 */

#include <regex>

class RestValidator {
private:
    rest_validation_level validation_level_;
    mutable std::string last_error_;
    
public:
    RestValidator() : validation_level_(REST_VALIDATION_BASIC) {}
    
    bool validate_json_request(const HTTPRequest& request) {
        // Check content type
        auto content_type_it = request.headers.find("content-type");
        if (content_type_it != request.headers.end()) {
            const std::string& content_type = content_type_it->second;
            if (content_type.find("application/json") == std::string::npos) {
                last_error_ = "Content-Type must be application/json for JSON requests";
                return false;
            }
        }
        
        // Validate JSON syntax if body exists
        if (!request.body.empty()) {
            return is_valid_json(request.body);
        }
        
        return true;
    }
    
    bool validate_route_parameters(const std::unordered_map<std::string, std::string>& params,
                                  const RestRoute& route) {
        // Basic parameter validation
        for (const std::string& required_param : route.param_names) {
            if (params.find(required_param) == params.end()) {
                last_error_ = "Missing required parameter: " + required_param;
                return false;
            }
        }
        
        return true;
    }
    
    bool validate_query_parameters(const std::string& query_string) {
        // Basic query string validation
        if (query_string.empty()) {
            return true;
        }
        
        // Check for properly formatted key=value pairs
        std::regex query_regex(R"([^&=]+=[^&=]*(&[^&=]+=[^&=]*)*)");
        return std::regex_match(query_string, query_regex);
    }
    
    bool is_valid_json(const std::string& json_str) {
        // Basic JSON validation - count braces and brackets
        int brace_count = 0;
        int bracket_count = 0;
        bool in_string = false;
        bool escaped = false;
        
        for (size_t i = 0; i < json_str.length(); ++i) {
            char c = json_str[i];
            
            if (escaped) {
                escaped = false;
                continue;
            }
            
            if (c == '\\') {
                escaped = true;
                continue;
            }
            
            if (c == '"') {
                in_string = !in_string;
                continue;
            }
            
            if (in_string) {
                continue;
            }
            
            switch (c) {
                case '{':
                    brace_count++;
                    break;
                case '}':
                    brace_count--;
                    if (brace_count < 0) return false;
                    break;
                case '[':
                    bracket_count++;
                    break;
                case ']':
                    bracket_count--;
                    if (bracket_count < 0) return false;
                    break;
            }
        }
        
        return brace_count == 0 && bracket_count == 0 && !in_string;
    }
    
    void set_validation_level(rest_validation_level level) {
        validation_level_ = level;
    }
    
    rest_validation_level get_validation_level() const {
        return validation_level_;
    }
    
    const char* get_last_error() const {
        return last_error_.c_str();
    }
    
    void clear_error() {
        last_error_.clear();
    }
};

// Global validator instance
static RestValidator global_validator;