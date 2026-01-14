if(${PACKAGE_DB_OBJECTS})
    add_library(package_db_objects STATIC
            "db_objects.cc"
            )
endif()
