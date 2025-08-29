# GraphQL FluffOS Integration Implementation Plan

## Overview

This document outlines the detailed implementation plan for integrating the GraphQL package with FluffOS. The GraphQL C++ backend is nearly complete, but the FluffOS interface layer (efuns) is missing, causing linking errors during compilation.

**Status**: Implementation Phase Required  
**Priority**: High - Blocking GraphQL functionality  
**Estimated Effort**: 2-3 development sessions  

## Problem Analysis

### Current Situation
- ✅ **GraphQL C++ Backend**: Complete implementation with ~900 lines of code
- ✅ **GraphQL Schema**: Comprehensive 409-line MUD schema with all types
- ✅ **GraphQL Header**: Complete class definitions and interfaces
- ✅ **CMake Integration**: Proper build configuration
- ❌ **FluffOS Interface**: Missing efun implementations for LPC integration

### Linking Errors
The following undefined references need implementation:
```
f_graphql_execute_query()
f_graphql_subscribe() 
f_graphql_set_schema()
f_graphql_broadcast_event()
f_graphql_broadcast_player_event()
f_graphql_broadcast_room_event()
```

## Implementation Strategy: Option A (Recommended)

**Approach**: Create FluffOS interface layer that bridges LPC calls to existing C++ backend

### Phase 1: Create GraphQL Efuns File

**File**: `src/packages/graphql/graphql_efuns.cc`

**Purpose**: Implement the 6 missing f_graphql_* functions that bridge LPC to C++

**Implementation Pattern**:
```cpp
#ifdef F_GRAPHQL_EXECUTE_QUERY
void f_graphql_execute_query() {
    // LPC stack parameter validation
    if (st_num_arg != 3) {
        error("graphql_execute_query: wrong number of arguments\n");
    }
    if ((sp-2)->type != T_NUMBER) {
        bad_arg(1, F_GRAPHQL_EXECUTE_QUERY);
    }
    if ((sp-1)->type != T_STRING) {
        bad_arg(2, F_GRAPHQL_EXECUTE_QUERY);  
    }
    if (sp->type != T_STRING) {
        bad_arg(3, F_GRAPHQL_EXECUTE_QUERY);
    }

    // Extract parameters
    int socket_fd = (sp-2)->u.number;
    const char* query = (sp-1)->u.string;
    const char* variables = sp->u.string;

    // Call C++ backend
    GraphQLManager* manager = GraphQLManager::getInstance();
    GraphQLRequest request;
    request.query = query;
    request.socket_fd = socket_fd;
    // Parse variables JSON string into request.variables map
    
    GraphQLResponse response = manager->execute_query(socket_fd, request);
    
    // Convert response to LPC format
    pop_3_elems();
    if (response.status == GRAPHQL_SUCCESS) {
        push_malloced_string(string_copy(response.data.c_str(), "graphql_result"));
    } else {
        push_number(response.status);
    }
}
#endif
```

### Phase 2: Parameter Validation and Error Handling

**Security Considerations**:
- Validate socket file descriptors
- Check string parameters for null/empty
- Implement proper memory management
- Add bounds checking for all inputs

**Error Handling Pattern**:
```cpp
// Standard FluffOS error handling
if (!manager || !manager->is_initialized()) {
    error("GraphQL manager not initialized\n");
}

if (socket_fd < 0 || socket_fd >= MAX_SOCKETS) {
    error("Invalid socket descriptor: %d\n", socket_fd);
}
```

### Phase 3: JSON Parameter Processing

**Challenge**: Convert LPC string parameters to C++ data structures

**Solution**:
```cpp
// Parse JSON variables string into std::map
std::map<std::string, std::string> parse_variables_json(const char* json_str) {
    std::map<std::string, std::string> variables;
    
    if (!json_str || strlen(json_str) == 0) {
        return variables;
    }
    
    // Use existing JSON parsing utilities in FluffOS
    // or implement simple key-value parser for basic cases
    
    return variables;
}
```

