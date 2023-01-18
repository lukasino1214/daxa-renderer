cmake_minimum_required(VERSION 3.12)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
project(daxa-renderer)

find_package(daxa CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(fastgltf CONFIG REQUIRED)
find_package(yaml-cpp CONFIG REQUIRED)
find_package(simdjson CONFIG REQUIRED)
find_package(EnTT CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)

find_path(STB_INCLUDE_DIRS "stb_image.h" REQUIRED)
add_library(stb_interface INTERFACE)
target_include_directories(stb_interface INTERFACE ${STB_INCLUDE_DIRS})

add_executable(${PROJECT_NAME} 
    "src/main.cpp" 
    "src/app.cpp" 
    "src/graphics/texture.cpp" 
    "src/graphics/model.cpp" 
    "src/graphics/camera.cpp"
    "src/data/scene.cpp"
    "src/data/scene_serializer.cpp"
    "src/data/entity.hpp"
    "src/data/entity.cpp"
    "src/data/UUID.cpp"
    "src/data/components.hpp"
    "src/panels/scene_hiearchy.cpp"
    "src/panels/viewport_panel.cpp"
    "src/bloom.cpp"
    "src/new_model.cpp"
    "src/new_model.hpp"
)
target_link_libraries(${PROJECT_NAME} PRIVATE daxa::daxa glm::glm glfw stb_interface fastgltf::fastgltf yaml-cpp EnTT::EnTT imgui::imgui)
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_20)

set(DAXA_INCLUDE_DIR "$<TARGET_FILE_DIR:${PROJECT_NAME}>/../vcpkg_installed/x64-$<LOWER_CASE:$<PLATFORM_ID>>/include")
target_compile_definitions(${PROJECT_NAME} PRIVATE DAXA_SHADER_INCLUDE_DIR="${DAXA_INCLUDE_DIR}")
