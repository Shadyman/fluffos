if(${PACKAGE_WEBSOCKET})
    # Use bundled libwebsockets (always available in FluffOS)
    if (TARGET websockets)
        add_library(package_websocket STATIC
                "websocket.cc"
                "websocket.h"
                "ws_server.cc"
                "ws_server.h"
                "ws_client.cc"
                "ws_client.h"
                "ws_frame.cc"
                "ws_frame.h"
                )
        
        # Link with bundled libwebsockets
        target_link_libraries(package_websocket websockets)
        
        # Include libwebsockets headers
        target_include_directories(package_websocket PRIVATE 
            ${CMAKE_SOURCE_DIR}/src/thirdparty/libwebsockets/include
            ${CMAKE_BINARY_DIR}/src/thirdparty/libwebsockets)
        
        message(STATUS "WebSocket package enabled with bundled libwebsockets")
        
    else()
        # Fallback: Try to find system libwebsockets
        find_package(PkgConfig QUIET)
        if (PKG_CONFIG_FOUND)
            pkg_check_modules(LIBWEBSOCKETS QUIET libwebsockets)
        endif()
        
        if (NOT LIBWEBSOCKETS_FOUND)
            # Try to find libwebsockets manually
            find_path(LIBWEBSOCKETS_INCLUDE_DIR libwebsockets.h)
            find_library(LIBWEBSOCKETS_LIBRARY websockets)
            
            if (LIBWEBSOCKETS_INCLUDE_DIR AND LIBWEBSOCKETS_LIBRARY)
                set(LIBWEBSOCKETS_FOUND TRUE)
                set(LIBWEBSOCKETS_LIBRARIES ${LIBWEBSOCKETS_LIBRARY})
                set(LIBWEBSOCKETS_INCLUDE_DIRS ${LIBWEBSOCKETS_INCLUDE_DIR})
            endif()
        endif()
        
        if (LIBWEBSOCKETS_FOUND)
            add_library(package_websocket STATIC
                    "websocket.cc"
                    "websocket.h"
                    "ws_server.cc"
                    "ws_server.h"
                    "ws_client.cc"
                    "ws_client.h"
                    "ws_frame.cc"
                    "ws_frame.h"
                    )
            
            target_include_directories(package_websocket PRIVATE ${LIBWEBSOCKETS_INCLUDE_DIRS})
            target_link_libraries(package_websocket ${LIBWEBSOCKETS_LIBRARIES})
            
            message(STATUS "WebSocket package enabled with system libwebsockets")
        else()
            message(WARNING "libwebsockets not found, WebSocket package disabled")
            set(PACKAGE_WEBSOCKET OFF CACHE BOOL "Enable WebSocket package" FORCE)
        endif()
    endif()
endif()