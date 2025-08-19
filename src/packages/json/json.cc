/**
 * json.cc
 *
 * JSON encoding and decoding package for FluffOS
 *
 * Provides efuns for working with JSON data format, enabling easy
 * serialization and deserialization of LPC data structures to/from JSON.
 * Uses the modern nlohmann::json library for robust JSON handling.
 *
 * Supported efuns:
 * - json_encode(mixed data): Convert LPC data to JSON string
 * - json_decode(string json): Parse JSON string to LPC data
 * - json_valid(string json): Check if a string is valid JSON
 * - json_pretty(mixed data, int indent): Pretty-print JSON with indentation
 * - json_get(string json, string pointer): Extract value using JSON pointer
 *
 * Type mapping:
 * LPC -> JSON:            JSON -> LPC:
 * int/float -> number     number -> int/float
 * string -> string        string -> string
 * array -> array          array -> array
 * mapping -> object       object -> mapping
 * buffer -> array(int)    boolean -> int (0/1)
 * other -> null           null -> int (0)
 *
 * Requirements:
 * - nlohmann::json library (header-only, included in FluffOS)
 * - PACKAGE_JSON enabled during compilation
 *
 * -- M. Lange for FluffOS project 27 Jul 2025 (Copilot assisted)
 */

#include "base/package_api.h"

#include <sys/stat.h>  // for struct stat
#include <nlohmann/json.hpp>

/**
 * Convert an LPC svalue to standard JSON format
 *
 * Recursively converts LPC data structures to nlohmann::json objects.
 * Handles all basic LPC types with appropriate JSON equivalents.
 * Non-string mapping keys are skipped with a warning since JSON
 * objects require string keys.
 *
 * @param sv Pointer to the svalue to convert
 * @return nlohmann::json object representing the data
 */
nlohmann::json svalue_to_standard_json(const svalue_t* sv) {
  switch (sv->type) {
    case T_NUMBER:
      // Convert LPC integers directly to JSON numbers
      return sv->u.number;
    case T_REAL:
      // Convert LPC floats directly to JSON numbers
      return sv->u.real;
    case T_STRING:
      // Convert LPC strings to JSON strings using std::string constructor
      return std::string(sv->u.string);
    case T_CLASS:
      // Classes are treated as arrays in JSON (fall through to T_ARRAY)
      // fall through
    case T_ARRAY: {
      // Convert LPC arrays to JSON arrays
      nlohmann::json arr = nlohmann::json::array();
      for (int i = 0; i < sv->u.arr->size; i++) {
        // Recursively convert each array element
        arr.push_back(svalue_to_standard_json(&sv->u.arr->item[i]));
      }
      return arr;
    }
    case T_MAPPING: {
      // Convert LPC mappings to JSON objects
      nlohmann::json obj = nlohmann::json::object();
      // Iterate through the mapping's hash table
      for (int i = 0; i < sv->u.map->table_size; i++) {
        // Walk the collision chain for this hash bucket
        for (auto* node = sv->u.map->table[i]; node; node = node->next) {
          auto key = &node->values[0];  // Mapping key
          auto val = &node->values[1];  // Mapping value
          if (key->type == T_STRING) {
            // Only string keys are valid in JSON objects
            obj[key->u.string] = svalue_to_standard_json(val);
          } else {
            // Non-string keys are not valid in JSON, log a warning and skip them
            fprintf(stderr, "Warning: Non-string key encountered in mapping during JSON encoding. Key will be skipped.\n");
            continue;
          }
        }
      }
      return obj;
    }
    case T_BUFFER: {
      // Convert LPC buffers to JSON arrays of integers
      nlohmann::json arr = nlohmann::json::array();
      for (unsigned int i = 0; i < sv->u.buf->size; i++) {
        // Each byte becomes an integer in the JSON array
        arr.push_back(static_cast<int>(sv->u.buf->item[i]));
      }
      return arr;
    }
    default:
      // For objects, functions, and other non-serializable types, return null
      // This includes T_OBJECT, T_FUNCTION, etc.
      return nullptr;
  }
}

/**
 * Convert standard JSON format to an LPC svalue
 *
 * Recursively converts nlohmann::json objects to LPC data structures.
 * Allocates appropriate memory for strings, arrays, and mappings.
 * Boolean values are converted to integers (0/1).
 *
 * @param j The JSON object to convert
 * @return svalue_t containing the converted LPC data
 */
