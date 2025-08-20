#ifndef SOCKET_OPTION_MANAGER_H_
#define SOCKET_OPTION_MANAGER_H_

#include "packages/sockets/socket_options.h"
#include "base/package_api.h"

#include <unordered_map>
#include <string>
#include <memory>

/*
 * SocketOptionManager - Unified Socket Option Management System
 * 
 * This class provides comprehensive socket option management for the
 * FluffOS unified socket architecture. It supports all option types
 * defined in socket_options.h with validation, type conversion, and
 * security controls.
 * 
 * Features:
 * - Dynamic option storage with efficient access
 * - Type validation and conversion
 * - Category-based organization
 * - Access level security
 * - Default value handling
 * - Backward compatibility with existing socket system
 */

struct SocketOptionDescriptor {
    socket_option_type type;
    socket_option_category category;
    socket_option_access access_level;
    svalue_t default_value;
    bool has_constraints;
    
    // Constraint values (when applicable)
    union {
        struct {
            long min_val;
            long max_val;
        } integer_constraints;
        struct {
            size_t min_length;
            size_t max_length;
        } string_constraints;
        struct {
            double min_val;
            double max_val;
        } float_constraints;
    };
    
    const char* description;
    const char* validation_regex;  // For string validation
};

class SocketOptionManager {
private:
    // Option storage: maps socket_options enum values to svalue_t
    std::unordered_map<int, svalue_t> options_;
    
    // Option descriptors for validation and metadata
    static std::unordered_map<int, SocketOptionDescriptor> option_descriptors_;
    static bool descriptors_initialized_;
    
    // Socket ID for security checks
    int socket_id_;
    
    // Helper methods
    void initialize_descriptors();
    bool validate_option_value(int option, const svalue_t* value) const;
    bool has_access_permission(int option, object_t* caller) const;
    void convert_to_type(svalue_t* value, socket_option_type target_type);
    svalue_t get_default_value(int option);
    
public:
    SocketOptionManager(int socket_id);
    ~SocketOptionManager();
    
    // Core option management
    bool set_option(int option, const svalue_t* value, object_t* caller = nullptr);
    bool get_option(int option, svalue_t* result, object_t* caller = nullptr);
    bool has_option(int option) const;
    bool remove_option(int option, object_t* caller = nullptr);
    
    // Option queries and metadata
    socket_option_type get_option_type(int option) const;
    socket_option_category get_option_category(int option) const;
    socket_option_access get_access_level(int option) const;
    const char* get_option_description(int option) const;
    
    // Validation
    bool validate_option(int option, const svalue_t* value) const;
    const char* get_validation_error() const { return last_error_.c_str(); }
    
    // Bulk operations
    bool set_options_from_mapping(const mapping_t* options, object_t* caller = nullptr);
    mapping_t* get_all_options(object_t* caller = nullptr) const;
    mapping_t* get_options_by_category(socket_option_category category, object_t* caller = nullptr) const;
    
    // Protocol-specific helpers
    bool is_http_mode() const;
    bool is_rest_mode() const;
    bool is_websocket_mode() const;
    bool is_mqtt_mode() const;
    bool is_external_mode() const;
    bool is_cache_enabled() const;
    
    // Configuration state
    void clear_all_options();
    size_t get_option_count() const { return options_.size(); }
    
    // Debug and introspection
    void dump_options(outbuffer_t* buffer) const;
    array_t* get_option_names() const;
    array_t* get_categories() const;
    
    // Backward compatibility with existing socket system
    void migrate_from_legacy_options(const svalue_t legacy_options[], size_t count);
    void update_legacy_options(svalue_t legacy_options[], size_t count) const;
    
    // Friend functions for global access to static members
    friend const SocketOptionDescriptor* get_option_descriptor(int option);
    friend bool validate_socket_option(int option, const svalue_t* value, const char** error_msg);
    friend array_t* get_options_in_category(socket_option_category category);
    friend bool is_valid_socket_option(int option);
    
private:
    mutable std::string last_error_;
    
    // Security helper
    bool check_system_permission(object_t* caller) const;
    bool is_socket_owner(object_t* caller) const;
    socket_option_access get_caller_access_level(object_t* caller) const;
    
    // Type matching helper
    bool svalue_matches_type(const svalue_t* value, socket_option_type expected_type) const;
    
    // Internal option registration helpers
    void register_option(int option_id, socket_option_type type, socket_option_category category, 
                        socket_option_access access, const svalue_t& default_val, const char* desc,
                        long min_int = 0, long max_int = 0);
    void register_option(int option_id, socket_option_type type, socket_option_category category, 
                        socket_option_access access, bool default_bool, const char* desc);
    void register_option(int option_id, socket_option_type type, socket_option_category category, 
                        socket_option_access access, const char* default_str, const char* desc);
    void register_option(int option_id, socket_option_type type, socket_option_category category, 
                        socket_option_access access, int default_int, const char* desc, 
                        long min_val = 0, long max_val = 0);
    void register_option(int option_id, socket_option_type type, socket_option_category category, 
                        socket_option_access access, void* default_ptr, const char* desc);
    
    // Option category registration methods
    void register_core_options();
    void register_http_options();
    void register_rest_options();
    void register_websocket_options();
    void register_mqtt_options();
    void register_external_options();
    void register_database_options();
    void register_cache_options();
    void register_tls_options();
    void register_internal_options();
    
    // Protocol mode management
    int get_socket_mode_from_options() const;
    void update_protocol_modes(int option, const svalue_t* value);
    void set_internal_mode(int mode_option, bool enabled);
    
    // Utility functions for formatting
    const char* get_type_name(socket_option_type type) const;
    const char* get_category_name(socket_option_category category) const;
    
    // Constraint validation helpers
    bool validate_integer_constraints(int option, long value) const;
    bool validate_string_constraints(int option, const char* value) const;
    bool validate_float_constraints(int option, double value) const;
    bool validate_mapping_structure(int option, const mapping_t* value) const;
    bool validate_array_structure(int option, const array_t* value) const;
};

/*
 * Global utility functions for option management
 */

// Get the global option descriptor for an option
const SocketOptionDescriptor* get_option_descriptor(int option);

// Validate option without creating manager instance
bool validate_socket_option(int option, const svalue_t* value, const char** error_msg = nullptr);

// Convert option enum to string name
const char* socket_option_to_string(int option);

// Convert string name to option enum
int string_to_socket_option(const char* name);

// Get all options in a category
array_t* get_options_in_category(socket_option_category category);

// Check if an option exists in the system
bool is_valid_socket_option(int option);

/*
 * Protocol mode detection utilities
 */
bool is_protocol_option(int option);
bool requires_protocol_mode(int option, socket_mode_extended mode);

/*
 * Option constraint macros for easy validation
 */
#define VALIDATE_INTEGER_RANGE(opt, val, min, max) \
    ((val) >= (min) && (val) <= (max))

#define VALIDATE_STRING_LENGTH(opt, str, min_len, max_len) \
    (strlen(str) >= (min_len) && strlen(str) <= (max_len))

#endif  // SOCKET_OPTION_MANAGER_H_