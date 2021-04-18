#include "lifetime_tracker.hpp"

void BG::Tracker::FrameObjects::ClearAll()
{
  framebuffers.clear();
}

void BG::Tracker::DisposeFramebuffer(vk::UniqueFramebuffer fb)
{
  m_frames[m_currentFrame].framebuffers.push_back(std::move(fb));
}

void BG::Tracker::NewFrame()
{
  m_currentFrame = (m_currentFrame + 1) % m_numFramesInFlight;

  m_frames[m_currentFrame].ClearAll();
}

BG::Tracker::Tracker(int maxFrames)
  : m_numFramesInFlight(maxFrames)
{
  m_frames.resize(maxFrames);
}