svalue_t standard_json_to_svalue(const nlohmann::json& j) {
  svalue_t sv = {};

  if (j.is_null()) {
    // JSON null becomes LPC integer 0
    sv.type = T_NUMBER;
    sv.u.number = 0;
  } else if (j.is_boolean()) {
    // JSON boolean becomes LPC integer (true=1, false=0)
    sv.type = T_NUMBER;
    sv.u.number = j.get<bool>() ? 1 : 0;
  } else if (j.is_number_integer()) {
    // JSON integer becomes LPC integer
    sv.type = T_NUMBER;
    sv.u.number = j.get<LPC_INT>();
  } else if (j.is_number_float()) {
    // JSON float becomes LPC real/float
    sv.type = T_REAL;
    sv.u.real = j.get<LPC_FLOAT>();
  } else if (j.is_string()) {
    // JSON string becomes LPC string (malloc'd copy)
    sv.type = T_STRING;
    sv.subtype = STRING_MALLOC;
    sv.u.string = string_copy(j.get<std::string>().c_str(), "json_decode: string");
  } else if (j.is_array()) {
    // JSON array becomes LPC array
    sv.type = T_ARRAY;
    sv.u.arr = allocate_array(j.size());
    // Recursively convert each array element
    for (size_t i = 0; i < j.size(); i++) {
      auto sv_item = standard_json_to_svalue(j[i]);
      assign_svalue_no_free(&sv.u.arr->item[i], &sv_item);
      free_svalue(&sv_item, "json_decode: array item");
    }
  } else if (j.is_object()) {
    // Convert JSON object to LPC mapping
    auto size = j.size();
    // Create parallel arrays for keys and values
    array_t* map_keys = allocate_array(size);
    array_t* map_values = allocate_array(size);

    int idx = 0;
    // Iterate through JSON object key-value pairs
    for (auto it = j.begin(); it != j.end(); ++it, ++idx) {
      // Convert the key (always a string in JSON)
      map_keys->item[idx].type = T_STRING;
      map_keys->item[idx].subtype = STRING_MALLOC;
      map_keys->item[idx].u.string = string_copy(it.key().c_str(), "json_decode: mapping key");

      // Convert the value (recursively)
      auto sv_val = standard_json_to_svalue(it.value());
      assign_svalue_no_free(&map_values->item[idx], &sv_val);
      free_svalue(&sv_val, "json_decode: mapping value");
    }

    // Create the LPC mapping from the key-value arrays
    sv.type = T_MAPPING;
    sv.u.map = mkmapping(map_keys, map_values);
    // Clean up temporary arrays (mkmapping copies the data)
    free_array(map_keys);
    free_array(map_values);
  } else {
    // Unknown/unsupported JSON type, return integer 0
    sv.type = T_NUMBER;
    sv.u.number = 0;
  }

  return sv;
}

#ifdef F_JSON_ENCODE
/**
 * json_encode(mixed data) - Convert LPC data to JSON string
 *
 * Serializes any LPC data structure to a compact JSON string.
 * Throws an error if the data cannot be serialized (e.g., circular references).
 */
void f_json_encode() {
  try {
    // Convert the LPC data (top of stack) to JSON object
    nlohmann::json j = svalue_to_standard_json(sp);
    // Serialize JSON to compact string format
    std::string json_str = j.dump();
    // Remove input from stack and push result
    pop_stack();
    push_malloced_string(string_copy(json_str.c_str(), "json_encode"));
  } catch (const std::exception& e) {
    // Clean up stack and report error
    pop_stack();
    error("json_encode: %s\n", e.what());
  }
}
#endif

#ifdef F_JSON_DECODE
/**
 * json_decode(string json) - Parse JSON string to LPC data
 *
 * Parses a JSON string and converts it to appropriate LPC data structures.
 * Throws an error if the JSON is malformed or cannot be parsed.
 */
