if(${PACKAGE_MQTT})
    add_library(package_mqtt STATIC
            "mqtt.cc"
            "mqtt.h"
            "mqtt_client.cc"
            "mqtt_client.h"
            "mqtt_message.cc"
            "mqtt_message.h"
            )
    
    # Link with libwebsockets for MQTT client support
    target_link_libraries(package_mqtt PRIVATE libwebsockets)
    
    # Include directories for libwebsockets MQTT headers
    target_include_directories(package_mqtt PRIVATE 
        "${CMAKE_CURRENT_SOURCE_DIR}/../../thirdparty/libwebsockets/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/../sockets"
    )
endif()