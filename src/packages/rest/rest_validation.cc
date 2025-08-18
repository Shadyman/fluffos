/**
 * rest_validation.cc - REST validation implementation
 *
 * Data validation against schemas
 */

#include "rest.h"

/**
 * Validate string according to schema
 */
bool rest_validate_string(svalue_t *value, mapping_t *schema) {
    if (value->type != T_STRING) {
        return false;
    }
    
    std::string str_value = value->u.string;
    
    // Check minimum length
    svalue_t *min_length_val = find_string_in_mapping(schema, "minLength");
    if (min_length_val && min_length_val->type == T_NUMBER) {
        if (str_value.length() < static_cast<size_t>(min_length_val->u.number)) {
            return false;
        }
    }
    
    // Check maximum length
    svalue_t *max_length_val = find_string_in_mapping(schema, "maxLength");
    if (max_length_val && max_length_val->type == T_NUMBER) {
        if (str_value.length() > static_cast<size_t>(max_length_val->u.number)) {
            return false;
        }
    }
    
    // Check pattern (basic regex support)
    svalue_t *pattern_val = find_string_in_mapping(schema, "pattern");
    if (pattern_val && pattern_val->type == T_STRING) {
        try {
            std::regex pattern(pattern_val->u.string);
            if (!std::regex_match(str_value, pattern)) {
                return false;
            }
        } catch (const std::exception &e) {
            return false; // Invalid regex
        }
    }
    
    // Check format
    svalue_t *format_val = find_string_in_mapping(schema, "format");
    if (format_val && format_val->type == T_STRING) {
        std::string format = format_val->u.string;
        
        if (format == "email") {
            // Simple email validation
            std::regex email_regex(R"([a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,})");
            if (!std::regex_match(str_value, email_regex)) {
                return false;
            }
        } else if (format == "uri") {
            // Basic URI validation
            if (str_value.find("://") == std::string::npos) {
                return false;
            }
        }
    }
    
    return true;
}

/**
 * Validate number according to schema
 */
bool rest_validate_number(svalue_t *value, mapping_t *schema) {
    if (value->type != T_NUMBER && value->type != T_REAL) {
        return false;
    }
    
    double num_value = (value->type == T_NUMBER) ? value->u.number : value->u.real;
    
    // Check minimum
    svalue_t *minimum_val = find_string_in_mapping(schema, "minimum");
    if (minimum_val && (minimum_val->type == T_NUMBER || minimum_val->type == T_REAL)) {
        double minimum = (minimum_val->type == T_NUMBER) ? minimum_val->u.number : minimum_val->u.real;
        if (num_value < minimum) {
            return false;
        }
    }
    
    // Check maximum
    svalue_t *maximum_val = find_string_in_mapping(schema, "maximum");
    if (maximum_val && (maximum_val->type == T_NUMBER || maximum_val->type == T_REAL)) {
        double maximum = (maximum_val->type == T_NUMBER) ? maximum_val->u.number : maximum_val->u.real;
        if (num_value > maximum) {
            return false;
        }
    }
    
    // Check multiple of
    svalue_t *multiple_of_val = find_string_in_mapping(schema, "multipleOf");
    if (multiple_of_val && (multiple_of_val->type == T_NUMBER || multiple_of_val->type == T_REAL)) {
        double multiple_of = (multiple_of_val->type == T_NUMBER) ? multiple_of_val->u.number : multiple_of_val->u.real;
        if (multiple_of > 0 && fmod(num_value, multiple_of) != 0.0) {
            return false;
        }
    }
    
    return true;
}

/**
 * Validate array according to schema
 */
bool rest_validate_array(svalue_t *value, mapping_t *schema) {
    if (value->type != T_ARRAY) {
        return false;
    }
    
    array_t *arr = value->u.arr;
    
    // Check minimum items
    svalue_t *min_items_val = find_string_in_mapping(schema, "minItems");
    if (min_items_val && min_items_val->type == T_NUMBER) {
        if (arr->size < min_items_val->u.number) {
            return false;
        }
    }
    
    // Check maximum items
    svalue_t *max_items_val = find_string_in_mapping(schema, "maxItems");
    if (max_items_val && max_items_val->type == T_NUMBER) {
        if (arr->size > max_items_val->u.number) {
            return false;
        }
    }
    
    // Validate items (simplified - assumes all items have same schema)
    svalue_t *items_schema_val = find_string_in_mapping(schema, "items");
    if (items_schema_val && items_schema_val->type == T_MAPPING) {
        for (int i = 0; i < arr->size; i++) {
            mapping_t *item_result = rest_validate_impl(&arr->item[i], items_schema_val->u.map);
            
            // Check if validation passed
            svalue_t *valid_val = find_string_in_mapping(item_result, "valid");
            bool item_valid = (valid_val && valid_val->type == T_NUMBER && valid_val->u.number != 0);
            
            free_mapping(item_result);
            
            if (!item_valid) {
                return false;
            }
        }
    }
    
    return true;
}

/**
 * Validate mapping (object) according to schema
 */
