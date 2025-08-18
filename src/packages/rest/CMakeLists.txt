if(${PACKAGE_REST})
    add_library(package_rest STATIC
            "rest.cc"
            "rest.h"
            "rest_router.cc"
            "rest_auth.cc"
            "rest_validation.cc"
            )
    target_link_libraries(package_rest PUBLIC package_http package_json package_crypto)
    target_include_directories(package_rest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
endif()