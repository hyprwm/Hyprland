#pragma once

#include "Renderer.hpp"
#include "./vulkan/Vulkan.hpp"
#include "./vulkan/Framebuffer.hpp"
#include "./vulkan/RenderPass.hpp"
#include "./vulkan/Pipeline.hpp"
#include "./vulkan/PipelineLayout.hpp"
#include "./vulkan/Shaders.hpp"
#include "desktop/DesktopTypes.hpp"
#include "helpers/Format.hpp"
#include "render/Framebuffer.hpp"
#include "render/Texture.hpp"
#include "render/vulkan/BlurPass.hpp"
#include "render/vulkan/BorderGradientUBO.hpp"
#include "render/vulkan/CommandBuffer.hpp"
#include "render/vulkan/VKElementRenderer.hpp"
#include <cstdint>
#include <drm_fourcc.h>
#include <hyprutils/math/Box.hpp>
#include <vector>

namespace Render::VK {
    class CHyprVKRenderer : public IHyprRenderer {
      public:
        CHyprVKRenderer();

        eType                   type() override;
        void                    startRenderPass() override;
        void                    endRender(const std::function<void()>& renderingDoneCallback = {}) override;
        SP<ITexture>            createStencilTexture(const int width, const int height) override;
        SP<ITexture>            createTexture(bool opaque = false) override;
        SP<ITexture>            createTexture(uint32_t drmFormat, uint8_t* pixels, uint32_t stride, const Vector2D& size, bool keepDataCopy = false, bool opaque = false) override;
        SP<ITexture>            createTexture(const Aquamarine::SDMABUFAttrs&, bool opaque = false) override;
        SP<ITexture>            createTexture(const int width, const int height, unsigned char* const data) override;
        SP<ITexture>            createTexture(cairo_surface_t* cairo) override;
        SP<ITexture>            createTexture(std::span<const float> lut3D, size_t N) override;
        bool                    explicitSyncSupported() override;
        std::vector<SDRMFormat> getDRMFormats() override;
        std::vector<uint64_t>   getDRMFormatModifiers(DRMFormat format) override;
        SP<IFramebuffer>        createFB(const std::string& name = "") override;
        void                    disableScissor() override;
        void                    blend(bool enabled) override;
        void                    drawShadow(const CBox& box, int round, float roundingPower, int range, CHyprColor color, float a) override;
        void                    setViewport(int x, int y, int width, int height) override;
        bool                    reloadShaders(const std::string& path = "") override;

        // TODO fix api
        SP<CVkPipelineLayout> ensurePipelineLayout(CVkPipelineLayout::KEY key);
        SP<CVkPipelineLayout> ensurePipelineLayout(uint32_t vertSize, uint32_t fragSize, uint8_t texCount = 1, uint8_t uboCount = 0);
        SP<CVkRenderPass>     getRenderPass(uint32_t fmt);
        SP<CVKBlurPass>       getBlurPass(uint32_t fmt);
        void                  bindFB(SP<IFramebuffer> fb) override;
        UP<ISyncFDManager>    createSyncFDManager() override;
        WP<IElementRenderer>  elementRenderer() override;

      private:
        SP<ITexture>                                     blurFramebuffer(SP<IFramebuffer> source, float a, CRegion* originalDamage) override;
        void                                             renderOffToMain(SP<IFramebuffer> off) override;
        SP<IRenderbuffer>                                getOrCreateRenderbufferInternal(SP<Aquamarine::IBuffer> buffer, uint32_t fmt) override;
        bool                                             beginRenderInternal(PHLMONITOR pMonitor, CRegion& damage, bool simple = false) override;
        bool                                             beginFullFakeRenderInternal(PHLMONITOR pMonitor, CRegion& damage, SP<IFramebuffer> fb, bool simple = false) override;
        void                                             initRender() override;
        bool                                             initRenderBuffer(SP<Aquamarine::IBuffer> buffer, uint32_t fmt) override;

        void                                             bindPipeline(WP<CVkPipeline> pipeline);
        Vector2D                                         currentRBSize();
        SP<CVKBorderGradientUBO>                         getGradientForWindow(PHLWINDOWREF window);

        void                                             setTexBarriers(SP<ITexture> tex);

        UP<IElementRenderer>                             m_elementRenderer;

        bool                                             m_busy         = false;
        bool                                             m_inRenderPass = false;
        SP<CVKFramebuffer>                               m_hasBoundFB;
        CBox                                             m_viewport;

        Vector2D                                         m_currentRenderbufferSize;

        std::vector<SP<CVkRenderPass>>                   m_renderPassList;
        std::vector<SP<CVKBlurPass>>                     m_blurPassList;
        SP<CVkRenderPass>                                m_currentRenderPass;
        SP<CVkShaders>                                   m_shaders;
        DRMFormat                                        m_currentDrmFormat = DRM_FORMAT_INVALID;

        std::vector<SP<CVkPipelineLayout>>               m_pipelineLayouts;

        WP<CVkPipeline>                                  m_currentPipeline;
        WP<CHyprVkCommandBuffer>                         m_currentCommandBuffer;

        std::map<PHLWINDOWREF, SP<CVKBorderGradientUBO>> m_borderGradients;

        std::vector<SP<ITexture>>                        m_needBarriers;

        friend class CHyprVulkanImpl;
        friend class CVKBlurPass;
        friend class CVKElementRenderer;
    };
}
