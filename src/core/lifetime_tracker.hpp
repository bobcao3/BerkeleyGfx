#pragma once

#include "berkeley_gfx.hpp"
#include <vulkan/vulkan.hpp>

namespace BG
{
  class Tracker
  {
  private:
    int m_numFramesInFlight = 3;
    int m_currentFrame = 0;

    struct FrameObjects
    {
      std::vector<vk::UniqueFramebuffer> framebuffers;

      void ClearAll();
    };

    std::vector<FrameObjects> m_frames;

  public:
    void DisposeFramebuffer(vk::UniqueFramebuffer fb);

    void NewFrame();

    Tracker(int maxFrames);
  };

}