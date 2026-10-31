cmake_minimum_required(VERSION 3.16)

# Keep sdkconfig inside the build folder, so each board gets its own build
# folder and config (e.g. build-esp32c6/ and build-esp32s3/) and a change to
# sdkconfig.defaults* takes effect in a fresh build folder.
set(SDKCONFIG "${CMAKE_BINARY_DIR}/sdkconfig")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(tomogotchi)
