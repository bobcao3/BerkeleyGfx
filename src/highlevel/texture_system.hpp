#pragma once

#include "berkeley_gfx.hpp"

#include <vulkan/vulkan.hpp>

namespace BG
{

  class TextureSystem
  {
  private:
    vk::Device m_device;
    MemoryAllocator& m_allocator;
    Renderer& m_renderer;

    std::vector<std::unique_ptr<Image>> m_images;
    std::vector<vk::UniqueImageView> m_imageViews;

    vk::UniqueSampler m_samplerBilinear;

  public:
    struct Handle
    {
      int index;
    };

    Handle AddTexture(uint8_t* imageBuffer, int width, int height, size_t size, vk::Format format = vk::Format::eR8G8B8Srgb);

    TextureSystem(vk::Device device, MemoryAllocator& allocator, Renderer& renderer);

    inline int GetNumImageViews() { return m_imageViews.size(); }

    inline vk::ImageView GetImageView(Handle id) { return m_imageViews[id.index].get(); }
    inline vk::Sampler GetSampler() { return m_samplerBilinear.get(); }
  };

}