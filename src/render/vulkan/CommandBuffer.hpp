#pragma once

#include "Device.hpp"

#include "../../helpers/memory/Memory.hpp"
#include "DeviceUser.hpp"
#include "../../render/Texture.hpp"
#include "render/Framebuffer.hpp"
#include "render/vulkan/BlurPass.hpp"
#include "render/vulkan/RenderPass.hpp"
#include <vulkan/vulkan_core.h>

namespace Render::VK {

    class CHyprVKRenderer;
    class CHyprVkFramebuffer;

    class CHyprVkCommandBuffer : public IDeviceUser {
      public:
        CHyprVkCommandBuffer(WP<CHyprVulkanDevice> device);
        ~CHyprVkCommandBuffer();

        struct SImageLayoutSettings {
            VkImageLayout        layout;
            VkPipelineStageFlags stageMask;
            VkAccessFlags        accessMask;
        };

        void            begin();
        void            end(uint64_t signalPoint);
        VkCommandBuffer vk();
        void            changeLayout(VkImage img, const SImageLayoutSettings& src, const SImageLayoutSettings& dst);
        void            changeLayout(WP<IFramebuffer> fb, const SImageLayoutSettings& src, const SImageLayoutSettings& dst);
        bool            busy();
        uint64_t        m_timelinePoint = 0;
        void            useTexture(SP<ITexture> tex);
        void            useFB(SP<CHyprVkFramebuffer> fb);
        void            useRenderPass(SP<CVkRenderPass> rp);
        void            useBlurPass(SP<CVKBlurPass> bp);

        void            resetUsedResources();

      private:
        VkCommandBuffer           m_cmdBuffer       = VK_NULL_HANDLE;
        VkSemaphore               m_waitSemaphore   = VK_NULL_HANDLE;
        VkSemaphore               m_signalSemaphore = VK_NULL_HANDLE;

        bool                      m_recording = false;
        std::vector<SP<ITexture>> m_usedTextures;
        SP<CHyprVkFramebuffer>    m_usedFB;
        SP<CVkRenderPass>         m_usedRenderPass;
        SP<CVKBlurPass>           m_usedBlurPass;

        // friend class CHyprVKRenderer;
        // friend class CHyprVulkanImpl;
    };
}