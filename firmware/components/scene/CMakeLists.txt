# Scene composition and animation. No ESP-IDF dependencies, so tests/host
# builds it too.
if(COMMAND idf_component_register)
    idf_component_register(SRCS "scene.c" INCLUDE_DIRS "include" REQUIRES pet sprites)
else()
    add_library(scene STATIC scene.c)
    target_include_directories(scene PUBLIC include)
    target_link_libraries(scene PUBLIC pet sprites)
endif()
