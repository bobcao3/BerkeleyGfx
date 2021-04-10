cmake_minimum_required(VERSION 3.14)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
    message(FATAL_ERROR "Do not build in-source. Please remove CMakeCache.txt and the CMakeFiles/ directory. Then build out-of-source.")
endif()

# The library itself

project(
    "BoilerPlate"
    VERSION 0.0.0
    LANGUAGES C CXX
)

set(BerkeleyGfx_SRC
  src/renderer.cpp
  src/pipelines.cpp
  src/command_buffer.cpp
  src/buffer.cpp
  src/texture_system.cpp
  src/lifetime_tracker.cpp
  src/static_callbacks.cpp

  ext/imgui/imgui.cpp
  ext/imgui/imgui_draw.cpp
  ext/imgui/imgui_tables.cpp
  ext/imgui/imgui_widgets.cpp
  ext/imgui/backends/imgui_impl_glfw.cpp
  ext/imgui/backends/imgui_impl_vulkan.cpp

  ext/SPIRV-Reflect/spirv_reflect.c
)

add_library(BerkeleyGfx ${BerkeleyGfx_SRC})

option(GLFW_BUILD_EXAMPLES "" OFF)
option(GLFW_BUILD_TESTS "" OFF)
option(GLFW_BUILD_DOCS "" OFF)
option(GLFW_INSTALL "" OFF)

add_subdirectory(ext/glfw)
add_subdirectory(ext/glm)
add_subdirectory(ext/glslang)
add_subdirectory(ext/spdlog)

find_package(Vulkan)

target_link_libraries(BerkeleyGfx PUBLIC Vulkan::Vulkan)
target_link_libraries(BerkeleyGfx PUBLIC glfw)
target_link_libraries(BerkeleyGfx PUBLIC glm)
target_link_libraries(BerkeleyGfx PUBLIC glslang)
target_link_libraries(BerkeleyGfx PUBLIC SPIRV)

set(BerkeleyGfx_INCLUDE
  ext/glm
  ${glfw_INCLUDE_DIRS}
  ext/imgui
  ext/glslang
  ext/spdlog/include
  ext/VulkanMemoryAllocator/src
  ext/tinygltf
  ext/SPRIV-Reflect
  ext/
  src
)

target_include_directories(BerkeleyGfx PUBLIC ${BerkeleyGfx_INCLUDE})

# Sample projects

add_executable(SampleHelloTriangle "sample/triangle.cpp")
target_link_libraries(SampleHelloTriangle PUBLIC BerkeleyGfx)
target_include_directories(SampleHelloTriangle PUBLIC ${BerkeleyGfx_INCLUDE})

add_executable(SampleGlTFViewer "sample/1_glTFViewer/gltfViewer.cpp")
target_compile_definitions(SampleGlTFViewer PUBLIC SRC_DIR="${CMAKE_SOURCE_DIR}")
target_link_libraries(SampleGlTFViewer PUBLIC BerkeleyGfx)
target_include_directories(SampleGlTFViewer PUBLIC ${BerkeleyGfx_INCLUDE})

add_executable(SampleTerrain "sample/2_terrain/terrain.cpp")
target_compile_definitions(SampleTerrain PUBLIC SRC_DIR="${CMAKE_SOURCE_DIR}")
target_link_libraries(SampleTerrain PUBLIC BerkeleyGfx)
target_include_directories(SampleTerrain PUBLIC ${BerkeleyGfx_INCLUDE})

add_executable(SampleShaderGraph "sample/3_shaderGraph/shaderGraph.cpp")
target_compile_definitions(SampleShaderGraph PUBLIC SRC_DIR="${CMAKE_SOURCE_DIR}")
target_link_libraries(SampleShaderGraph PUBLIC BerkeleyGfx)
target_include_directories(SampleShaderGraph PUBLIC ${BerkeleyGfx_INCLUDE})
set_property(TARGET SampleShaderGraph PROPERTY CXX_STANDARD 17)

# Set default project when generating a solution file

set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT SampleHelloTriangle)