### Phase 4: Response Formatting

**LPC Return Values**:
- `graphql_execute_query()`: Returns string (success) or int (error code)
- `graphql_subscribe()`: Returns subscription ID string or error code
- `graphql_set_schema()`: Returns 1 (success) or 0 (failure)
- Broadcast functions: Return void (fire-and-forget)

### Phase 5: Update Build Configuration

**Ensure CMakeLists.txt includes the new efuns file**:
```cmake
set(GRAPHQL_SOURCES
    graphql.cc
    graphql_schema.cc
    graphql_subscription_manager.cc
    graphql_efuns.cc  # Add this line
)
```

## Technical Specifications

### Function Signatures (LPC Interface)

```lpc
// Execute GraphQL query
mixed graphql_execute_query(int socket_fd, string query, string variables);

// Subscribe to GraphQL events  
mixed graphql_subscribe(int socket_fd, string subscription, string variables);

// Set GraphQL schema
int graphql_set_schema(int socket_fd, string schema_text);

// Broadcast events to subscribers (three separate functions)
void graphql_broadcast_event(string event_type, string data);
void graphql_broadcast_player_event(string player_id, string event_type, string data);
void graphql_broadcast_room_event(string room_id, string event_type, string data);
```

### Broadcast Function Design Decision

**Three separate functions chosen over single function** for:
- **Clear Intent**: Each function has obvious purpose
- **Type Safety**: Different parameter counts prevent errors
- **Performance**: Direct routing without parsing
- **MUD-Specific**: Aligns with MUD concepts (global, player, room events)

### C++ Backend Integration Points

```cpp
// Key C++ classes to integrate with:
GraphQLManager* GraphQLManager::getInstance()
GraphQLResponse GraphQLManager::execute_query(int socket_fd, const GraphQLRequest& request)
bool GraphQLManager::subscribe(int socket_fd, const std::string& subscription, const std::map<std::string, std::string>& variables)
bool GraphQLManager::load_schema(int socket_fd, const std::string& schema_text)
void GraphQLManager::broadcast_event(const std::string& event_type, const std::string& data)
```

## Risk Mitigation

### Memory Management
- Use FluffOS string allocation functions (`string_copy`, `push_malloced_string`)
- Proper stack cleanup with `pop_n_elems()`
- No direct malloc/free - use FluffOS memory management

### Error Handling
- Follow FluffOS error patterns (`error()`, `bad_arg()`)
- Validate all inputs before C++ backend calls
- Graceful degradation when GraphQL manager unavailable

### Thread Safety
- GraphQL C++ backend appears thread-safe
- LPC interface is single-threaded by design
- No additional synchronization needed

## Success Criteria

1. **Compilation Success**: All undefined references resolved
2. **LPC Integration**: Functions callable from LPC code
3. **Functional Testing**: Basic GraphQL operations work
4. **Memory Safety**: No leaks or crashes under normal use
5. **Error Handling**: Graceful handling of invalid inputs

## Implementation Sequence

1. **Create `graphql_efuns.cc`** with all 6 functions
2. **Update CMakeLists.txt** to include new file
3. **Test compilation** using build monitor
4. **Basic functionality testing** with simple LPC calls
5. **Integration testing** with MUD-specific GraphQL operations
6. **Documentation updates** and knowledge base entries

## Timeline Estimate

- **Session 1**: Create efuns file, basic function stubs
- **Session 2**: Implement full parameter processing and C++ integration
- **Session 3**: Testing, debugging, and documentation

## Files to Modify

1. **CREATE**: `src/packages/graphql/graphql_efuns.cc` - Main implementation file
2. **MODIFY**: `src/packages/graphql/CMakeLists.txt` - Add new source file
3. **TEST**: Create LPC test files in appropriate test directory
4. **DOCUMENT**: Update package documentation and knowledge base

---

**Next Action**: Begin implementation by creating the `graphql_efuns.cc` file with proper FluffOS interface patterns.