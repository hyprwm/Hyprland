#include "Kawase.hpp"

#include "../../OpenGL.hpp"
#include "../../Renderer.hpp"
#include "../../../config/ConfigValue.hpp"

#include <algorithm>

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

CDualKawaseBlurProvider::CDualKawaseBlurProvider(CHyprOpenGLImpl& impl) : m_impl(impl) {
    ;
}

eBlurType CDualKawaseBlurProvider::type() const noexcept {
    return eBlurType::BLUR_DUAL_KAWASE;
}

bool CDualKawaseBlurProvider::isAnimated() const noexcept {
    return false;
}

bool CDualKawaseBlurProvider::requiresLiveBlur() const noexcept {
    return false;
}

float Render::GL::dualKawaseDamageRadius(int64_t size, int64_t passes) {
    const auto blurPasses       = std::clamp(passes, sc<int64_t>(1), sc<int64_t>(8));
    const auto accumulatedScale = (1 << blurPasses) - 1;
    return 2.F * std::max(size, sc<int64_t>(1)) * accumulatedScale;
}

void CDualKawaseBlurProvider::expandDamage(CRegion& damage, float multiplier) const {
    damage.expand(damageRadius() * multiplier);
}

ePreparedFragmentShader CDualKawaseBlurProvider::finishFragment() const noexcept {
    return SH_FRAG_BLURFINISH;
}

bool CDualKawaseBlurProvider::requiresPreparedInput() const noexcept {
    return false;
}

void CDualKawaseBlurProvider::updateProviderState(const SBlurContext& context, const CRegion& outputDamage) {
    ;
}

void CDualKawaseBlurProvider::setFinishUniforms(WP<CShader> shader, float strength, const SBlurContext& context) const {
    ;
}

float CDualKawaseBlurProvider::damageRadius() const {
    static auto PBLURSIZE   = CConfigValue<Config::INTEGER>("decoration:blur:size");
    static auto PBLURPASSES = CConfigValue<Config::INTEGER>("decoration:blur:passes");

    return dualKawaseDamageRadius(std::clamp<Config::INTEGER>(*PBLURSIZE, 1, 40), std::clamp<Config::INTEGER>(*PBLURPASSES, 1, 8));
}

