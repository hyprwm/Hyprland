#include "Kawase.hpp"

#include "../../OpenGL.hpp"
#include "../../Renderer.hpp"
#include "../../../config/ConfigValue.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

using namespace Render;
using namespace Render::GL;
using namespace NColorManagement;

static SCMSettings blurIntermediateCMSettings(bool toIntermediate) {
    const auto WORKBUFFER   = g_pHyprRenderer->workBufferImageDescription();
    const auto INTERMEDIATE = getDefaultImageDescription();

    auto       settings = toIntermediate ? g_pHyprRenderer->getCMSettings(WORKBUFFER, INTERMEDIATE) : g_pHyprRenderer->getCMSettings(INTERMEDIATE, WORKBUFFER);
    auto&      range    = toIntermediate ? settings.dstTFRange : settings.srcTFRange;
    range.max           = std::max(range.max, sc<float>(WORKBUFFER->value().luminances.max));
    return settings;
}

static float cmBlurNoiseMultiplier(const SCMSettings& settings) {
    const float DEFAULT_LUMINANCE_RANGE = getDefaultImageDescription()->value().getTFMaxLuminance() - settings.srcTFRange.min;
    const float CM_LUMINANCE_RANGE      = settings.srcTFRange.max - settings.srcTFRange.min;

    return std::pow(DEFAULT_LUMINANCE_RANGE / CM_LUMINANCE_RANGE, 1.F / 2.2F);
}

CDualKawaseBlurProvider::CDualKawaseBlurProvider(CHyprOpenGLImpl& impl) : CDualKawaseBlurProvider(impl, makeUnique<CDefaultBlurMaterial>()) {
    ;
}

CDualKawaseBlurProvider::CDualKawaseBlurProvider(CHyprOpenGLImpl& impl, UP<IGLBlurMaterial> material) : m_impl(impl), m_material(std::move(material)) {
    RASSERT(m_material, "Cannot create a dual Kawase blur provider without a material");
    ;
}

eBlurType CDualKawaseBlurProvider::type() const noexcept {
    return m_material->type();
}

bool CDualKawaseBlurProvider::isAnimated() const noexcept {
    return m_material->isAnimated();
}

bool CDualKawaseBlurProvider::requiresLiveBlur() const noexcept {
    return m_material->requirements().liveBlur;
}

float Render::GL::dualKawaseDamageRadius(int64_t size, int64_t passes) {
    const auto blurPasses       = std::clamp(passes, sc<int64_t>(1), sc<int64_t>(8));
    const auto accumulatedScale = (1 << blurPasses) - 1;
    return 2.F * std::max(size, sc<int64_t>(1)) * accumulatedScale;
}

void CDualKawaseBlurProvider::expandDamage(CRegion& damage, float multiplier) const {
    damage.expand(damageRadius() * multiplier);
}

float CDualKawaseBlurProvider::damageRadius() const {
    static auto PBLURSIZE   = CConfigValue<Config::INTEGER>("decoration:blur:size");
    static auto PBLURPASSES = CConfigValue<Config::INTEGER>("decoration:blur:passes");

    return dualKawaseDamageRadius(m_material->blurSizeForDamage(*PBLURSIZE), *PBLURPASSES) + m_material->sampleRadius();
}

