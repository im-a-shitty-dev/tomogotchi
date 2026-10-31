# Built by ESP-IDF as a component, and by tests/host as a plain library.
if(COMMAND idf_component_register)
    idf_component_register(SRCS "pet.c" "place.c" INCLUDE_DIRS "include")
else()
    add_library(pet STATIC pet.c place.c)
    target_include_directories(pet PUBLIC include)
endif()
