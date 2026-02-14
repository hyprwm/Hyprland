#pragma once

#include "Renderer.hpp"
#include "./vulkan/Vulkan.hpp"
#include "./vulkan/Framebuffer.hpp"
#include "./vulkan/RenderPass.hpp"
#include "./vulkan/Pipeline.hpp"
#include "./vulkan/PipelineLayout.hpp"
#include "./vulkan/Shaders.hpp"
#include "render/Texture.hpp"
#include "render/vulkan/CommandBuffer.hpp"
#include <vector>

class CHyprVKRenderer : public IHyprRenderer {
  public:
    CHyprVKRenderer();

    void                    endRender(const std::function<void()>& renderingDoneCallback = {}) override;
    SP<ITexture>            createTexture(bool opaque = false) override;
    SP<ITexture>            createTexture(uint32_t drmFormat, uint8_t* pixels, uint32_t stride, const Vector2D& size, bool keepDataCopy = false, bool opaque = false) override;
    SP<ITexture>            createTexture(const Aquamarine::SDMABUFAttrs&, void* image, bool opaque = false) override;
    SP<ITexture>            createTexture(const int width, const int height, unsigned char* const data) override;
    SP<ITexture>            createTexture(cairo_surface_t* cairo) override;
    void*                   createImage(const SP<Aquamarine::IBuffer> buffer) override;
    bool                    explicitSyncSupported() override;
    std::vector<SDRMFormat> getDRMFormats() override;
    SP<IFramebuffer>        createFB() override;

    // TODO fix api
    SP<CVkPipelineLayout> ensurePipelineLayout(CVkPipelineLayout::KEY key);
    SP<CVkPipelineLayout> ensurePipelineLayout(uint32_t vertSize, uint32_t fragSize);

  private:
    bool                                beginRenderInternal(PHLMONITOR pMonitor, CRegion& damage, bool simple = false) override;
    bool                                beginFullFakeRenderInternal(PHLMONITOR pMonitor, CRegion& damage, SP<IFramebuffer> fb, bool simple = false) override;
    void                                initRender() override;
    bool                                initRenderBuffer(SP<Aquamarine::IBuffer> buffer, uint32_t fmt) override;

    void                                draw(CBorderPassElement* element, const CRegion& damage) override;
    void                                draw(CClearPassElement* element, const CRegion& damage) override;
    void                                draw(CFramebufferElement* element, const CRegion& damage) override;
    void                                draw(CPreBlurElement* element, const CRegion& damage) override;
    void                                draw(CRectPassElement* element, const CRegion& damage) override;
    void                                draw(CRendererHintsPassElement* element, const CRegion& damage) override;
    void                                draw(CShadowPassElement* element, const CRegion& damage) override;
    void                                draw(CSurfacePassElement* element, const CRegion& damage) override;
    void                                draw(CTexPassElement* element, const CRegion& damage) override;
    void                                draw(CTextureMatteElement* element, const CRegion& damage) override;

    void                                bindPipeline(WP<CVkPipeline> pipeline);

    std::vector<SP<CHyprVkFramebuffer>> m_renderBuffers;
    SP<CHyprVkFramebuffer>              m_currentRenderbuffer;

    std::vector<SP<CVkRenderPass>>      m_renderPassList;
    SP<CVkRenderPass>                   m_currentRenderPass;
    SP<CVkShaders>                      m_shaders;

    std::vector<SP<CVkPipelineLayout>>  m_pipelineLayouts;

    WP<CVkPipeline>                     m_currentPipeline;
    WP<CHyprVkCommandBuffer>            m_currentCommandBuffer;

    friend class CHyprVulkanImpl;
};
