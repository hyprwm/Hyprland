#pragma once

#include "Renderer.hpp"
#include "./vulkan/Vulkan.hpp"
#include "./vulkan/Framebuffer.hpp"
#include "./vulkan/RenderPass.hpp"
#include "./vulkan/Pipeline.hpp"
#include "./vulkan/PipelineLayout.hpp"
#include "./vulkan/Shaders.hpp"

class CHyprVKRenderer : public IHyprRenderer {
  public:
    CHyprVKRenderer();

    void endRender(const std::function<void()>& renderingDoneCallback = {}) override;

  private:
    bool                                beginRenderInternal(PHLMONITOR pMonitor, CRegion& damage, bool simple = false) override;
    bool                                beginFullFakeRenderInternal(PHLMONITOR pMonitor, CRegion& damage, CFramebuffer* fb, bool simple = false) override;
    void                                initRender() override;
    bool                                initRenderBuffer(SP<Aquamarine::IBuffer> buffer, uint32_t fmt) override;

    std::vector<SP<CHyprVkFramebuffer>> m_renderBuffers;
    SP<CHyprVkFramebuffer>              m_currentRenderbuffer;

    std::vector<SP<CVkRenderPass>>      m_renderPassList;
    SP<CVkRenderPass>                   m_currentRenderPass;
    SP<CVkShaders>                      m_shaders;
    SP<CVkPipelineLayout>               m_pipelineLayout;
    SP<CVkPipeline>                     m_texturePipeline;

    friend class CHyprVulkanImpl;
};
