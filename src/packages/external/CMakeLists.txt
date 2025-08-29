# External Process Package CMakeLists.txt
# FluffOS Unified Socket Architecture - External Process Integration

if(${PACKAGE_EXTERNAL})
    # Define external package sources
    set(EXTERNAL_SOURCES
        "external.cc"           # Main external package implementation with unified socket support
        "socketpair.cc"         # Cross-platform socketpair implementation
        "process_manager.cc"    # Process lifecycle management
        "command_executor.cc"   # High-level command execution engine
        "file_monitor.cc"       # File system monitoring with inotify integration (Phase 1)
        "event_notifier.cc"     # Enhanced async notifications with eventfd integration (Phase 2)
        "io_redirector.cc"      # I/O redirection and process communication (Phase 3)
        "resource_manager.cc"   # Resource control and sandboxing (Phase 4)
    )
    
    # Define external package headers
    set(EXTERNAL_HEADERS
        "external.h"            # Main external package header
        "process_manager.h"     # Process management interface
        "command_executor.h"    # Command execution interface
        "file_monitor.h"        # File monitoring interface (Phase 1: inotify support)
        "event_notifier.h"      # Event notification interface (Phase 2: eventfd support)
        "io_redirector.h"       # I/O redirection interface (Phase 3: I/O controls)
        "resource_manager.h"    # Resource control interface (Phase 4: resource management)
    )
    
    # Create external package library
    add_library(package_external STATIC
        ${EXTERNAL_SOURCES}
        ${EXTERNAL_HEADERS}
    )
    
    # Link dependencies
    target_link_libraries(package_external 
        PRIVATE package_sockets  # Required for unified socket architecture
    )
    
    # Include directories
    target_include_directories(package_external
        PRIVATE ${CMAKE_SOURCE_DIR}/src
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
        PRIVATE ${CMAKE_SOURCE_DIR}/src/packages/sockets  # For socket integration
    )
    
    # Compiler definitions
    target_compile_definitions(package_external PRIVATE 
        PACKAGE_EXTERNAL=1
        EXTERNAL_UNIFIED_SOCKET_SUPPORT=1
    )
    
    # Platform-specific settings
    if(WIN32)
        target_compile_definitions(package_external PRIVATE 
            _WIN32_WINNT=0x0601  # Windows 7 minimum
            WIN32_LEAN_AND_MEAN
        )
        target_link_libraries(package_external PRIVATE ws2_32)
    else()
        # Unix/Linux specific features
        target_compile_definitions(package_external PRIVATE 
            _GNU_SOURCE
            _POSIX_C_SOURCE=200809L
        )
        # Link pthread for process monitoring threads
        find_package(Threads REQUIRED)
        target_link_libraries(package_external PRIVATE Threads::Threads)
    endif()
    
    # Add threading support (required for ProcessManager and CommandExecutor)
    find_package(Threads REQUIRED)
    target_link_libraries(package_external PRIVATE Threads::Threads)
    
    # Security framework dependencies
    if(UNIX AND NOT APPLE)
        # Linux-specific security features
        target_compile_definitions(package_external PRIVATE HAVE_SECCOMP=1)
        # Note: libseccomp would be linked here if available
    endif()
    
    # Debug and optimization settings
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        target_compile_definitions(package_external PRIVATE 
            EXTERNAL_DEBUG=1
            EXTERNAL_DETAILED_LOGGING=1
        )
    endif()
    
    # Enable C++17 features for std::variant and other modern C++ constructs
    set_target_properties(package_external PROPERTIES 
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
    )
    
    # Compiler warnings
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        target_compile_options(package_external PRIVATE 
            -Wall -Wextra -Wpedantic
            -Wno-unused-parameter  # Common in FluffOS efun implementations
            -Wno-missing-field-initializers
        )
    endif()
    
    # Add to main build
    if(PACKAGE_EXTERNAL)
        target_link_libraries(${DRIVER_BIN} package_external)
        message(STATUS "External Process package enabled with unified socket architecture support")
        
        # Feature summary
        message(STATUS "  - Process lifecycle management: YES")
        message(STATUS "  - Command execution engine: YES")
        message(STATUS "  - Security sandboxing: ${UNIX}")
        message(STATUS "  - Socket integration: YES")
        message(STATUS "  - Threading support: YES")
        message(STATUS "  - Platform: ${CMAKE_SYSTEM_NAME}")
    endif()
    
    # Installation (if needed)
    if(INSTALL_EXTERNAL_HEADERS)
        install(FILES ${EXTERNAL_HEADERS}
            DESTINATION include/packages/external
            COMPONENT Development
        )
    endif()
    
    # Testing (if enabled)
    if(BUILD_TESTING AND PACKAGE_EXTERNAL)
        # Add external package tests here
        # enable_testing()
        # add_subdirectory(tests)
    endif()

else()
    # Package disabled
    message(STATUS "External Process package: DISABLED")
endif()

# Feature configuration summary
if(${PACKAGE_EXTERNAL})
    message(STATUS "")
    message(STATUS "External Process Package Configuration:")
    message(STATUS "  Package enabled: YES")
    message(STATUS "  Unified socket support: YES")
    message(STATUS "  Process management: YES")
    message(STATUS "  Command execution: YES")
    message(STATUS "  Security framework: ${UNIX}")
    message(STATUS "  Threading support: YES")
    message(STATUS "  Platform: ${CMAKE_SYSTEM_NAME}")
    message(STATUS "  Build type: ${CMAKE_BUILD_TYPE}")
    message(STATUS "")
endif()