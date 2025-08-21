if(${PACKAGE_ZMQTT})
    add_library(package_zmqtt STATIC
            "mqtt.cc"
            "mqtt.h"
            "mqtt_client.cc"
            "mqtt_client.h"
            "mqtt_message.cc"
            "mqtt_message.h"
            )
    
    # Link with bundled libwebsockets for MQTT client support
    if (TARGET websockets)
        target_link_libraries(package_zmqtt PRIVATE websockets)
        
        # Include directories for bundled libwebsockets MQTT headers
        target_include_directories(package_zmqtt PRIVATE 
            "${CMAKE_SOURCE_DIR}/src/thirdparty/libwebsockets/include"
            "${CMAKE_BINARY_DIR}/src/thirdparty/libwebsockets"
            "${CMAKE_CURRENT_SOURCE_DIR}/../sockets"
        )
        message(STATUS "MQTT package enabled with bundled libwebsockets")
    else()
        message(WARNING "MQTT package requires libwebsockets but websockets target not found")
        set(PACKAGE_ZMQTT OFF CACHE BOOL "Enable ZMQTT package" FORCE)
    endif()
endif()