bool rest_validate_mapping(svalue_t *value, mapping_t *schema) {
    if (value->type != T_MAPPING) {
        return false;
    }
    
    mapping_t *obj = value->u.map;
    
    // Check required properties
    svalue_t *required_val = find_string_in_mapping(schema, "required");
    if (required_val && required_val->type == T_ARRAY) {
        array_t *required_arr = required_val->u.arr;
        
        for (int i = 0; i < required_arr->size; i++) {
            if (required_arr->item[i].type == T_STRING) {
                svalue_t *prop_val = find_string_in_mapping(obj, required_arr->item[i].u.string);
                if (!prop_val) {
                    return false; // Required property missing
                }
            }
        }
    }
    
    // Validate properties
    svalue_t *properties_val = find_string_in_mapping(schema, "properties");
    if (properties_val && properties_val->type == T_MAPPING) {
        mapping_t *properties = properties_val->u.map;
        
        // Validate each property in the object
        for (int i = 0; i < obj->table_size; i++) {
            mapping_node_t *node = obj->table[i];
            while (node) {
                if (node->values[0].type == T_STRING) {
                    std::string prop_name = node->values[0].u.string;
                    
                    // Find property schema
                    svalue_t *prop_schema_val = find_string_in_mapping(properties, prop_name.c_str());
                    if (prop_schema_val && prop_schema_val->type == T_MAPPING) {
                        mapping_t *prop_result = rest_validate_impl(&node->values[1], prop_schema_val->u.map);
                        
                        // Check if validation passed
                        svalue_t *valid_val = find_string_in_mapping(prop_result, "valid");
                        bool prop_valid = (valid_val && valid_val->type == T_NUMBER && valid_val->u.number != 0);
                        
                        free_mapping(prop_result);
                        
                        if (!prop_valid) {
                            return false;
                        }
                    }
                }
                node = node->next;
            }
        }
    }
    
    return true;
}

/**
 * Validate data implementation
 */
mapping_t *rest_validate_impl(svalue_t *data, mapping_t *schema) {
    if (!data || !schema) {
        mapping_t *result = allocate_mapping(2);
        svalue_t key, value;
        
        key.type = T_STRING; key.u.string = make_shared_string("valid");
        value.type = T_NUMBER; value.u.number = 0;
        svalue_t *entry1 = find_for_insert(result, &key, 0); *entry1 = value;
        free_string(key.u.string);
        
        key.type = T_STRING; key.u.string = make_shared_string("errors");
        value.type = T_ARRAY; value.u.arr = allocate_empty_array(1);
        
        svalue_t error;
        error.type = T_STRING; error.u.string = make_shared_string("Invalid input or schema");
        value.u.arr->item[0] = error;
        
        svalue_t *entry2 = find_for_insert(result, &key, 0); *entry2 = value;
        free_string(key.u.string);
        free_array(value.u.arr);
        
        return result;
    }
    
    // Get expected type from schema
    svalue_t *type_val = find_string_in_mapping(schema, "type");
    if (!type_val || type_val->type != T_STRING) {
        // No type specified - assume valid
        mapping_t *result = allocate_mapping(1);
        svalue_t key, value;
        
        key.type = T_STRING; key.u.string = make_shared_string("valid");
        value.type = T_NUMBER; value.u.number = 1;
        svalue_t *entry3 = find_for_insert(result, &key, 0); *entry3 = value;
        free_string(key.u.string);
        
        return result;
    }
    
    std::string expected_type = type_val->u.string;
    bool is_valid = false;
    
    // Validate based on type
    if (expected_type == "string") {
        is_valid = rest_validate_string(data, schema);
    } else if (expected_type == "number" || expected_type == "integer") {
        is_valid = rest_validate_number(data, schema);
    } else if (expected_type == "array") {
        is_valid = rest_validate_array(data, schema);
    } else if (expected_type == "object") {
        is_valid = rest_validate_mapping(data, schema);
    } else if (expected_type == "boolean") {
        is_valid = (data->type == T_NUMBER); // LPC uses numbers for booleans
    } else {
        is_valid = true; // Unknown type - assume valid
    }
    
    // Create result mapping
    mapping_t *result = allocate_mapping(2);
    svalue_t key, value;
    
    key.type = T_STRING; key.u.string = make_shared_string("valid");
    value.type = T_NUMBER; value.u.number = is_valid ? 1 : 0;
    svalue_t *entry4 = find_for_insert(result, &key, 0); *entry4 = value;
    free_string(key.u.string);
    
    // Add errors array (simplified)
    key.type = T_STRING; key.u.string = make_shared_string("errors");
    
    if (is_valid) {
        value.type = T_ARRAY; value.u.arr = allocate_empty_array(0);
    } else {
        value.type = T_ARRAY; value.u.arr = allocate_empty_array(1);
        
        svalue_t error;
        error.type = T_STRING; 
        error.u.string = make_shared_string(("Validation failed for type: " + expected_type).c_str());
        value.u.arr->item[0] = error;
    }
    
    svalue_t *entry5 = find_for_insert(result, &key, 0); *entry5 = value;
    free_string(key.u.string);
    free_array(value.u.arr);
    
    return result;
}