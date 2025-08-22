# GraphQL Package for FluffOS Unified Socket Architecture

# Package source files
set(GRAPHQL_SOURCES
    graphql.cc
    graphql_schema.cc
    graphql_subscription_manager.cc
)

# Package headers
set(GRAPHQL_HEADERS
    graphql.h
)

# Create the GraphQL package
add_library(package_graphql STATIC ${GRAPHQL_SOURCES})

# Include directories
target_include_directories(package_graphql PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/packages
    ${CMAKE_SOURCE_DIR}/src/packages/sockets
)

# Link with socket package for SocketOptionManager
target_link_libraries(package_graphql
    package_sockets
)

# C++17 required for modern features
target_compile_features(package_graphql PRIVATE cxx_std_17)

# Compiler flags
target_compile_options(package_graphql PRIVATE
    -Wall
    -Wextra
    -Wno-unused-parameter
)

# Add debug flags if in debug mode
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_definitions(package_graphql PRIVATE DEBUG=1)
endif()

# Install headers
install(FILES ${GRAPHQL_HEADERS} 
        DESTINATION include/packages/graphql)

# Install library
install(TARGETS package_graphql 
        DESTINATION lib)

# Package information
set(PACKAGE_GRAPHQL_VERSION "1.0.0")
set(PACKAGE_GRAPHQL_DESCRIPTION "GraphQL server and client package for FluffOS unified socket architecture")