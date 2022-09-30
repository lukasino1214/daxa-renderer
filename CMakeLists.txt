cmake_minimum_required(VERSION 3.12)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_TOOLCHAIN_FILE "/home/lukas/vcpkg/scripts/buildsystems/vcpkg.cmake")

execute_process(
    COMMAND ${CMAKE_COMMAND} -E create_symlink
        ${CMAKE_BINARY_DIR}/compile_commands.json
        ${CMAKE_SOURCE_DIR}/compile_commands.json
)

project(daxa-renderer)

find_package(daxa CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_path(TINYGLTF_INCLUDE_DIRS "tiny_gltf.h")

add_executable(${PROJECT_NAME} 
    "src/main.cpp" 
    "src/graphics/model.hpp"
    "src/graphics/model.cpp"
    "src/graphics/texture.hpp"
    "src/graphics/texture.cpp"
    "src/graphics/camera.hpp"
    "src/graphics/camera.cpp"
    "src/systems/ibl_renderer.hpp"
    "src/systems/ibl_renderer.cpp"
)

target_link_libraries(${PROJECT_NAME} PRIVATE daxa::daxa glm::glm glfw)
target_include_directories(${PROJECT_NAME} PRIVATE ${TINYGLTF_INCLUDE_DIRS})
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_20)
