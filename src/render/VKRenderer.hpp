#pragma once

#include "Renderer.hpp"
#include "./vulkan/Vulkan.hpp"
#include "./vulkan/Framebuffer.hpp"
#include "./vulkan/RenderPass.hpp"
#include "./vulkan/Pipeline.hpp"
#include "./vulkan/PipelineLayout.hpp"
#include "./vulkan/Shaders.hpp"
#include "render/Framebuffer.hpp"
#include "render/Texture.hpp"
#include "render/vulkan/CommandBuffer.hpp"
#include <cstdint>
#include <vector>

class CHyprVKRenderer : public IHyprRenderer {
  public:
    CHyprVKRenderer();

    void                    startRenderPass() override;
    void                    endRender(const std::function<void()>& renderingDoneCallback = {}) override;
    SP<ITexture>            createTexture(bool opaque = false) override;
    SP<ITexture>            createTexture(uint32_t drmFormat, uint8_t* pixels, uint32_t stride, const Vector2D& size, bool keepDataCopy = false, bool opaque = false) override;
    SP<ITexture>            createTexture(const Aquamarine::SDMABUFAttrs&, bool opaque = false) override;
    SP<ITexture>            createTexture(const int width, const int height, unsigned char* const data) override;
    SP<ITexture>            createTexture(cairo_surface_t* cairo) override;
    bool                    explicitSyncSupported() override;
    std::vector<SDRMFormat> getDRMFormats() override;
    SP<IFramebuffer>        createFB() override;
    void                    disableScissor() override;
    void                    blend(bool enabled) override;
    void                    drawShadow(const CBox& box, int round, float roundingPower, int range, CHyprColor color, float a) override;

    // TODO fix api
    SP<CVkPipelineLayout> ensurePipelineLayout(CVkPipelineLayout::KEY key);
    SP<CVkPipelineLayout> ensurePipelineLayout(uint32_t vertSize, uint32_t fragSize, uint8_t texCount = 1);
    SP<CVkRenderPass>     getRenderPass(uint32_t fmt);
    void                  bindFB(SP<CHyprVkFramebuffer> fb);

  private:
    SP<IRenderbuffer>                  getOrCreateRenderbufferInternal(SP<Aquamarine::IBuffer> buffer, uint32_t fmt) override;
    bool                               beginRenderInternal(PHLMONITOR pMonitor, CRegion& damage, bool simple = false) override;
    bool                               beginFullFakeRenderInternal(PHLMONITOR pMonitor, CRegion& damage, SP<IFramebuffer> fb, bool simple = false) override;
    void                               initRender() override;
    bool                               initRenderBuffer(SP<Aquamarine::IBuffer> buffer, uint32_t fmt) override;

    void                               draw(CBorderPassElement* element, const CRegion& damage) override;
    void                               draw(CClearPassElement* element, const CRegion& damage) override;
    void                               draw(CFramebufferElement* element, const CRegion& damage) override;
    void                               draw(CPreBlurElement* element, const CRegion& damage) override;
    void                               draw(CRectPassElement* element, const CRegion& damage) override;
    void                               draw(CRendererHintsPassElement* element, const CRegion& damage) override;
    void                               draw(CShadowPassElement* element, const CRegion& damage) override;
    void                               draw(CSurfacePassElement* element, const CRegion& damage) override;
    void                               draw(CTexPassElement* element, const CRegion& damage) override;
    void                               draw(CTextureMatteElement* element, const CRegion& damage) override;

    void                               bindPipeline(WP<CVkPipeline> pipeline);
    Vector2D                           currentRBSize();

    bool                               m_busy         = false;
    bool                               m_inRenderPass = false;
    SP<CHyprVkFramebuffer>             m_hasBoundFB;

    SP<CHyprVkFramebuffer>             m_currentRenderbuffer;
    Vector2D                           m_currentRenderbufferSize;

    std::vector<SP<CVkRenderPass>>     m_renderPassList;
    SP<CVkRenderPass>                  m_currentRenderPass;
    SP<CVkShaders>                     m_shaders;

    std::vector<SP<CVkPipelineLayout>> m_pipelineLayouts;

    WP<CVkPipeline>                    m_currentPipeline;
    WP<CHyprVkCommandBuffer>           m_currentCommandBuffer;

    friend class CHyprVulkanImpl;
};
