#pragma once

#include "berkeley_gfx.hpp"

#include <GLFW/glfw3.h>
#include <vulkan/vulkan.hpp>

#include <functional>

namespace BG
{

  using sDispatch = vk::DispatchLoaderStatic;
  using dDispatch = vk::DispatchLoaderDynamic;
  using DUniqueDebugUtilsMessengerEXT = vk::UniqueHandle<vk::DebugUtilsMessengerEXT, dDispatch>;

  class Renderer
  {
  private:
    GLFWwindow* m_window;

    bool m_isRunning = true;
    const int MAX_FRAMES_IN_FLIGHT = 2;

    int m_width = 1280, m_height = 720;

    double m_timeSpentLast100Frames = 1.0;

    // Vulkan member stuff
    vk::UniqueInstance                 m_instance;
    vk::DispatchLoaderDynamic          m_dispatcher;
    DUniqueDebugUtilsMessengerEXT      m_debugMessenger;
    vk::PhysicalDevice                 m_physicalDevice;
    vk::UniqueDevice                   m_device;
    vk::UniqueSurfaceKHR               m_surface;
    vk::UniqueSwapchainKHR             m_swapchain;
    vk::Format                         m_swapchainFormat;

    vk::Queue                          m_graphcisQueue;
    vk::Queue                          m_computeQueue;
    vk::Queue                          m_transferQueue;

    vk::UniqueCommandPool              m_graphicsCmdPool;
    vk::UniqueCommandPool              m_guiCmdPool;

    VkDescriptorPool                   m_ImGuiDescPool;
    vk::UniqueRenderPass               m_ImGuiRenderPass;

    // Vulkan per-frame stuff
    std::vector<vk::UniqueSemaphore>      m_imageAvailableSemaphores;
    std::vector<vk::UniqueSemaphore>      m_renderFinishedSemaphores;
    std::vector<vk::UniqueFence>          m_inFlightFences;
    std::vector<vk::UniqueFence*>         m_imagesInFlight;
    std::vector<vk::UniqueCommandBuffer>  m_cmdBuffers;
    std::vector<vk::UniqueCommandBuffer>  m_ImGuiCmdBuffers;
    std::vector<vk::UniqueFramebuffer>    m_ImGuiFramebuffer;
    std::vector<vk::UniqueDescriptorPool> m_descPools;

    // Images & image views
    std::vector<vk::Image>                  m_swapchainImages;
    std::vector<vk::UniqueImageView>        m_swapchainImageViews;
    std::vector<std::unique_ptr<BG::Image>> m_depthImages;
    std::vector<vk::UniqueImageView>        m_depthImageViews;

    // Misc components from BG
    std::unique_ptr<MemoryAllocator> m_memoryAllocator;
    std::unique_ptr<TextureSystem>   m_textureSystem;
    std::unique_ptr<Tracker>         m_tracker;

    struct {
      int graphics = -1, compute = -1, transfer = -1;
    } m_selectedPhyDeviceQueueIndices;

    std::string m_name;
    bool m_enableValidationLayers = false;

    void InitWindow();
    void InitVulkan();
    void InitImGui();
    
    // Vulkan Initializations
    void CreateInstance();
    void PickPhysicalDevice();
    void CreateDevice();
    void CreateSurface();
    void CreateSwapChain();
    void CreateCmdPools();
    void CreateCmdBuffers();
    void CreateSemaphore();
    void CreateDescriptorPools();

    void DestroySwapChain();
    void DestroyCmdPools();
    void DestroyCmdBuffers();
    void DestroySemaphore();
    void DestroyDescriptorPools();
    void DestroySurface();
    void DestroyDevice();
    void DestroyImGui();

    void DestroyImGuiSwapChain();

  public:

    bool m_hasDescriptorIndexing = false;

    struct Context
    {
      CommandBuffer& cmdBuffer;
      vk::DescriptorPool descPool;
      vk::ImageView imageView;
      vk::ImageView depthImageView;
      vk::Image image;
      int imageIndex;
      int currentFrame;
      float time;
    };

#ifdef _DEBUG
    Renderer(std::string name, bool enableValidationLayers = true);
#else
    Renderer(std::string name, bool enableValidationLayers = false);
#endif
    ~Renderer();

    std::unique_ptr<Pipeline> CreatePipeline();

    int getWidth();
    int getHeight();

    glm::vec2 getCursorPos();

    inline BG::MemoryAllocator& getMemoryAllocator() { return *m_memoryAllocator; };
    inline BG::TextureSystem& getTextureSystem() { return *m_textureSystem; };
    inline BG::Tracker& getTracker() { return *m_tracker; }

    inline std::vector<vk::Image>& getSwapchainImages() { return m_swapchainImages; };
    inline std::vector<vk::UniqueImageView>& getSwapchainImageViews() { return m_swapchainImageViews; };
    inline std::vector<vk::UniqueImageView>& getDepthImageViews() { return m_depthImageViews; };

    inline vk::Device getDevice() { return m_device.get(); }

    vk::Format getSwapChainFormat();

    vk::UniqueFramebuffer CreateFramebuffer(vk::RenderPass renderpass, std::vector<vk::ImageView>& imageView, int width, int height);
    vk::UniqueFramebuffer CreateFramebuffer(vk::RenderPass renderpass, std::vector<vk::ImageView>& imageView, std::vector<vk::ImageView>& depthView, int width, int height);

    vk::UniqueCommandBuffer AllocCmdBuffer();

    void SubmitCmdBufferNow(vk::CommandBuffer buf, bool wait = true);

    void Run(std::function<void()> init, std::function<void(Context&)> render, std::function<void()> renderGUI, std::function<void()> cleanup);
  };
}