cmake_minimum_required(VERSION 3.28)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(App LANGUAGES CXX)


set(CMAKE_CXX_STANDARD 20)

set(FETCHCONTENT_QUIET OFF)

include(FetchContent)

set(DAWN_FETCH_DEPENDENCIES ON)

FetchContent_Declare(
    dawn
    GIT_REPOSITORY https://github.com/google/dawn.git
    GIT_TAG        v20260720.160313
    GIT_SHALLOW    TRUE
    GIT_SUBMODULES ""
)

FetchContent_Declare(
    glm
    GIT_REPOSITORY  https://github.com/g-truc/glm.git
    GIT_TAG         1.0.1
)

FetchContent_Declare(
    tinyobjloader
    GIT_REPOSITORY  https://github.com/tinyobjloader/tinyobjloader.git
    GIT_TAG         v2.0.0rc13
)

FetchContent_MakeAvailable(dawn)
FetchContent_MakeAvailable(glm)
FetchContent_MakeAvailable(tinyobjloader)

add_executable(App "main.cpp"
    misc.h
    application.h
    application.cpp)
    
target_link_libraries(App PRIVATE 
    webgpu_dawn
    webgpu_glfw
    glfw
    glm::glm
    tinyobjloader)

target_compile_definitions(
    App PRIVATE
    RESOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/resources"
    GLM_FORCE_DEPTH_ZERO_TO_ONE
    GLM_FORCE_LEFT_HANDED
)

