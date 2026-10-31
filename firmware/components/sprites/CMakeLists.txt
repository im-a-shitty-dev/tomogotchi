# Sprite data and drawing. No ESP-IDF dependencies, so tests/host builds it too.
if(COMMAND idf_component_register)
    idf_component_register(
        SRCS "sprite.c" "sprites_gen.c"
        INCLUDE_DIRS "include"
    )
else()
    add_library(sprites STATIC sprite.c sprites_gen.c)
    target_include_directories(sprites PUBLIC include)
endif()
