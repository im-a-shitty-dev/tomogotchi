# When the screen is on. No ESP-IDF dependencies, so tests/host builds it too.
if(COMMAND idf_component_register)
    idf_component_register(SRCS "wake.c" INCLUDE_DIRS "include")
else()
    add_library(wake STATIC wake.c)
    target_include_directories(wake PUBLIC include)
endif()
