# gRPC Package for FluffOS Unified Socket Architecture

# Package source files
set(GRPC_SOURCES
    grpc.cc
    grpc_server.cc
    grpc_client.cc
    grpc_protobuf.cc
    grpc_channel_manager.cc
    lpc_functions.cc
)

# Package headers
set(GRPC_HEADERS
    grpc.h
    lpc_interface.h
)

# Try to find gRPC and Protocol Buffers
find_package(PkgConfig QUIET)

# Check for gRPC++ library
set(GRPC_FOUND FALSE)
set(PROTOBUF_FOUND FALSE)

if(PKG_CONFIG_FOUND)
    pkg_check_modules(GRPC QUIET grpc++)
    pkg_check_modules(PROTOBUF QUIET protobuf)
    
    if(GRPC_FOUND AND PROTOBUF_FOUND)
        message(STATUS "gRPC libraries found: ${GRPC_VERSION}")
        message(STATUS "Protocol Buffers found: ${PROTOBUF_VERSION}")
        
        # Find the Protocol Buffers compiler
        find_program(PROTOC protoc)
        find_program(GRPC_CPP_PLUGIN grpc_cpp_plugin)
        
        if(PROTOC AND GRPC_CPP_PLUGIN)
            message(STATUS "Protocol compiler found: ${PROTOC}")
            message(STATUS "gRPC C++ plugin found: ${GRPC_CPP_PLUGIN}")
            set(GRPC_FULL_SUPPORT TRUE)
        else()
            message(STATUS "Protocol compiler or gRPC plugin not found - stub implementation only")
            set(GRPC_FULL_SUPPORT FALSE)
        endif()
    else()
        message(STATUS "gRPC libraries not found - using stub implementation")
    endif()
else()
    message(STATUS "pkg-config not found - using stub gRPC implementation")
endif()

# Create the gRPC package
add_library(package_grpc STATIC ${GRPC_SOURCES})

# Include directories
target_include_directories(package_grpc PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/packages
    ${CMAKE_SOURCE_DIR}/src/packages/sockets
)

# Link with socket package for SocketOptionManager
target_link_libraries(package_grpc
    package_sockets
)

# Add gRPC libraries if available
if(GRPC_FOUND AND PROTOBUF_FOUND)
    target_include_directories(package_grpc PRIVATE ${GRPC_INCLUDE_DIRS} ${PROTOBUF_INCLUDE_DIRS})
    target_link_libraries(package_grpc ${GRPC_LIBRARIES} ${PROTOBUF_LIBRARIES})
    target_compile_definitions(package_grpc PRIVATE HAVE_GRPC=1)
    
    # Add additional gRPC specific flags
    target_compile_options(package_grpc PRIVATE ${GRPC_CFLAGS_OTHER} ${PROTOBUF_CFLAGS_OTHER})
    
    message(STATUS "gRPC Package: Full gRPC support enabled")
else()
    target_compile_definitions(package_grpc PRIVATE HAVE_GRPC=0)
    message(STATUS "gRPC Package: Stub implementation (install libgrpc++-dev libprotobuf-dev for full support)")
endif()

# C++17 required for modern features
target_compile_features(package_grpc PRIVATE cxx_std_17)

# Compiler flags
target_compile_options(package_grpc PRIVATE
    -Wall
    -Wextra
    -Wno-unused-parameter
)

# Add debug flags if in debug mode
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_definitions(package_grpc PRIVATE DEBUG=1)
endif()

