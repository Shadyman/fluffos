/*
 * GraphQL Schema Implementation
 * 
 * Handles GraphQL schema loading, validation, and introspection.
 */

#include "graphql.h"
#include <fstream>
#include <sstream>
#include <regex>

/*
 * GraphQL Schema Implementation
 */

GraphQLSchema::GraphQLSchema() : valid_(false) {
    GRAPHQL_DEBUG("Creating GraphQL schema instance");
}

GraphQLSchema::~GraphQLSchema() {
    GRAPHQL_DEBUG("Destroying GraphQL schema instance");
}

bool GraphQLSchema::load_from_string(const std::string& schema_text) {
    schema_text_ = schema_text;
    validation_errors_.clear();
    types_.clear();
    
    GRAPHQL_DEBUG("Loading schema from string");
    
    if (schema_text.empty()) {
        validation_errors_.push_back("Schema text is empty");
        valid_ = false;
        return false;
    }
    
    // Parse and validate the schema
    if (!parse_schema()) {
        valid_ = false;
        return false;
    }
    
    valid_ = validate();
    GRAPHQL_DEBUG_F("Schema loaded, valid: %s", valid_ ? "true" : "false");
    return valid_;
}

bool GraphQLSchema::load_from_file(const std::string& file_path) {
    GRAPHQL_DEBUG_F("Loading schema from file: %s", file_path.c_str());
    
    std::ifstream file(file_path);
    if (!file.is_open()) {
        validation_errors_.push_back("Failed to open schema file: " + file_path);
        valid_ = false;
        return false;
    }
    
    std::ostringstream buffer;
    buffer << file.rdbuf();
    file.close();
    
    return load_from_string(buffer.str());
}

bool GraphQLSchema::validate() {
    validation_errors_.clear();
    
    GRAPHQL_DEBUG("Validating GraphQL schema");
    
    // Check for required root types
    bool has_query = types_.find("Query") != types_.end();
    if (!has_query) {
        validation_errors_.push_back("Schema must define a Query type");
    }
    
    // Validate type references
    validate_type_references();
    
    bool is_valid = validation_errors_.empty();
    GRAPHQL_DEBUG_F("Schema validation complete, valid: %s", is_valid ? "true" : "false");
    
    if (!is_valid) {
        for (const auto& error : validation_errors_) {
            GRAPHQL_DEBUG_F("Validation error: %s", error.c_str());
        }
    }
    
    return is_valid;
}

std::vector<std::string> GraphQLSchema::get_validation_errors() const {
    return validation_errors_;
}

std::string GraphQLSchema::get_introspection_query() const {
    if (!valid_) {
        return "{\"errors\": [{\"message\": \"Schema is not valid\"}]}";
    }
    
    GRAPHQL_DEBUG("Generating introspection response");
    
    // Generate basic introspection response
    std::ostringstream oss;
    oss << "{\"data\": {\"__schema\": {";
    oss << "\"queryType\": {\"name\": \"Query\"},";
    oss << "\"mutationType\": " << (types_.find("Mutation") != types_.end() ? "{\"name\": \"Mutation\"}" : "null") << ",";
    oss << "\"subscriptionType\": " << (types_.find("Subscription") != types_.end() ? "{\"name\": \"Subscription\"}" : "null") << ",";
    oss << "\"types\": [";
    
    bool first = true;
    for (const auto& type_pair : types_) {
        if (!first) oss << ",";
        first = false;
        
        oss << "{";
        oss << "\"name\": \"" << type_pair.first << "\",";
        oss << "\"kind\": \"OBJECT\",";
        oss << "\"description\": null,";
        oss << "\"fields\": [";
        
        bool first_field = true;
        for (const auto& field_pair : type_pair.second) {
            if (!first_field) oss << ",";
            first_field = false;
            
            oss << "{";
            oss << "\"name\": \"" << field_pair.first << "\",";
            oss << "\"description\": null,";
            oss << "\"args\": [],";
            oss << "\"type\": {\"name\": \"" << field_pair.second << "\", \"kind\": \"SCALAR\"},";
            oss << "\"isDeprecated\": false,";
            oss << "\"deprecationReason\": null";
            oss << "}";
        }
        
        oss << "],";
        oss << "\"interfaces\": [],";
        oss << "\"possibleTypes\": null,";
        oss << "\"enumValues\": null,";
        oss << "\"inputFields\": null";
        oss << "}";
    }
    
    oss << "],";
    oss << "\"directives\": []";
    oss << "}}}";
    
    return oss.str();
}

std::string GraphQLSchema::get_schema_sdl() const {
    return schema_text_;
}

