# Host-side unit tests for hardware-free firmware code.
#   cmake -S tests/host -B build/host && cmake --build build/host && ctest --test-dir build/host
cmake_minimum_required(VERSION 3.16)
project(tomogotchi_host_tests C)

set(CMAKE_C_STANDARD 11)
add_compile_options(-Wall -Wextra -Werror)

enable_testing()

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../firmware/components/pet pet)

add_executable(test_pet test_pet.c)
target_link_libraries(test_pet PRIVATE pet)
add_test(NAME pet COMMAND test_pet)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../firmware/components/sprites sprites)

add_executable(test_sprites test_sprites.c)
target_link_libraries(test_sprites PRIVATE sprites)
add_test(NAME sprites COMMAND test_sprites)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../firmware/components/scene scene)

add_executable(test_scene test_scene.c)
target_link_libraries(test_scene PRIVATE scene)
add_test(NAME scene COMMAND test_scene)

# Not a test: dumps animation frames for tools/sprites/animate.py.
add_executable(scene_dump scene_dump.c)
target_link_libraries(scene_dump PRIVATE scene)