# Protocol buffer code generation function (if available)
if(GRPC_FULL_SUPPORT)
    function(grpc_generate_cpp SRCS HDRS GRPC_SRCS GRPC_HDRS)
        if(NOT ARGN)
            message(SEND_ERROR "Error: grpc_generate_cpp() called without any proto files")
            return()
        endif()

        set(${SRCS})
        set(${HDRS})
        set(${GRPC_SRCS})
        set(${GRPC_HDRS})
        
        foreach(FIL ${ARGN})
            get_filename_component(ABS_FIL ${FIL} ABSOLUTE)
            get_filename_component(FIL_WE ${FIL} NAME_WE)
            get_filename_component(FIL_DIR ${ABS_FIL} DIRECTORY)

            list(APPEND ${SRCS} "${CMAKE_CURRENT_BINARY_DIR}/${FIL_WE}.pb.cc")
            list(APPEND ${HDRS} "${CMAKE_CURRENT_BINARY_DIR}/${FIL_WE}.pb.h")
            list(APPEND ${GRPC_SRCS} "${CMAKE_CURRENT_BINARY_DIR}/${FIL_WE}.grpc.pb.cc")
            list(APPEND ${GRPC_HDRS} "${CMAKE_CURRENT_BINARY_DIR}/${FIL_WE}.grpc.pb.h")

            add_custom_command(
                OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${FIL_WE}.pb.cc"
                       "${CMAKE_CURRENT_BINARY_DIR}/${FIL_WE}.pb.h"
                       "${CMAKE_CURRENT_BINARY_DIR}/${FIL_WE}.grpc.pb.cc"
                       "${CMAKE_CURRENT_BINARY_DIR}/${FIL_WE}.grpc.pb.h"
                COMMAND ${PROTOC}
                ARGS --grpc_out=${CMAKE_CURRENT_BINARY_DIR}
                     --cpp_out=${CMAKE_CURRENT_BINARY_DIR}
                     --plugin=protoc-gen-grpc=${GRPC_CPP_PLUGIN}
                     -I${FIL_DIR}
                     ${ABS_FIL}
                DEPENDS ${ABS_FIL}
                COMMENT "Running gRPC C++ protocol buffer compiler on ${FIL}"
                VERBATIM)
        endforeach()

        set_source_files_properties(${${SRCS}} ${${HDRS}} ${${GRPC_SRCS}} ${${GRPC_HDRS}} PROPERTIES GENERATED TRUE)
        set(${SRCS} ${${SRCS}} PARENT_SCOPE)
        set(${HDRS} ${${HDRS}} PARENT_SCOPE)
        set(${GRPC_SRCS} ${${GRPC_SRCS}} PARENT_SCOPE)
        set(${GRPC_HDRS} ${${GRPC_HDRS}} PARENT_SCOPE)
    endfunction()
    
    # Generate code from example proto files if they exist
    file(GLOB PROTO_FILES "${CMAKE_CURRENT_SOURCE_DIR}/examples/*.proto")
    if(PROTO_FILES)
        grpc_generate_cpp(PROTO_SRCS PROTO_HDRS GRPC_SRCS GRPC_HDRS ${PROTO_FILES})
        target_sources(package_grpc PRIVATE ${PROTO_SRCS} ${GRPC_SRCS})
        target_include_directories(package_grpc PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
        message(STATUS "Generated gRPC code from ${PROTO_FILES}")
    endif()
endif()

# Install headers
install(FILES ${GRPC_HEADERS} 
        DESTINATION include/packages/grpc)

# Install library
install(TARGETS package_grpc 
        DESTINATION lib)

# Package information
set(PACKAGE_GRPC_VERSION "1.0.0")
set(PACKAGE_GRPC_DESCRIPTION "gRPC server and client package for FluffOS unified socket architecture")

# Package configuration summary
message(STATUS "gRPC Package Configuration:")
message(STATUS "  Version: ${PACKAGE_GRPC_VERSION}")
message(STATUS "  gRPC Support: ${GRPC_FOUND}")
message(STATUS "  Protocol Buffers: ${PROTOBUF_FOUND}")
message(STATUS "  Full Implementation: ${GRPC_FULL_SUPPORT}")
if(GRPC_FOUND)
    message(STATUS "  gRPC Version: ${GRPC_VERSION}")
    message(STATUS "  gRPC Libraries: ${GRPC_LIBRARIES}")
endif()
if(PROTOBUF_FOUND)
    message(STATUS "  Protobuf Version: ${PROTOBUF_VERSION}")
endif()

# Optional: Install example proto files
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/examples/")
    install(DIRECTORY examples/
            DESTINATION share/fluffos/grpc/examples
            FILES_MATCHING PATTERN "*.proto" PATTERN "*.lpc")
endif()