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

extern VKAPI_ATTR VkBool32 VKAPI_CALL STATIC_DebugCallback(
  VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
  VkDebugUtilsMessageTypeFlagsEXT messageType,
  const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
  void* pUserData);

void BG::Renderer::InitWindow()
{
  glfwInit();

  // Use glfw to check for Vulkan support.
  if (glfwVulkanSupported() == GLFW_FALSE)
  {
    spdlog::error("No Vulkan supported found on system!");
    throw std::runtime_error("No vulkan support");
  }

  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

  m_window = glfwCreateWindow(m_width, m_height, "Berkeley Gfx", nullptr, nullptr);
}

void BG::Renderer::InitVulkan()
{
  CreateInstance();
  PickPhysicalDevice();
  CreateDevice();
  CreateSurface();
  CreateSwapChain();
  CreateCmdPools();
  CreateCmdBuffers();
  CreateDescriptorPools();
  CreateSemaphore();
}

void BG::Renderer::InitImGui()
{
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO(); (void)io;
  //io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
  //io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

  // Setup Dear ImGui style
  ImGui::StyleColorsDark();
  //ImGui::StyleColorsClassic();

  // Create Descriptor Pool
  {
    VkDescriptorPoolSize pool_sizes[] =
    {
        { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
    };
    VkDescriptorPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    pool_info.maxSets = 1000 * IM_ARRAYSIZE(pool_sizes);
    pool_info.poolSizeCount = (uint32_t)IM_ARRAYSIZE(pool_sizes);
    pool_info.pPoolSizes = pool_sizes;
    vkCreateDescriptorPool(m_device.get(), &pool_info, nullptr, &m_ImGuiDescPool);
  }

  // Create renderpass
  {
    std::vector<vk::AttachmentReference> attachmentRefs = { { 0, vk::ImageLayout::eColorAttachmentOptimal } };

    std::vector<vk::SubpassDescription> subpass;
    vk::SubpassDescription mainSubpass;
    mainSubpass.setPipelineBindPoint(vk::PipelineBindPoint::eGraphics);
    mainSubpass.setColorAttachments(attachmentRefs);
    subpass.push_back(mainSubpass);

    vk::AttachmentDescription attachment;
    attachment.format = m_swapchainFormat;
    attachment.samples = vk::SampleCountFlagBits::e1;
    attachment.initialLayout = vk::ImageLayout::ePresentSrcKHR;
    attachment.finalLayout = vk::ImageLayout::ePresentSrcKHR;

    attachment.loadOp = vk::AttachmentLoadOp::eLoad;
    attachment.storeOp = vk::AttachmentStoreOp::eStore;
    attachment.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
    attachment.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;

    std::vector< vk::AttachmentDescription> attachments = { attachment };

    m_ImGuiRenderPass = m_device->createRenderPassUnique({ {}, attachments, subpass });
  }

  // Create framebuffers
  for (auto& imageView : m_swapchainImageViews)
  {
    vk::FramebufferCreateInfo framebufferInfo;
    framebufferInfo.setRenderPass(m_ImGuiRenderPass.get());
    framebufferInfo.setAttachmentCount(1);
    framebufferInfo.setPAttachments(&imageView.get());
    framebufferInfo.setWidth(m_width);
    framebufferInfo.setHeight(m_height);
    framebufferInfo.setLayers(1);

    m_ImGuiFramebuffer.push_back(m_device->createFramebufferUnique(framebufferInfo));
  }

  // Setup Platform/Renderer backends
  ImGui_ImplGlfw_InitForVulkan(m_window, true);
  ImGui_ImplVulkan_InitInfo init_info = {};
  init_info.Instance = m_instance.get();
  init_info.PhysicalDevice = m_physicalDevice;
  init_info.Device = m_device.get();
  init_info.QueueFamily = m_selectedPhyDeviceQueueIndices.graphics;
  init_info.Queue = m_graphcisQueue;
  init_info.PipelineCache = nullptr;
  init_info.DescriptorPool = m_ImGuiDescPool;
  init_info.Allocator = nullptr;
  init_info.MinImageCount = m_swapchainImages.size();
  init_info.ImageCount = m_swapchainImages.size();
  init_info.CheckVkResultFn = nullptr;
  ImGui_ImplVulkan_Init(&init_info, m_ImGuiRenderPass.get());

  m_ImGuiCmdBuffers[0]->begin(vk::CommandBufferBeginInfo{ {}, nullptr });
  ImGui_ImplVulkan_CreateFontsTexture(m_ImGuiCmdBuffers[0].get());
  m_ImGuiCmdBuffers[0]->end();

  SubmitCmdBufferNow(m_ImGuiCmdBuffers[0].get(), true);

  ImGui_ImplVulkan_DestroyFontUploadObjects();
}

void BG::Renderer::CreateInstance()
{
  // Enumerate all available instance layers.
  std::vector<vk::LayerProperties> layerProperties = vk::enumerateInstanceLayerProperties();

  bool standardValidationFound = false;
  for (auto prop : layerProperties)
  {
    char* layerName = prop.layerName;
    if (std::string(layerName) == "VK_LAYER_KHRONOS_validation")
    {
      standardValidationFound = true;
      break;
    }
  }

  m_enableValidationLayers &= standardValidationFound;

  // Initialize a Vulkan instance with the validation layers enabled and extensions required by glfw.
  std::vector<const char*> glfwExtensionsVec;

  uint32_t glfwExtensionCount = 0;
  const char** glfwExtensions;
  glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
  for (int i = 0; i < glfwExtensionCount; i++) glfwExtensionsVec.push_back(glfwExtensions[i]);

  std::vector<const char*> instanceLayers;
  if (m_enableValidationLayers)
  {
    instanceLayers.push_back("VK_LAYER_KHRONOS_validation");
    glfwExtensionsVec.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
  }

  vk::ApplicationInfo appInfo{ m_name.c_str(), VK_MAKE_VERSION(1, 0, 0), "Berkeley Gfx", VK_MAKE_VERSION(1, 0, 0), VK_API_VERSION_1_0 };
  vk::InstanceCreateInfo createInfo{
    {}, /* flags */
    &appInfo, /* pApplicationInfo */
    instanceLayers, /* ppEnabledLayerNames */
    glfwExtensionsVec /* ppEnabledExtensionNames */
  };

  m_instance = vk::createInstanceUnique(createInfo, nullptr);
  m_dispatcher = vk::DispatchLoaderDynamic(m_instance.get(), vkGetInstanceProcAddr);

  if (m_enableValidationLayers)
  {
    vk::DebugUtilsMessengerCreateInfoEXT createInfo{
      {}, /* flags */
      vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning | vk::DebugUtilsMessageSeverityFlagBitsEXT::eError, /* messageSeverity */
      vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral | vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance, /* messageType  */
      STATIC_DebugCallback, /* pfnUserCallback  */
      nullptr /* pUserData */
    };

    m_debugMessenger = m_instance->createDebugUtilsMessengerEXTUnique(createInfo, nullptr, m_dispatcher);
  }
}

void BG::Renderer::PickPhysicalDevice()
{
  std::vector<vk::PhysicalDevice> devices = m_instance->enumeratePhysicalDevices();

  int highest_score = 0;

  for (auto& device : devices)
  {
    auto deviceProperty = device.getProperties();
    auto memoryProperty = device.getMemoryProperties();

    int deviceLocalHeapIndex = 0;
    size_t totalLocalMemorySize = 0;

    for (const auto& type : memoryProperty.memoryTypes)
    {
      if (type.propertyFlags & vk::MemoryPropertyFlagBits::eDeviceLocal)
      {
        deviceLocalHeapIndex = type.heapIndex;
        totalLocalMemorySize += memoryProperty.memoryHeaps[type.heapIndex].size;
      }
    }

    bool isDiscrete = deviceProperty.deviceType == vk::PhysicalDeviceType::eDiscreteGpu;

    spdlog::info("{} ({} MB), isDiscrete={}", deviceProperty.deviceName, (totalLocalMemorySize >> 20), isDiscrete);

    auto queueFamilies = device.getQueueFamilyProperties();

    int i = 0, graphicsQueue = -1, computeQueue = -1, transferQueue = -1;

    for (const auto& queueFamily : queueFamilies)
    {
      spdlog::info("- Queue {}", i);

      if (queueFamily.queueFlags & vk::QueueFlagBits::eGraphics) {
        if (graphicsQueue == -1) graphicsQueue = i;
        spdlog::info("  - Graphics");
      }

      if (queueFamily.queueFlags & vk::QueueFlagBits::eCompute) {
        if (computeQueue == -1) computeQueue = i;
        spdlog::info("  - Compute");
      }

      if (queueFamily.queueFlags & vk::QueueFlagBits::eTransfer) {
        if (transferQueue == -1) transferQueue = i;
        spdlog::info("  - Transfer");
      }

      i++;
    }

    int score = (totalLocalMemorySize >> 20) + (isDiscrete ? 2000 : 0);

    if (score > highest_score && graphicsQueue != -1)
    {
      highest_score = score;
      m_physicalDevice = device;

      m_selectedPhyDeviceQueueIndices.graphics = graphicsQueue;
      m_selectedPhyDeviceQueueIndices.compute = computeQueue;
      m_selectedPhyDeviceQueueIndices.transfer = transferQueue;
    }
  }
}

void BG::Renderer::CreateDevice()
{
  const float graphicsQueuePriority = 1.0;
  const float computeQueuePriority = 0.5;
  const float transferQueuePriority = 0.0;

  std::vector<vk::DeviceQueueCreateInfo> queueCreateInfo;
  queueCreateInfo.push_back({ {}, uint32_t(m_selectedPhyDeviceQueueIndices.graphics), 1, &graphicsQueuePriority });

  if (m_selectedPhyDeviceQueueIndices.compute != m_selectedPhyDeviceQueueIndices.graphics)
  {
    queueCreateInfo.push_back({ {}, uint32_t(m_selectedPhyDeviceQueueIndices.compute), 1, &computeQueuePriority });
  }

  if (m_selectedPhyDeviceQueueIndices.transfer != m_selectedPhyDeviceQueueIndices.graphics)
  {
    queueCreateInfo.push_back({ {}, uint32_t(m_selectedPhyDeviceQueueIndices.transfer), 1, &transferQueuePriority });
  }

  std::vector<const char*> deviceLayers;

  std::vector<const char*> deviceExtensions;

  deviceExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);

  vk::PhysicalDeviceFeatures deviceFeatures;

  m_device = m_physicalDevice.createDeviceUnique({ {}, queueCreateInfo, deviceLayers, deviceExtensions, &deviceFeatures }, nullptr);
  
  m_graphcisQueue = m_device->getQueue(m_selectedPhyDeviceQueueIndices.graphics, 0);

  if (m_selectedPhyDeviceQueueIndices.compute != m_selectedPhyDeviceQueueIndices.graphics)
  {
    m_computeQueue = m_device->getQueue(m_selectedPhyDeviceQueueIndices.compute, 0);
  }
  else
  {
    m_computeQueue = m_graphcisQueue;
  }

  if (m_selectedPhyDeviceQueueIndices.transfer != m_selectedPhyDeviceQueueIndices.graphics)
  {
    m_transferQueue = m_device->getQueue(m_selectedPhyDeviceQueueIndices.transfer, 0);
  }
  else
  {
    m_transferQueue = m_graphcisQueue;
  }

  if (!glfwGetPhysicalDevicePresentationSupport(m_instance.get(), m_physicalDevice, m_selectedPhyDeviceQueueIndices.graphics))
  {
    throw std::runtime_error("No presentation support on the graphcis queue");
  }

  m_memoryAllocator = std::make_shared<BG::MemoryAllocator>(m_physicalDevice, m_device.get(), m_instance.get(), MAX_FRAMES_IN_FLIGHT);
}

