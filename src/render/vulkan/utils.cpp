#include "utils.hpp"
#include "../Renderer.hpp"

namespace Render::VK {

    // "UNASSIGNED-CoreValidation-Shader-OutputNotConsumed"
    bool isIgnoredDebugMessage(const std::string& idName) {
        return false;
    }

    std::string resultToStr(VkResult res) {
        return std::to_string(res);
    }

    bool hasExtension(const std::vector<VkExtensionProperties>& extensions, const std::string& name) {
        return std::ranges::any_of(extensions, [name](const auto& ext) { return ext.extensionName == name; });
    };

    int findVkMemType(VkPhysicalDevice dev, VkMemoryPropertyFlags flags, uint32_t bits) {
        VkPhysicalDeviceMemoryProperties2 props = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2,
        };
        vkGetPhysicalDeviceMemoryProperties2(dev, &props);

        for (unsigned i = 0; i < props.memoryProperties.memoryTypeCount; ++i) {
            if (bits & (1 << i)) {
                if ((props.memoryProperties.memoryTypes[i].propertyFlags & flags) == flags)
                    return i;
            }
        }

        return -1;
    }

    bool isDisjoint(const Aquamarine::SDMABUFAttrs& attrs) {
        if (attrs.planes == 1) {
            return false;
        }

        struct stat fdStat;
        if (fstat(attrs.fds[0], &fdStat) != 0)
            return true;

        for (int i = 1; i < attrs.planes; i++) {
            struct stat fdStat2;
            if (fstat(attrs.fds[i], &fdStat2) != 0)
                return true;

            if (fdStat.st_ino != fdStat2.st_ino) {
                return true;
            }
        }

        return false;
    }

    SVkVertShaderData matToVertShader(const std::array<float, 9> mat) {
        return {
            .mat4 =
                {
                    {mat[0], mat[1], 0, mat[2]},
                    {mat[3], mat[4], 0, mat[5]},
                    {0, 0, 1, 0},
                    {0, 0, 0, 1},
                },
            .uvOffset = {0, 0},
            .uvSize   = {1, 1},
        };
    }

    void drawRegionRects(const CRegion& region, VkCommandBuffer cb, bool applyTransform) {
        if (!region.empty()) {
            const CBox max = {{0, 0}, {INT32_MAX, INT32_MAX}};
            region.forEachRect([&](const auto& RECT) {
                CBox box = {RECT.x1, RECT.y1, RECT.x2 - RECT.x1, RECT.y2 - RECT.y1};
                if (applyTransform) {
                    const auto TR = Math::wlTransformToHyprutils(Math::invertTransform(g_pHyprRenderer->m_renderData.pMonitor->m_transform));
                    box.transform(TR, g_pHyprRenderer->m_renderData.pMonitor->m_transformedSize.x, g_pHyprRenderer->m_renderData.pMonitor->m_transformedSize.y);
                }
                box           = box.intersection(max);
                VkRect2D rect = {
                    .offset = {.x = box.x, .y = box.y},
                    .extent = {.width = box.width, .height = box.height},
                };

                if (rect.offset.x < 0 || rect.offset.y < 0)
                    Log::logger->log(Log::WARN, "vkCmdSetScissor tex {}x{}@{}x{}", rect.extent.width, rect.extent.height, rect.offset.x, rect.offset.y);

                vkCmdSetScissor(cb, 0, 1, &rect);
                vkCmdDraw(cb, 4, 1, 0, 0);
            });
        }
    }

    uint8_t passCMUniforms(const SCMSettings& settings, SShaderCM& cmData, SShaderTonemap& tonemapData, SShaderTargetPrimaries& primariesData) {
        uint8_t     usedFeatures = 0;
        const auto& mat          = settings.convertMatrix;

        cmData = {
            .sourceTF        = settings.sourceTF,
            .targetTF        = settings.targetTF,
            .srcRefLuminance = settings.srcRefLuminance,

            .srcTFRange = {settings.srcTFRange.min, settings.srcTFRange.max},
            .dstTFRange = {settings.dstTFRange.min, settings.dstTFRange.max},

            .convertMatrix =
                {
                    {mat[0][0], mat[0][1], mat[0][2]}, //
                    {mat[1][0], mat[1][1], mat[1][2]}, //
                    {mat[2][0], mat[2][1], mat[2][2]}, //,
                },
        };

        if (settings.needsTonemap) {
            usedFeatures |= SH_FEAT_TONEMAP;
            tonemapData = {
                .maxLuminance    = settings.maxLuminance,
                .dstMaxLuminance = settings.dstMaxLuminance,
                .dstRefLuminance = settings.dstRefLuminance,
            };
        }

        if (settings.needsTonemap || settings.needsSDRmod) {
            if (settings.needsSDRmod)
                usedFeatures |= SH_FEAT_SDR_MOD;

            const auto& mat = settings.dstPrimaries2XYZ;
            primariesData   = {
                  .xyz =
                    {
                        {mat[0][0], mat[0][1], mat[0][2]}, //
                        {mat[1][0], mat[1][1], mat[1][2]}, //
                        {mat[2][0], mat[2][1], mat[2][2]}, //,
                    },
                  .sdrSaturation           = settings.sdrSaturation,
                  .sdrBrightnessMultiplier = settings.sdrBrightnessMultiplier,
            };
        }
        return usedFeatures;
    }
}