SP<CGLFramebuffer> CDualKawaseBlurProvider::blurGL(SP<CGLFramebuffer> source, float strength, const CRegion& originalDamage, const SBlurContext& context) {
    TRACY_GPU_ZONE("RenderBlurFramebufferWithDamage");
    auto&      m_renderData = g_pHyprRenderer->m_renderData;

    const auto BLENDBEFORE = m_impl.m_blend;
    m_impl.blend(false);
    m_impl.setCapStatus(GL_STENCIL_TEST, false);

    CBox                       MONITORBOX = {0, 0, m_renderData.pMonitor->m_transformedSize.x, m_renderData.pMonitor->m_transformedSize.y};

    const auto&                glMatrix = g_pHyprRenderer->projectBoxToTarget(MONITORBOX);

    static auto                PBLURSIZE             = CConfigValue<Config::INTEGER>("decoration:blur:size");
    static auto                PBLURPASSES           = CConfigValue<Config::INTEGER>("decoration:blur:passes");
    static auto                PBLURVIBRANCY         = CConfigValue<Config::FLOAT>("decoration:blur:vibrancy");
    static auto                PBLURVIBRANCYDARKNESS = CConfigValue<Config::FLOAT>("decoration:blur:vibrancy_darkness");

    const auto                 BLUR_PASSES = std::clamp(*PBLURPASSES, sc<int64_t>(1), sc<int64_t>(8));

    CRegion                    outputDamage{originalDamage};

    const SBlurMaterialContext materialContext{
        .blurContext  = context,
        .outputDamage = outputDamage,
        .strength     = strength,
    };
    m_material->prepare(materialContext);

    CRegion workingDamage{outputDamage};
    expandDamage(workingDamage);

    const auto MATERIAL_REQUIREMENTS   = m_material->requirements();
    const bool REQUIRES_PREPARED_INPUT = MATERIAL_REQUIREMENTS.preparedInput;

    const auto PMIRRORFB     = dynamicPointerCast<CGLFramebuffer>(m_renderData.pMonitor->resources()->getUnusedWorkBuffer());
    const auto PMIRRORSWAPFB = dynamicPointerCast<CGLFramebuffer>(m_renderData.pMonitor->resources()->getUnusedWorkBuffer());
    RASSERT(PMIRRORFB && PMIRRORSWAPFB, "Failed to obtain GL work buffers for dual Kawase blur");

    const auto PPREPAREDFB = REQUIRES_PREPARED_INPUT ? dynamicPointerCast<CGLFramebuffer>(m_renderData.pMonitor->resources()->getUnusedWorkBuffer()) : PMIRRORSWAPFB;
    RASSERT(PPREPAREDFB, "Failed to obtain GL prepared work buffer for dual Kawase blur");

    auto currentRenderToFB = PMIRRORFB;

    // Begin with base color adjustments - global brightness and contrast
    // TODO: make this a part of the first pass maybe to save on a drawcall?
    {
        static auto PBLURCONTRAST   = CConfigValue<Config::FLOAT>("decoration:blur:contrast");
        static auto PBLURBRIGHTNESS = CConfigValue<Config::FLOAT>("decoration:blur:brightness");
        static auto PBLEND          = CConfigValue<Config::INTEGER>("render:use_shader_blur_blend");

        PPREPAREDFB->bind();
        PPREPAREDFB->clearAfterInvalidation();

        m_impl.setActiveTexture(GL_TEXTURE0);

        auto currentTex = source->getTexture();

        currentTex->bind();
        currentTex->setTexParameter(GL_TEXTURE_MIN_FILTER, GL_LINEAR);

        WP<CShader> shader;

        const bool  skipCM = !m_impl.m_cmSupported || !g_pHyprRenderer->workBufferImageDescription()->needsCM(getDefaultImageDescription());
        if (!skipCM) {
            const auto settings = blurIntermediateCMSettings(/* toIntermediate */ true);
            shader              = m_impl.useShader(m_impl.getShaderVariant(SH_FRAG_BLURPREPARE, SH_FEAT_CM, settings.sourceTF, settings.targetTF));

            m_impl.passCMUniforms(shader, g_pHyprRenderer->workBufferImageDescription(), getDefaultImageDescription(), false, -1.F, -1, settings);
            shader->setUniformFloat(SHADER_SDR_SATURATION,
                                    m_renderData.pMonitor->m_sdrSaturation > 0 &&
                                            g_pHyprRenderer->workBufferImageDescription()->value().transferFunction == CM_TRANSFER_FUNCTION_ST2084_PQ ?
                                        m_renderData.pMonitor->m_sdrSaturation :
                                        1.0f);
            shader->setUniformFloat(SHADER_SDR_BRIGHTNESS,
                                    m_renderData.pMonitor->m_sdrBrightness > 0 &&
                                            g_pHyprRenderer->workBufferImageDescription()->value().transferFunction == CM_TRANSFER_FUNCTION_ST2084_PQ ?
                                        m_renderData.pMonitor->m_sdrBrightness :
                                        1.0f);
        } else
            shader = m_impl.useShader(m_impl.getShaderVariant(SH_FRAG_BLURPREPARE));

        shader->setUniformMatrix3fv(SHADER_PROJ, 1, GL_TRUE, glMatrix.getMatrix());
        shader->setUniformFloat(SHADER_CONTRAST, *PBLURCONTRAST);
        shader->setUniformFloat(SHADER_BRIGHTNESS, *PBLURBRIGHTNESS);
        shader->setUniformInt(SHADER_TEX, 0);

        glBindVertexArray(shader->getUniformLocation(SHADER_SHADER_VAO));

        if (!workingDamage.empty()) {
            workingDamage.forEachRect([this](const auto& RECT) {
                m_impl.scissor(&RECT, false);
                glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
            });
        }

        glBindVertexArray(0);
        currentRenderToFB = PPREPAREDFB;
    }

    auto drawPass = [&](WP<CShader> shader, ePreparedFragmentShader frag, CRegion* passDamage) {
        if (currentRenderToFB == PMIRRORFB)
            PMIRRORSWAPFB->bind();
        else
            PMIRRORFB->bind();

        m_impl.setActiveTexture(GL_TEXTURE0);

        auto currentTex = currentRenderToFB->getTexture();

        currentTex->bind();
        currentTex->setTexParameter(GL_TEXTURE_MIN_FILTER, GL_LINEAR);

        shader->setUniformMatrix3fv(SHADER_PROJ, 1, GL_TRUE, glMatrix.getMatrix());
        shader->setUniformFloat(SHADER_RADIUS, *PBLURSIZE * strength);
        if (frag == SH_FRAG_BLUR1) {
            shader->setUniformFloat2(SHADER_HALFPIXEL, 0.5f / (m_renderData.pMonitor->m_transformedSize.x / 2.f), 0.5f / (m_renderData.pMonitor->m_transformedSize.y / 2.f));
            shader->setUniformInt(SHADER_PASSES, BLUR_PASSES);
            shader->setUniformFloat(SHADER_VIBRANCY, *PBLURVIBRANCY);
            shader->setUniformFloat(SHADER_VIBRANCY_DARKNESS, *PBLURVIBRANCYDARKNESS);
        } else
            shader->setUniformFloat2(SHADER_HALFPIXEL, 0.5f / (m_renderData.pMonitor->m_transformedSize.x * 2.f), 0.5f / (m_renderData.pMonitor->m_transformedSize.y * 2.f));
        shader->setUniformInt(SHADER_TEX, 0);

        glBindVertexArray(shader->getUniformLocation(SHADER_SHADER_VAO));

        if (!passDamage->empty()) {
            passDamage->forEachRect([this](const auto& RECT) {
                m_impl.scissor(&RECT, false);
                glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
            });
        }

        glBindVertexArray(0);

        if (currentRenderToFB != PMIRRORFB)
            currentRenderToFB = PMIRRORFB;
        else
            currentRenderToFB = PMIRRORSWAPFB;
    };

    PMIRRORFB->bind();
    PMIRRORFB->clearAfterInvalidation();
    PMIRRORSWAPFB->getTexture()->bind();

    CRegion tempDamage{workingDamage};

    auto    shader = m_impl.useShader(m_impl.getShaderVariant(SH_FRAG_BLUR1));
    for (auto i = 1; i <= BLUR_PASSES; ++i) {
        tempDamage = workingDamage.copy().scale(1.f / (1 << i));
        drawPass(shader, SH_FRAG_BLUR1, &tempDamage);
    }

    shader = m_impl.useShader(m_impl.getShaderVariant(SH_FRAG_BLUR2));
    for (auto i = BLUR_PASSES - 1; i >= 0; --i) {
        tempDamage = workingDamage.copy().scale(1.f / (1 << i));
        drawPass(shader, SH_FRAG_BLUR2, &tempDamage);
    }

    {
        static auto PBLURNOISE      = CConfigValue<Config::FLOAT>("decoration:blur:noise");
        static auto PBLURBRIGHTNESS = CConfigValue<Config::FLOAT>("decoration:blur:brightness");

        if (currentRenderToFB == PMIRRORFB)
            PMIRRORSWAPFB->bind();
        else
            PMIRRORFB->bind();

        m_impl.setActiveTexture(GL_TEXTURE0);

        auto currentTex = currentRenderToFB->getTexture();

        currentTex->bind();
        currentTex->setTexParameter(GL_TEXTURE_MIN_FILTER, GL_LINEAR);

        if (REQUIRES_PREPARED_INPUT) {
            m_impl.setActiveTexture(GL_TEXTURE1);
            auto preparedTex = PPREPAREDFB->getTexture();
            preparedTex->bind();
            preparedTex->setTexParameter(GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            m_impl.setActiveTexture(GL_TEXTURE0);
        }

        const bool skipCM            = !m_impl.m_cmSupported || !g_pHyprRenderer->workBufferImageDescription()->needsCM(getDefaultImageDescription());
        float      cmNoiseMultiplier = 1.F;
        if (!skipCM) {
            const auto settings = blurIntermediateCMSettings(/* toIntermediate */ false);
            cmNoiseMultiplier   = cmBlurNoiseMultiplier(settings);
            shader              = m_impl.useShader(m_impl.getShaderVariant(MATERIAL_REQUIREMENTS.finishFragment, SH_FEAT_CM, settings.sourceTF, settings.targetTF));

            m_impl.passCMUniforms(shader, getDefaultImageDescription(), g_pHyprRenderer->workBufferImageDescription(), false, -1.F, -1, settings);
            shader->setUniformFloat(SHADER_SDR_SATURATION,
                                    m_renderData.pMonitor->m_sdrSaturation > 0 &&
                                            g_pHyprRenderer->workBufferImageDescription()->value().transferFunction == CM_TRANSFER_FUNCTION_ST2084_PQ ?
                                        m_renderData.pMonitor->m_sdrSaturation :
                                        1.0f);
            shader->setUniformFloat(SHADER_SDR_BRIGHTNESS,
                                    m_renderData.pMonitor->m_sdrBrightness > 0 &&
                                            g_pHyprRenderer->workBufferImageDescription()->value().transferFunction == CM_TRANSFER_FUNCTION_ST2084_PQ ?
                                        m_renderData.pMonitor->m_sdrBrightness :
                                        1.0f);
        } else
            shader = m_impl.useShader(m_impl.getShaderVariant(MATERIAL_REQUIREMENTS.finishFragment));

        shader->setUniformMatrix3fv(SHADER_PROJ, 1, GL_TRUE, glMatrix.getMatrix());
        shader->setUniformFloat(SHADER_NOISE, *PBLURNOISE * cmNoiseMultiplier);
        shader->setUniformFloat(SHADER_BRIGHTNESS, *PBLURBRIGHTNESS);
        shader->setUniformInt(SHADER_TEX, 0);
        if (REQUIRES_PREPARED_INPUT)
            shader->setUniformInt(SHADER_SHARP_TEX, 1);
        m_material->bindFinish(shader, materialContext);

        glBindVertexArray(shader->getUniformLocation(SHADER_SHADER_VAO));

        if (!outputDamage.empty()) {
            outputDamage.forEachRect([this](const auto& RECT) {
                m_impl.scissor(&RECT, false /* this region is already transformed */);
                glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
            });
        }

        glBindVertexArray(0);

        if (currentRenderToFB != PMIRRORFB)
            currentRenderToFB = PMIRRORFB;
        else
            currentRenderToFB = PMIRRORSWAPFB;
    }

    PMIRRORFB->getTexture()->unbind();
    m_impl.blend(BLENDBEFORE);

    return currentRenderToFB;
}