void BG::Renderer::CreateSurface()
{
  VkSurfaceKHR surface;
  glfwCreateWindowSurface(m_instance.get(), m_window, NULL, &surface);
  m_surface = vk::UniqueSurfaceKHR(surface, *m_instance);
}

void BG::Renderer::CreateSwapChain()
{
  int width, height;
  glfwGetFramebufferSize(m_window, &width, &height);

  m_width = width;
  m_height = height;

  vk::Extent2D actualExtent = {
      static_cast<uint32_t>(width),
      static_cast<uint32_t>(height)
  };

  auto surfaceSupport = m_physicalDevice.getSurfaceSupportKHR(m_selectedPhyDeviceQueueIndices.graphics, m_surface.get());

  if (!surfaceSupport)
  {
    spdlog::error("No surface support");
    throw std::runtime_error("No surface support");
  }

  auto surfaceCapability = m_physicalDevice.getSurfaceCapabilitiesKHR(m_surface.get());
  auto formatCapability = m_physicalDevice.getSurfaceFormatsKHR(m_surface.get());
  auto presentModes = m_physicalDevice.getSurfacePresentModesKHR(m_surface.get());

  vk::SurfaceFormatKHR surfaceFormat;

  if (std::find(formatCapability.begin(), formatCapability.end(), vk::Format::eR8G8B8A8Srgb) != formatCapability.end())
  {
    surfaceFormat = vk::Format::eR8G8B8A8Srgb;
    spdlog::info("Using format: RGBA8 SRGB");
  }
  else if (std::find(formatCapability.begin(), formatCapability.end(), vk::Format::eB8G8R8A8Srgb) != formatCapability.end())
  {
    surfaceFormat = vk::Format::eB8G8R8A8Srgb;
    spdlog::info("Using format: BGRA8 SRGB");
  }
  else
  {
    spdlog::info("No suitable surface format");
    throw std::runtime_error("No suitable surface format");
  }

  vk::PresentModeKHR presentMode = vk::PresentModeKHR::eFifo;

  if (std::find(presentModes.begin(), presentModes.end(), vk::PresentModeKHR::eMailbox) != presentModes.end())
  {
    presentMode = vk::PresentModeKHR::eMailbox;
  }

  uint32_t imageCount = std::min(surfaceCapability.minImageCount + 1, surfaceCapability.maxImageCount);

  vk::SwapchainCreateInfoKHR createInfo{ {}, m_surface.get(), imageCount, surfaceFormat.format, surfaceFormat.colorSpace, actualExtent, 1, vk::ImageUsageFlagBits::eColorAttachment };

  createInfo.setImageSharingMode(vk::SharingMode::eExclusive);
  createInfo.setPreTransform(surfaceCapability.currentTransform);
  createInfo.setCompositeAlpha(vk::CompositeAlphaFlagBitsKHR::eOpaque);
  createInfo.setPresentMode(presentMode);
  createInfo.setClipped(true);

  m_swapchain = m_device->createSwapchainKHRUnique(createInfo, nullptr);

  m_swapchainImages = m_device->getSwapchainImagesKHR(m_swapchain.get());

  for (const auto& image : m_swapchainImages)
  {
    vk::ImageViewCreateInfo imageviewInfo;

    imageviewInfo.setImage(image);
    imageviewInfo.setViewType(vk::ImageViewType::e2D);
    imageviewInfo.setFormat(surfaceFormat.format);
    imageviewInfo.setComponents({ vk::ComponentSwizzle::eIdentity, vk::ComponentSwizzle::eIdentity, vk::ComponentSwizzle::eIdentity, vk::ComponentSwizzle::eIdentity });
    imageviewInfo.setSubresourceRange({ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });

    m_swapchainImageViews.push_back(m_device->createImageViewUnique(imageviewInfo));
  }

  m_swapchainFormat = surfaceFormat.format;

  for (const auto& i : m_swapchainImages)
  {
    auto image = m_memoryAllocator->AllocImage2D(glm::uvec2(m_width, m_height), 1, vk::Format::eD32Sfloat, vk::ImageUsageFlagBits::eDepthStencilAttachment);
    m_depthImages.push_back(image);

    vk::ImageViewCreateInfo viewInfo;
    viewInfo.image = image->image;
    viewInfo.viewType = vk::ImageViewType::e2D;
    viewInfo.format = vk::Format::eD32Sfloat;
    viewInfo.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eDepth;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    m_depthImageViews.push_back(m_device->createImageViewUnique(viewInfo));
  }
}

