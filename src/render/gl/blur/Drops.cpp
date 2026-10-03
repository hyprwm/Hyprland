#include "Drops.hpp"

#include "../../Renderer.hpp"
#include "../../Shader.hpp"
#include "../../ShaderLoader.hpp"
#include "../../../config/ConfigValue.hpp"
#include "../../../event/EventBus.hpp"

#include <algorithm>
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

CDropsBlurMaterial::CDropsBlurMaterial() : CGlassBlurMaterial(eBlurType::BLUR_DROPS, SH_FRAG_DROPSFINISH, true) {
    m_configListener = Event::bus()->m_events.config.props_refreshed.listen([this](const bool) {
        const auto SPEED = dropsSpeed();
        m_animationClock.update(Time::steadyNow(), SPEED);
    });
}

CDropsBlurProvider::CDropsBlurProvider(CHyprOpenGLImpl& impl) : CGlassBlurProvider(impl, makeUnique<CDropsBlurMaterial>()) {
    ;
}

bool CDropsBlurMaterial::isAnimated(CRenderContext& ctx) const noexcept {
    static auto PBLURENABLED     = CConfigValue<Config::INTEGER>("decoration:blur:enabled");
    static auto PGLASSREFRACTION = CConfigValue<Config::FLOAT>("decoration:blur:glass:refraction");
    static auto PGLASSROUGHNESS  = CConfigValue<Config::FLOAT>("decoration:blur:glass:roughness");

    const auto  SPEED = dropsSpeed();
    if (!ctx.readOnlyEffects())
        m_animationClock.update(ctx.effectTime(), SPEED);
    return *PBLURENABLED && SPEED > 0.F && (*PGLASSREFRACTION > 0.F || *PGLASSROUGHNESS > 0.F);
}

void CDropsBlurMaterial::bindFinish(CRenderContext& ctx, WP<CShader> shader, const SBlurMaterialContext& context) const {
    CGlassBlurMaterial::bindFinish(ctx, shader, context);
    shader->setUniformFloat(SHADER_TIME, animationPhase(ctx));

    const CBox patternBox = context.blurContext.patternBox.value_or(CBox{});

    shader->setUniformFloat2(SHADER_DROPS_POSITION, sc<float>(patternBox.x), sc<float>(patternBox.y));
}

float CDropsBlurMaterial::animationPhase(CRenderContext& ctx) const {
    const auto SPEED = dropsSpeed();
    const auto NOW   = ctx.effectTime();
    const auto TIME  = ctx.readOnlyEffects() ? m_animationClock.sample(NOW) : m_animationClock.update(NOW, SPEED);
    if (SPEED <= 0.F)
        return 0.F;

    const auto PHASE = sc<float>(std::fmod(TIME * DROPS_BASE_SPEED, DROPS_PATTERN_PERIOD));
    return std::max(PHASE, 0.00002F);
}