bool GraphQLSchema::has_type(const std::string& type_name) const {
    return types_.find(type_name) != types_.end();
}

std::vector<std::string> GraphQLSchema::get_type_fields(const std::string& type_name) const {
    std::vector<std::string> fields;
    
    auto type_it = types_.find(type_name);
    if (type_it != types_.end()) {
        for (const auto& field_pair : type_it->second) {
            fields.push_back(field_pair.first);
        }
    }
    
    return fields;
}

std::string GraphQLSchema::get_field_type(const std::string& type_name, const std::string& field_name) const {
    auto type_it = types_.find(type_name);
    if (type_it != types_.end()) {
        auto field_it = type_it->second.find(field_name);
        if (field_it != type_it->second.end()) {
            return field_it->second;
        }
    }
    
    return "";
}

int GraphQLSchema::calculate_query_depth(const std::string& query) const {
    int depth = 0;
    int max_depth = 0;
    
    for (char c : query) {
        if (c == '{') {
            depth++;
            max_depth = std::max(max_depth, depth);
        } else if (c == '}') {
            depth--;
        }
    }
    
    GRAPHQL_DEBUG_F("Calculated query depth: %d", max_depth);
    return max_depth;
}

int GraphQLSchema::calculate_query_complexity(const std::string& query) const {
    // Simple complexity calculation - count field selections
    int complexity = 0;
    
    // Use regex to find field selections
    std::regex field_pattern(R"(\b[a-zA-Z_][a-zA-Z0-9_]*\s*(?:\(.*?\))?\s*(?:\{|:))");
    std::sregex_iterator iter(query.begin(), query.end(), field_pattern);
    std::sregex_iterator end;
    
    while (iter != end) {
        complexity++;
        ++iter;
    }
    
    GRAPHQL_DEBUG_F("Calculated query complexity: %d", complexity);
    return complexity;
}

bool GraphQLSchema::parse_schema() {
    GRAPHQL_DEBUG("Parsing GraphQL schema");
    
    try {
        extract_types();
        return true;
    } catch (const std::exception& e) {
        validation_errors_.push_back("Schema parsing error: " + std::string(e.what()));
        return false;
    }
}

void GraphQLSchema::extract_types() {
    // Simple regex-based type extraction
    // In a real implementation, this would use a proper GraphQL parser
    
    std::regex type_pattern(R"(type\s+(\w+)\s*\{([^}]*)\})");
    std::sregex_iterator type_iter(schema_text_.begin(), schema_text_.end(), type_pattern);
    std::sregex_iterator end;
    
    while (type_iter != end) {
        std::string type_name = (*type_iter)[1].str();
        std::string type_body = (*type_iter)[2].str();
        
        GRAPHQL_DEBUG_F("Found type: %s", type_name.c_str());
        
        // Extract fields from type body
        std::map<std::string, std::string> fields;
        std::regex field_pattern(R"((\w+)\s*:\s*(\w+[!]?))");
        std::sregex_iterator field_iter(type_body.begin(), type_body.end(), field_pattern);
        
        while (field_iter != end) {
            std::string field_name = (*field_iter)[1].str();
            std::string field_type = (*field_iter)[2].str();
            
            fields[field_name] = field_type;
            GRAPHQL_DEBUG_F("  Field: %s -> %s", field_name.c_str(), field_type.c_str());
            
            ++field_iter;
        }
        
        types_[type_name] = fields;
        ++type_iter;
    }
    
    // If no types found, create a default Query type
    if (types_.empty()) {
        GRAPHQL_DEBUG("No types found, creating default Query type");
        types_["Query"]["hello"] = "String";
    }
}

void GraphQLSchema::validate_type_references() {
    GRAPHQL_DEBUG("Validating type references");
    
    // Define built-in scalar types
    std::set<std::string> builtin_types = {
        "String", "Int", "Float", "Boolean", "ID"
    };
    
    for (const auto& type_pair : types_) {
        const std::string& type_name = type_pair.first;
        const auto& fields = type_pair.second;
        
        for (const auto& field_pair : fields) {
            const std::string& field_name = field_pair.first;
            std::string field_type = field_pair.second;
            
            // Remove non-null indicator
            if (!field_type.empty() && field_type.back() == '!') {
                field_type.pop_back();
            }
            
            // Check if type exists
            if (builtin_types.find(field_type) == builtin_types.end() &&
                types_.find(field_type) == types_.end()) {
                
                std::string error = "Field '" + field_name + "' in type '" + type_name + 
                                  "' references unknown type '" + field_type + "'";
                validation_errors_.push_back(error);
                
                GRAPHQL_DEBUG_F("Type reference error: %s", error.c_str());
            }
        }
    }
}