void f_json_decode() {
  try {
    // Get JSON string from stack
    const char* json_str = sp->u.string;
    // Parse the JSON string
    nlohmann::json j = nlohmann::json::parse(json_str);
    // Convert JSON object to LPC data structure
    svalue_t result = standard_json_to_svalue(j);
    // Remove input from stack and push result
    pop_stack();
    push_svalue(&result);
    // Clean up temporary result (push_svalue made a copy)
    free_svalue(&result, "json_decode: result");
  } catch (const nlohmann::json::parse_error& e) {
    // Handle JSON parsing errors specifically
    pop_stack();
    error("json_decode: Parse error: %s\n", e.what());
  } catch (const std::exception& e) {
    // Handle any other errors
    pop_stack();
    error("json_decode: %s\n", e.what());
  }
}
#endif

#ifdef F_JSON_VALID
/**
 * json_valid(string json) - Check if string is valid JSON
 *
 * Returns 1 if the string is valid JSON, 0 otherwise.
 * Does not throw errors - provides a safe way to test JSON validity.
 */
void f_json_valid() {
  try {
    // Get JSON string from stack
    const char* json_str = sp->u.string;
    // Attempt to parse - if successful, JSON is valid
    nlohmann::json::parse(json_str);
    pop_stack();
    push_number(1);  // Valid JSON
  } catch (const nlohmann::json::parse_error& e) {
    // JSON parsing failed - invalid JSON
    pop_stack();
    push_number(0);  // Invalid JSON
  } catch (const std::exception& e) {
    // Any other error - treat as invalid
    pop_stack();
    push_number(0);  // Invalid JSON
  }
}
#endif

#ifdef F_JSON_PRETTY
/**
 * json_pretty(mixed data, int indent) - Pretty-print JSON with indentation
 *
 * Converts LPC data to a nicely formatted JSON string with proper indentation.
 * Default indent is 2 spaces if not specified.
 *
 * @param data The LPC data to serialize
 * @param indent Number of spaces for indentation (optional, default: 2)
 */
void f_json_pretty() {
  try {
    int indent = 2;  // Default indentation level
    // Check if indent parameter was provided
    if (st_num_arg == 2) {
      indent = sp->u.number;  // Get user-specified indent
      pop_stack();  // Remove indent parameter from stack
    }

    // Convert LPC data to JSON object
    nlohmann::json j = svalue_to_standard_json(sp);
    // Format JSON with specified indentation
    std::string json_str = j.dump(indent);
    // Remove input data from stack and push formatted result
    pop_stack();
    push_malloced_string(string_copy(json_str.c_str(), "json_pretty"));
  } catch (const std::exception& e) {
    // Clean up all arguments and report error
    pop_n_elems(st_num_arg);
    error("json_pretty: %s\n", e.what());
  }
}
#endif

#ifdef F_JSON_GET
/**
 * json_get(string json, string pointer) - Extract value using JSON pointer
 *
 * Uses JSON Pointer (RFC 6901) syntax to extract a specific value from JSON.
 * Examples: "/foo/bar", "/array/0", "/nested/object/key"
 * Throws an error if the pointer is invalid or path not found.
 *
 * @param json The JSON string to query
 * @param pointer JSON pointer path (e.g., "/path/to/value")
 */
void f_json_get() {
  try {
    // Get arguments from stack: sp = pointer, sp-1 = json_string
    const char* json_pointer = sp->u.string;
    const char* json_str = (sp - 1)->u.string;

    // Parse the JSON string first
    nlohmann::json j = nlohmann::json::parse(json_str);

    nlohmann::json result;
    try {
      // Use JSON Pointer to extract the specified value
      result = j[nlohmann::json::json_pointer(json_pointer)];
    } catch (const nlohmann::json::parse_error& e) {
      // Invalid JSON pointer syntax
      pop_n_elems(2);
      error("json_get: Invalid JSON pointer syntax: %s\n", e.what());
    }

    // Convert result back to LPC data structure
    svalue_t sv_result = standard_json_to_svalue(result);
    pop_n_elems(2);  // Remove both arguments
    push_svalue(&sv_result);  // Push result
    free_svalue(&sv_result, "json_get: result");  // Clean up temp result
  } catch (const nlohmann::json::parse_error& e) {
    // JSON parsing failed
    pop_n_elems(2);
    error("json_get: Parse error: %s\n", e.what());
  } catch (const nlohmann::json::out_of_range& e) {
    // JSON pointer path not found
    pop_n_elems(2);
    error("json_get: Path not found: %s\n", e.what());
  } catch (const std::exception& e) {
    // Any other error
    pop_n_elems(2);
    error("json_get: %s\n", e.what());
  }
}
#endif
