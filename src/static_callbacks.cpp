#include "renderer.hpp"
#include "pipelines.hpp"
#include "command_buffer.hpp"
#include "buffer.hpp"
#include "texture_system.hpp"
#include "lifetime_tracker.hpp"

#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_vulkan.h"

#include <GLFW/glfw3native.h>

using namespace BG;

VKAPI_ATTR VkBool32 VKAPI_CALL STATIC_DebugCallback(
  VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
  VkDebugUtilsMessageTypeFlagsEXT messageType,
  const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
  void* pUserData) {

  spdlog::error("validation layer: {}", pCallbackData->pMessage);

  // throw std::runtime_error("Vulkan Validation Error");

  return VK_FALSE;
}