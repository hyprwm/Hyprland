#include "Drops.hpp"

#include "../../Renderer.hpp"
#include "../../Shader.hpp"
#include "../../ShaderLoader.hpp"
#include "../../../config/ConfigValue.hpp"
#include "../../../event/EventBus.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>

using namespace Render;
using namespace Render::GL;

static constexpr float  MAX_DROPS_SPEED      = 10.F;
static constexpr double DROPS_BASE_SPEED     = 0.055;
static constexpr double DROPS_PATTERN_PERIOD = 256.0;

static float            dropsSpeed() {
    static auto PDROPSSPEED = CConfigValue<Config::FLOAT>("decoration:blur:drops:speed");
    return std::clamp(*PDROPSSPEED, 0.F, MAX_DROPS_SPEED);
}

CDropsBlurProvider::CDropsBlurProvider(CHyprOpenGLImpl& impl) : CGlassBlurProvider(impl, eBlurType::BLUR_DROPS, SH_FRAG_DROPSFINISH), m_lastAnimationUpdate(Time::steadyNow()) {
    m_configListener = Event::bus()->m_events.config.props_refreshed.listen([this](const bool) { updateAnimation(dropsSpeed()); });
}

bool CDropsBlurProvider::isAnimated() const noexcept {
    static auto PBLURENABLED     = CConfigValue<Config::INTEGER>("decoration:blur:enabled");
    static auto PGLASSREFRACTION = CConfigValue<Config::FLOAT>("decoration:blur:glass:refraction");
    static auto PGLASSROUGHNESS  = CConfigValue<Config::FLOAT>("decoration:blur:glass:roughness");

    const auto  SPEED = dropsSpeed();
    updateAnimation(SPEED);
    return *PBLURENABLED && SPEED > 0.F && (*PGLASSREFRACTION > 0.F || *PGLASSROUGHNESS > 0.F);
}

bool CDropsBlurProvider::requiresPreparedInput() const noexcept {
    return true;
}

void CDropsBlurProvider::setFinishUniforms(WP<CShader> shader, float strength, const SBlurContext& context) const {
    CGlassBlurProvider::setFinishUniforms(shader, strength, context);
    updateAnimation(dropsSpeed());
    shader->setUniformFloat(SHADER_TIME, animationPhase());

    CBox transformedPatternBox;
    if (context.patternBox && g_pHyprRenderer->m_renderData.pMonitor) {
        const auto MONITOR    = g_pHyprRenderer->m_renderData.pMonitor;
        transformedPatternBox = *context.patternBox;
        transformedPatternBox.transform(Math::wlTransformToHyprutils(Math::invertTransform(MONITOR->m_transform)), MONITOR->m_transformedSize.x, MONITOR->m_transformedSize.y);
    }

    shader->setUniformFloat2(SHADER_DROPS_POSITION, sc<float>(transformedPatternBox.x), sc<float>(transformedPatternBox.y));
}

void CDropsBlurProvider::updateAnimation(float speed) const {
    const auto NOW = Time::steadyNow();
    m_animationTime += std::chrono::duration<double>(NOW - m_lastAnimationUpdate).count() * m_previousSpeed;
    m_lastAnimationUpdate = NOW;
    m_previousSpeed       = speed;
}

float CDropsBlurProvider::animationPhase() const {
    if (m_previousSpeed <= 0.F)
        return 0.F;

    const auto PHASE = sc<float>(std::fmod(m_animationTime * DROPS_BASE_SPEED, DROPS_PATTERN_PERIOD));
    return std::max(PHASE, 0.00002F);
}