void BG::Renderer::CreateCmdPools()
{
  m_graphicsCmdPool = m_device->createCommandPoolUnique({ vk::CommandPoolCreateFlagBits::eResetCommandBuffer, uint32_t(m_selectedPhyDeviceQueueIndices.graphics) });
  m_guiCmdPool = m_device->createCommandPoolUnique({ vk::CommandPoolCreateFlagBits::eResetCommandBuffer, uint32_t(m_selectedPhyDeviceQueueIndices.graphics) });
}

void BG::Renderer::CreateCmdBuffers()
{
  for (int i = 0; i < m_swapchainImages.size(); i++)
  {
    m_cmdBuffers.push_back(AllocCmdBuffer());
    m_ImGuiCmdBuffers.push_back(std::move(m_device->allocateCommandBuffersUnique({ m_guiCmdPool.get(), vk::CommandBufferLevel::ePrimary, 1 })[0]));
  }
}

void BG::Renderer::CreateSemaphore()
{
  for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
  {
    m_imageAvailableSemaphores.push_back(m_device->createSemaphoreUnique({}));
    m_renderFinishedSemaphores.push_back(m_device->createSemaphoreUnique({}));
    m_inFlightFences.push_back(m_device->createFenceUnique({}));
  }

  m_imagesInFlight.resize(m_swapchainImages.size(), nullptr);
}

