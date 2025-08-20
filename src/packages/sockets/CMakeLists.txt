if(${PACKAGE_SOCKETS})
    add_library(package_sockets STATIC
            "socket_efuns.cc"
            "socket_efuns.h"
            "socket_option_manager.h"
            "socket_option_manager.cc"
            "socket_options.h"
            "socket_option_validator.h"
            "socket_option_validator.cc"
            "socket_error_handler.h"
            "socket_error_handler.cc"
            "sockets.cc"
            )
endif()
