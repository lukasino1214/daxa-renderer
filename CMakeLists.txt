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
find_package(EnTT CONFIG REQUIRED)
find_path(TINYGLTF_INCLUDE_DIRS "tiny_gltf.h")
find_package(yaml-cpp CONFIG REQUIRED)

add_executable(${PROJECT_NAME}
    "src/main.cpp" 
    "src/graphics/window.hpp"
    "src/graphics/window.cpp"
    "src/graphics/model.hpp"
    "src/graphics/model.cpp"
    "src/graphics/texture.hpp"
    "src/graphics/texture.cpp"
    "src/graphics/camera.hpp"
    "src/graphics/camera.cpp"
    "src/graphics/buffer.hpp"
    "src/systems/ibl_renderer.hpp"
    "src/systems/ibl_renderer.cpp"
    "src/systems/rendering_system.hpp"
    "src/systems/rendering_system.cpp"
    "src/data/scene.hpp"
    "src/data/scene.cpp"
    "src/data/scene_serializer.hpp"
    "src/data/scene_serializer.cpp"
    "src/data/entity.hpp"
    "src/data/entity.cpp"
    "src/data/UUID.hpp"
    "src/data/UUID.cpp"
    "src/data/components.hpp"
    "src/utils/utils.hpp"
    "src/utils/utils.cpp"
    "src/panels/scene_hiearchy.hpp"    
    "src/panels/scene_hiearchy.cpp"
)

target_link_libraries(${PROJECT_NAME} PRIVATE daxa::daxa glm::glm glfw EnTT::EnTT yaml-cpp)
target_include_directories(${PROJECT_NAME} PRIVATE ${TINYGLTF_INCLUDE_DIRS})
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_20)