void BG::Renderer::CreateDescriptorPools()
{
  vk::DescriptorPoolSize poolSize;
  poolSize.descriptorCount = 1024;

  vk::DescriptorPoolCreateInfo info;
  info.poolSizeCount = 1;
  info.pPoolSizes = &poolSize;
  info.maxSets = 256;

  for (int i = 0; i < m_swapchainImages.size(); i++)
  {
    m_descPools.push_back(m_device->createDescriptorPoolUnique(info));
  }
}

BG::Renderer::Renderer(std::string name, bool enableValidationLayers)
  : m_name(name), m_enableValidationLayers(enableValidationLayers), m_tracker(std::make_shared<BG::Tracker>(MAX_FRAMES_IN_FLIGHT))
{
  InitWindow();
  InitVulkan();
  InitImGui();

  m_textureSystem = std::make_shared<TextureSystem>(m_device.get(), *m_memoryAllocator, *this);
}

BG::Renderer::~Renderer()
{
  glfwTerminate();
}

void BG::Renderer::Run(std::function<void()> init, std::function<void(Context&)> render, std::function<void()> renderGUI, std::function<void()> cleanup)
{
  init();

  int imageIndex = 0;
  size_t currentFrame = 0;

  std::mutex m;
  std::condition_variable cv;
  bool ready = false;
  bool processed = false;

  std::thread guiThread([&] {
    while (m_isRunning)
    {
      // Wait until main thread starts new frame
      std::unique_lock<std::mutex> lk(m);
      cv.wait(lk, [&] { return ready; });
      ready = false;

      if (!m_isRunning) break;

      ImGui_ImplVulkan_NewFrame();
      
      ImGui::NewFrame();

      renderGUI();

      ImGui::Text("Last 100 frames took %fms", m_timeSpentLast100Frames * 1000.0);
      ImGui::Text("FPS = %f", 100.0 / m_timeSpentLast100Frames);

      ImGui::Render();
      ImDrawData* draw_data = ImGui::GetDrawData();

      {
        auto cmdBuf = m_ImGuiCmdBuffers[imageIndex].get();

        cmdBuf.begin(vk::CommandBufferBeginInfo{ {}, nullptr });

        vk::ClearValue clearValue{};

        vk::RenderPassBeginInfo info = {};
        info.renderPass = m_ImGuiRenderPass.get();
        info.framebuffer = m_ImGuiFramebuffer[imageIndex].get();
        info.renderArea.extent.width = m_width;
        info.renderArea.extent.height = m_height;
        info.clearValueCount = 1;
        info.pClearValues = &clearValue;

        cmdBuf.beginRenderPass(info, vk::SubpassContents::eInline);

        ImGui_ImplVulkan_RenderDrawData(draw_data, cmdBuf);

        cmdBuf.endRenderPass();
        cmdBuf.end();
      }

      // GUI finished processing
      processed = true;
      lk.unlock();
      cv.notify_one();
    }
    });

  size_t frameCount = 0;
  auto startTime = std::chrono::high_resolution_clock::now();

  auto startTimeSteady = std::chrono::steady_clock::now();

  while (!glfwWindowShouldClose(m_window))
  {
    // Check for window messages to process.
    glfwPollEvents();

    imageIndex = m_device->acquireNextImageKHR(m_swapchain.get(), UINT64_MAX, m_imageAvailableSemaphores[currentFrame].get(), nullptr);

    if (m_imagesInFlight[imageIndex] != nullptr)
    {
      m_device->waitForFences(1, &m_imagesInFlight[imageIndex]->get(), true, UINT64_MAX);
    }
    else if (std::find(m_imagesInFlight.begin(), m_imagesInFlight.end(), &m_inFlightFences[currentFrame]) != m_imagesInFlight.end())
    {
      // This happens when there are more images than the in-flight limit
      m_device->waitForFences(1, &m_inFlightFences[currentFrame].get(), true, UINT64_MAX);
    }

    m_imagesInFlight[imageIndex] = &m_inFlightFences[currentFrame];

    // Trigger GUI thread (GLFW is single threaded, therefore glfw related setup must be on main thread)
    ImGui_ImplGlfw_NewFrame();
    {
      std::lock_guard<std::mutex> lk(m);
      ready = true;
    }
    cv.notify_one();

    // Begin new frame on main thread
    m_device->resetDescriptorPool(m_descPools[imageIndex].get());
    m_memoryAllocator->NewFrame();
    m_tracker->NewFrame();

    float time = (std::chrono::steady_clock::now() - startTimeSteady).count() * 1e-9;
    CommandBuffer bgCmdBuf(m_device.get(), m_cmdBuffers[imageIndex].get(), *m_tracker);
    Context ctx{
      bgCmdBuf,
      m_descPools[imageIndex].get(),
      m_swapchainImageViews[imageIndex].get(), m_depthImageViews[imageIndex].get(),
      m_swapchainImages[imageIndex],
      imageIndex, int(currentFrame), time };

    render(ctx);

    // wait for the GUI thread
    {
      std::unique_lock<std::mutex> lk(m);
      cv.wait(lk, [&] { return processed; });
    }
    processed = false;

    vk::SubmitInfo submitInfo;

    std::vector<vk::PipelineStageFlags> waitStages = { vk::PipelineStageFlagBits::eColorAttachmentOutput };

    std::vector<vk::CommandBuffer> submitBuffers = { m_cmdBuffers[imageIndex].get(), m_ImGuiCmdBuffers[imageIndex].get() };

    submitInfo.setWaitSemaphores(m_imageAvailableSemaphores[ctx.currentFrame].get());
    submitInfo.setWaitDstStageMask(waitStages);
    submitInfo.setCommandBuffers(submitBuffers);
    submitInfo.setSignalSemaphores(m_renderFinishedSemaphores[ctx.currentFrame].get());

    m_device->resetFences(1, &m_imagesInFlight[imageIndex]->get());

    m_graphcisQueue.submit(1, &submitInfo, m_inFlightFences[ctx.currentFrame].get());

    uint32_t imageIndexU32 = imageIndex;

    vk::PresentInfoKHR presentInfo;
    presentInfo.setWaitSemaphores(m_renderFinishedSemaphores[ctx.currentFrame].get());
    presentInfo.setSwapchains(m_swapchain.get());
    presentInfo.pImageIndices = &imageIndexU32;

    m_graphcisQueue.presentKHR(presentInfo);

    currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;

    frameCount++;
    if (frameCount % 100 == 99)
    {
      auto now = std::chrono::high_resolution_clock::now();
      m_timeSpentLast100Frames = (now - startTime).count() * 1e-9;
      startTime = now;
    }
  }

  m_isRunning = false;

  {
    std::lock_guard<std::mutex> lk(m);
    ready = true;
  }
  cv.notify_one();

  guiThread.join();

  m_device->waitIdle();

  m_device->destroyDescriptorPool(m_ImGuiDescPool);

  cleanup();
}

