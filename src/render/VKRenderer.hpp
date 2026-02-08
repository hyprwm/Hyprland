#pragma once

#include "Renderer.hpp"
#include "./vulkan/Vulkan.hpp"
#include "./vulkan/Framebuffer.hpp"
#include "./vulkan/RenderPass.hpp"

class CHyprVKRenderer : public IHyprRenderer {
  public:
    CHyprVKRenderer();

    void endRender(const std::function<void()>& renderingDoneCallback = {}) override;

  private:
    bool beginRenderInternal(PHLMONITOR pMonitor, CRegion& damage, SP<IHLBuffer> buffer = {}, CFramebuffer* fb = nullptr, bool simple = false) override;
    bool beginFullFakeRenderInternal(PHLMONITOR pMonitor, CRegion& damage, CFramebuffer* fb, bool simple = false) override;
    void initRender() override;
    bool initRenderBuffer(SP<Aquamarine::IBuffer> buffer, uint32_t fmt) override;

    std::vector<SP<CHyprVkFramebuffer>> m_renderBuffers;
    SP<CHyprVkFramebuffer>              m_currentRenderbuffer;

    std::vector<SP<CVkRenderPass>>      m_renderPassList;
    SP<CVkRenderPass>                   m_currentRenderPass;

    friend class CHyprVulkanImpl;
};
