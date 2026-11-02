# Spots a stuck motion sensor. No ESP-IDF dependencies, so tests/host builds it too.
if(COMMAND idf_component_register)
    idf_component_register(SRCS "imu_guard.c" INCLUDE_DIRS "include")
else()
    add_library(imu_guard STATIC imu_guard.c)
    target_include_directories(imu_guard PUBLIC include)
    target_link_libraries(imu_guard PUBLIC m)
endif()
