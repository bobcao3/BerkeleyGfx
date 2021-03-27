cmake_minimum_required(VERSION 3.14)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
    message(FATAL_ERROR "Do not build in-source. Please remove CMakeCache.txt and the CMakeFiles/ directory. Then build out-of-source.")
endif()

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

  ext/imgui/imgui.cpp
  ext/imgui/imgui_draw.cpp
  ext/imgui/imgui_tables.cpp
  ext/imgui/imgui_widgets.cpp
  ext/imgui/backends/imgui_impl_glfw.cpp
  ext/imgui/backends/imgui_impl_vulkan.cpp
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
  src
)

target_include_directories(BerkeleyGfx PUBLIC ${BerkeleyGfx_INCLUDE})

add_executable(SampleProject sample/main.cpp)

target_link_libraries(SampleProject PUBLIC BerkeleyGfx)
target_include_directories(SampleProject PUBLIC ${BerkeleyGfx_INCLUDE})

set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT SampleProject)