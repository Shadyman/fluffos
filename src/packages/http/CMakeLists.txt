if(${PACKAGE_HTTP})
    add_library(package_http STATIC
            "http.cc"
            "http.h"
            "http_server.cc"
            "http_client.cc"
            )
    target_link_libraries(package_http PUBLIC package_sockets package_json)
    target_include_directories(package_http PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
endif()