SP<CGLFramebuffer> CDualKawaseBlurProvider::blurGL(SP<CGLFramebuffer> source, float strength, const CRegion& originalDamage, const SBlurContext& context) {
    TRACY_GPU_ZONE("RenderBlurFramebufferWithDamage");
    auto&      m_renderData = g_pHyprRenderer->m_renderData;

    const auto BLENDBEFORE = m_impl.m_blend;
    m_impl.blend(false);
    m_impl.setCapStatus(GL_STENCIL_TEST, false);

    const auto  TRANSFORM  = Math::wlTransformToHyprutils(Math::invertTransform(m_renderData.pMonitor->m_transform));
    CBox        MONITORBOX = {0, 0, m_renderData.pMonitor->m_transformedSize.x, m_renderData.pMonitor->m_transformedSize.y};

    const auto& glMatrix = g_pHyprRenderer->projectBoxToTarget(MONITORBOX, TRANSFORM);

    static auto PBLURSIZE             = CConfigValue<Config::INTEGER>("decoration:blur:size");
    static auto PBLURPASSES           = CConfigValue<Config::INTEGER>("decoration:blur:passes");
    static auto PBLURVIBRANCY         = CConfigValue<Config::FLOAT>("decoration:blur:vibrancy");
    static auto PBLURVIBRANCYDARKNESS = CConfigValue<Config::FLOAT>("decoration:blur:vibrancy_darkness");

    const auto  BLUR_PASSES = std::clamp(*PBLURPASSES, sc<int64_t>(1), sc<int64_t>(8));

    CRegion     outputDamage{originalDamage};
    outputDamage.transform(Math::wlTransformToHyprutils(Math::invertTransform(m_renderData.pMonitor->m_transform)), m_renderData.pMonitor->m_transformedSize.x,
                           m_renderData.pMonitor->m_transformedSize.y);

    updateProviderState(context, outputDamage);

    CRegion workingDamage{outputDamage};
    expandDamage(workingDamage);

    const bool REQUIRES_PREPARED_INPUT = requiresPreparedInput();

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

        glActiveTexture(GL_TEXTURE0);

        auto currentTex = source->getTexture();

        currentTex->bind();
        currentTex->setTexParameter(GL_TEXTURE_MIN_FILTER, GL_LINEAR);

        WP<CShader> shader;

        const bool  skipCM = !m_impl.m_cmSupported || !g_pHyprRenderer->workBufferImageDescription()->needsCM(getDefaultImageDescription());
        if (!skipCM) {
            shader = m_impl.useShader(m_impl.getShaderVariant(SH_FRAG_BLURPREPARE, SH_FEAT_CM));

            m_impl.passCMUniforms(shader, g_pHyprRenderer->workBufferImageDescription(), getDefaultImageDescription(), false, -1.F, -1,
                                  blurIntermediateCMSettings(/* toIntermediate */ true));
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

        const auto& prepareMatrix = g_pHyprRenderer->projectBoxToTarget(MONITORBOX, *PBLEND ? HYPRUTILS_TRANSFORM_NORMAL : TRANSFORM);
        shader->setUniformMatrix3fv(SHADER_PROJ, 1, GL_TRUE, prepareMatrix.getMatrix());
        shader->setUniformFloat(SHADER_CONTRAST, *PBLURCONTRAST);
        shader->setUniformFloat(SHADER_BRIGHTNESS, *PBLURBRIGHTNESS);
        shader->setUniformInt(SHADER_TEX, 0);

        glBindVertexArray(shader->getUniformLocation(SHADER_SHADER_VAO));

        if (!workingDamage.empty()) {
            workingDamage.forEachRect([this](const auto& RECT) {
                m_impl.scissor(&RECT, false /* this region is already transformed */);
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

        glActiveTexture(GL_TEXTURE0);

        auto currentTex = currentRenderToFB->getTexture();

        currentTex->bind();
        currentTex->setTexParameter(GL_TEXTURE_MIN_FILTER, GL_LINEAR);

        shader->setUniformMatrix3fv(SHADER_PROJ, 1, GL_TRUE, glMatrix.getMatrix());
        shader->setUniformFloat(SHADER_RADIUS, *PBLURSIZE * strength);
        if (frag == SH_FRAG_BLUR1) {
            shader->setUniformFloat2(SHADER_HALFPIXEL, 0.5f / (m_renderData.pMonitor->m_pixelSize.x / 2.f), 0.5f / (m_renderData.pMonitor->m_pixelSize.y / 2.f));
            shader->setUniformInt(SHADER_PASSES, BLUR_PASSES);
            shader->setUniformFloat(SHADER_VIBRANCY, *PBLURVIBRANCY);
            shader->setUniformFloat(SHADER_VIBRANCY_DARKNESS, *PBLURVIBRANCYDARKNESS);
        } else
            shader->setUniformFloat2(SHADER_HALFPIXEL, 0.5f / (m_renderData.pMonitor->m_pixelSize.x * 2.f), 0.5f / (m_renderData.pMonitor->m_pixelSize.y * 2.f));
        shader->setUniformInt(SHADER_TEX, 0);

        glBindVertexArray(shader->getUniformLocation(SHADER_SHADER_VAO));

        if (!passDamage->empty()) {
            passDamage->forEachRect([this](const auto& RECT) {
                m_impl.scissor(&RECT, false /* this region is already transformed */);
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

        glActiveTexture(GL_TEXTURE0);

        auto currentTex = currentRenderToFB->getTexture();

        currentTex->bind();
        currentTex->setTexParameter(GL_TEXTURE_MIN_FILTER, GL_LINEAR);

        if (REQUIRES_PREPARED_INPUT) {
            glActiveTexture(GL_TEXTURE1);
            auto preparedTex = PPREPAREDFB->getTexture();
            preparedTex->bind();
            preparedTex->setTexParameter(GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glActiveTexture(GL_TEXTURE0);
        }

        const bool skipCM = !m_impl.m_cmSupported || !g_pHyprRenderer->workBufferImageDescription()->needsCM(getDefaultImageDescription());
        if (!skipCM) {
            shader = m_impl.useShader(m_impl.getShaderVariant(finishFragment(), SH_FEAT_CM));

            m_impl.passCMUniforms(shader, getDefaultImageDescription(), g_pHyprRenderer->workBufferImageDescription(), false, -1.F, -1,
                                  blurIntermediateCMSettings(/* toIntermediate */ false));
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
            shader = m_impl.useShader(m_impl.getShaderVariant(finishFragment()));

        shader->setUniformMatrix3fv(SHADER_PROJ, 1, GL_TRUE, glMatrix.getMatrix());
        shader->setUniformFloat(SHADER_NOISE, *PBLURNOISE);
        shader->setUniformFloat(SHADER_BRIGHTNESS, *PBLURBRIGHTNESS);
        shader->setUniformInt(SHADER_TEX, 0);
        if (REQUIRES_PREPARED_INPUT)
            shader->setUniformInt(SHADER_SHARP_TEX, 1);
        setFinishUniforms(shader, strength, context);

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
