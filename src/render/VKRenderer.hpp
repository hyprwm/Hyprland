#pragma once

#include "Renderer.hpp"
#include "./vulkan/Vulkan.hpp"

class CHyprVKRenderer : public IHyprRenderer {
  public:
    CHyprVKRenderer();

    void endRender(const std::function<void()>& renderingDoneCallback = {}) override;

  private:
    bool beginRenderInternal(PHLMONITOR pMonitor, CRegion& damage, SP<IHLBuffer> buffer = {}, CFramebuffer* fb = nullptr, bool simple = false) override;

    friend class CHyprVulkanImpl;
};
