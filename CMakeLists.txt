cmake_minimum_required(VERSION 3.28)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(App LANGUAGES CXX)


set(CMAKE_CXX_STANDARD 20)

include(FetchContent)

set(DAWN_FETCH_DEPENDENCIES ON)
set(DAWN_BUILD_SAMPLES OFF)
set(BUILD_SAMPLES OFF)

FetchContent_Declare(
    dawn
    GIT_REPOSITORY https://github.com/google/dawn.git
    GIT_TAG        v20260720.160313
    GIT_SHALLOW    TRUE
)

FetchContent_MakeAvailable(dawn)

add_executable(App "main.cpp"
    misc.h
    application.h
    application.cpp)
target_link_libraries(App PRIVATE webgpu_dawn webgpu_glfw glfw)

target_compile_definitions(
    App PRIVATE
    RESOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/resources"
)