std::shared_ptr<Pipeline> BG::Renderer::CreatePipeline()
{
  return std::make_shared<Pipeline>(m_device.get());
}

int Renderer::getWidth()
{
  return m_width;
}

int Renderer::getHeight()
{
  return m_height;
}

vk::Format BG::Renderer::getSwapChainFormat()
{
  return m_swapchainFormat;
}

vk::UniqueFramebuffer BG::Renderer::CreateFramebuffer(vk::RenderPass& renderpass, std::vector<vk::ImageView>& imageView, int width, int height)
{
  vk::FramebufferCreateInfo framebufferInfo;
  framebufferInfo.setRenderPass(renderpass);
  framebufferInfo.setAttachments(imageView);
  framebufferInfo.setWidth(width);
  framebufferInfo.setHeight(height);
  framebufferInfo.setLayers(1);

  return m_device->createFramebufferUnique(framebufferInfo);
}

vk::UniqueFramebuffer BG::Renderer::CreateFramebuffer(vk::RenderPass& renderpass, std::vector<vk::ImageView>& imageView, std::vector<vk::ImageView>& depthView, int width, int height)
{
  vk::FramebufferCreateInfo framebufferInfo;
  framebufferInfo.setRenderPass(renderpass);
  framebufferInfo.setAttachments(imageView);
  framebufferInfo.setWidth(width);
  framebufferInfo.setHeight(height);
  framebufferInfo.setLayers(1);

  return m_device->createFramebufferUnique(framebufferInfo);
}

vk::UniqueCommandBuffer BG::Renderer::AllocCmdBuffer()
{
  return std::move(m_device->allocateCommandBuffersUnique({ m_graphicsCmdPool.get(), vk::CommandBufferLevel::ePrimary, 1 })[0]);
}

void BG::Renderer::SubmitCmdBufferNow(vk::CommandBuffer buf, bool wait)
{
  vk::Fence fence;
  vk::SubmitInfo submitInfo;

  if (wait)
  {
    fence = m_device->createFence({});
  }

  submitInfo.setCommandBuffers(buf);

  m_graphcisQueue.submit(1, &submitInfo, fence);

  if (wait)
  {
    m_device->waitForFences(1, &fence, true, UINT64_MAX);
    m_device->destroyFence(fence);
  }
}
