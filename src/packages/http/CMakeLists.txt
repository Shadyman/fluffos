if(${PACKAGE_HTTP})
    add_library(package_http STATIC
            "http.cc"
            "http.h"
            "http_client.cc"
            "http_server.cc"
            "http_parser.cc"
            "http_response.cc"
            "rest/rest_router.cc"
            "rest/rest_router.h"
            "rest/rest_validation.cc"
            "rest/rest_auth.cc"
            "rest/openapi_generator.cc"
            )
    
    target_link_libraries(package_http
        package_sockets  # Depends on Phase 1 sockets package
        )
        
    target_include_directories(package_http PRIVATE
        ${CMAKE_SOURCE_DIR}/src/packages/sockets  # Access to socket option manager
        )
endif()