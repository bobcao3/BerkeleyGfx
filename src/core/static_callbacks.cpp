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

#include <atomic>

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

static std::atomic<uint64_t> __UidCounter(0);

uint64_t BG::GetUID()
{
  return __UidCounter.fetch_add(1);
}