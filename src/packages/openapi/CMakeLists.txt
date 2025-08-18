if(${PACKAGE_OPENAPI})
    add_library(package_openapi STATIC
            "openapi.cc"
            "openapi.h"
            "openapi_generator.cc"
            "openapi_docs.cc"
            )
    target_link_libraries(package_openapi PUBLIC package_rest package_json package_http)
    target_include_directories(package_openapi PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
endif()