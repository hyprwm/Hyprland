#include "Ripple.hpp"

#include "Kawase.hpp"

#include "../../Renderer.hpp"
#include "../../Shader.hpp"
#include "../../ShaderLoader.hpp"
#include "../../../config/ConfigValue.hpp"
#include "../../../event/EventBus.hpp"
#include "../../../pointer/PointerManager.hpp"
#include "../../../state/MonitorState.hpp"
#include "../../../managers/input/InputManager.hpp"

#include <algorithm>
#include <cmath>
#include <ranges>
#include <vector>

using namespace Render;
using namespace Render::GL;

static constexpr float MAX_RIPPLE_DISPLACEMENT = 32.F;

CRippleBlurMaterial::CRippleBlurMaterial() {
    m_listeners.mouseButton = Event::bus()->m_events.input.mouse.button.listen([this](IPointer::SButtonEvent event, Event::SCallbackInfo&) {
        m_lastMouseHeldCoord.reset();

        if (event.state == WL_POINTER_BUTTON_STATE_PRESSED) {
            m_mouseIsHeld = true;
            return;
        }

        if (event.state != WL_POINTER_BUTTON_STATE_RELEASED)
            return;

        m_mouseIsHeld = false;

        static auto PRIPPLESTRENGTH = CConfigValue<Config::FLOAT>("decoration:blur:ripple:strength");
        static auto PRIPPLEDURATION = CConfigValue<Config::FLOAT>("decoration:blur:ripple:duration");
        static auto PBLURENABLED    = CConfigValue<Config::INTEGER>("decoration:blur:enabled");

        if (!*PBLURENABLED || *PRIPPLESTRENGTH <= 0.F || *PRIPPLEDURATION <= 0.F)
            return;

        addImpulse();
    });

    m_listeners.mouseMotion = Event::bus()->m_events.input.mouse.move.listen([this](Vector2D pos, Event::SCallbackInfo&) {
        if (!m_mouseIsHeld)
            return;

        static auto PRIPPLESTRENGTH = CConfigValue<Config::FLOAT>("decoration:blur:ripple:strength");
        static auto PRIPPLEDURATION = CConfigValue<Config::FLOAT>("decoration:blur:ripple:duration");
        static auto PBLURENABLED    = CConfigValue<Config::INTEGER>("decoration:blur:enabled");

        if (!*PBLURENABLED || *PRIPPLESTRENGTH <= 0.F || *PRIPPLEDURATION <= 0.F)
            return;

        if (m_lastMouseHeldCoord) {
            const auto Δ = (*m_lastMouseHeldCoord - g_pInputManager->getMouseCoordsInternal()).size();
            if (Δ < 6.9F) // arbitrarily chosen by me, fuck you
                return;
        }

        addImpulse();

        m_lastMouseHeldCoord = g_pInputManager->getMouseCoordsInternal();
    });
}

CRippleBlurProvider::CRippleBlurProvider(CHyprOpenGLImpl& impl) : CDualKawaseBlurProvider(impl, makeUnique<CRippleBlurMaterial>()) {
    ;
}

eBlurType CRippleBlurMaterial::type() const noexcept {
    return eBlurType::BLUR_RIPPLE;
}

void CRippleBlurMaterial::addImpulse() {
    static auto PRIPPLESTRENGTH = CConfigValue<Config::FLOAT>("decoration:blur:ripple:strength");
    static auto PRIPPLERADIUS   = CConfigValue<Config::FLOAT>("decoration:blur:ripple:radius");
    static auto PRIPPLEWIDTH    = CConfigValue<Config::FLOAT>("decoration:blur:ripple:width");
    static auto PRIPPLEDURATION = CConfigValue<Config::FLOAT>("decoration:blur:ripple:duration");
    static auto PBLURENABLED    = CConfigValue<Config::INTEGER>("decoration:blur:enabled");

    const auto  POS      = g_pInputManager->getMouseCoordsInternal();
    const auto  PMONITOR = State::monitorState()->query().vec(POS).run();
    if (!PMONITOR)
        return;

    const auto NOW     = Time::steadyNow();
    auto&      impulse = m_impulses[m_nextImpulse];
    const auto AGE     = std::chrono::duration<float>(NOW - impulse.started).count();
    if (impulse.occupied && AGE >= 0.F && AGE < *PRIPPLEDURATION)
        damageImpulse(impulse);

    impulse = SImpulse{
        .globalPosition = POS,
        .started        = NOW,
        .monitor        = PMONITOR,
        .damageReach    = rippleOutputReach(*PRIPPLERADIUS, *PRIPPLEWIDTH),
        .occupied       = true,
    };

    m_nextImpulse = (m_nextImpulse + 1) % MAX_IMPULSES;
    damageImpulse(impulse);
}

bool CRippleBlurMaterial::isAnimated(const CRenderingContext& context) const noexcept {
    static auto PRIPPLESTRENGTH = CConfigValue<Config::FLOAT>("decoration:blur:ripple:strength");
    static auto PRIPPLEDURATION = CConfigValue<Config::FLOAT>("decoration:blur:ripple:duration");
    static auto PBLURENABLED    = CConfigValue<Config::INTEGER>("decoration:blur:enabled");

    if (!*PBLURENABLED || *PRIPPLESTRENGTH <= 0.F || *PRIPPLEDURATION <= 0.F || !context.sceneMonitor)
        return false;

    const auto now = Time::steadyNow();
    return std::ranges::any_of(m_impulses, [&](const auto& impulse) { return impulseIsActive(impulse, context.sceneMonitor, now, *PRIPPLEDURATION); });
}

SBlurMaterialRequirements CRippleBlurMaterial::requirements() const noexcept {
    return {
        .finishFragment = SH_FRAG_RIPPLEFINISH,
    };
}

void CRippleBlurMaterial::bindFinish(WP<CShader> shader, const SBlurMaterialContext& context) const {
    static auto        PRIPPLESTRENGTH = CConfigValue<Config::FLOAT>("decoration:blur:ripple:strength");
    static auto        PRIPPLERADIUS   = CConfigValue<Config::FLOAT>("decoration:blur:ripple:radius");
    static auto        PRIPPLEWIDTH    = CConfigValue<Config::FLOAT>("decoration:blur:ripple:width");
    static auto        PRIPPLEDURATION = CConfigValue<Config::FLOAT>("decoration:blur:ripple:duration");

    const auto         monitor  = context.renderingContext.sceneMonitor;
    const auto         now      = Time::steadyNow();
    const auto         duration = std::max(*PRIPPLEDURATION, 0.001F);

    std::vector<float> impulses;
    impulses.reserve(MAX_IMPULSES * 4);

    for (const auto& impulse : m_impulses) {
        if (!impulseIsActive(impulse, monitor, now, duration))
            continue;

        const auto position = (impulse.globalPosition - monitor->m_position) * monitor->m_scale;

        const auto age = std::chrono::duration<float>(now - impulse.started).count();
        impulses.insert(impulses.end(), {sc<float>(position.x), sc<float>(position.y), age, 0.F});
    }

    const auto count = sc<GLsizei>(impulses.size() / 4);
    shader->setUniformInt(SHADER_RIPPLE_COUNT, count);
    if (count > 0)
        shader->setUniform4fv(SHADER_RIPPLE_IMPULSES, count, impulses);

    shader->setUniformFloat4(SHADER_RIPPLE_PARAMS, duration, std::max(*PRIPPLERADIUS, 1.F), std::max(*PRIPPLEWIDTH, 1.F),
                             std::clamp(*PRIPPLESTRENGTH, 0.F, MAX_RIPPLE_DISPLACEMENT) * std::clamp(context.strength, 0.F, 1.F));
}

float CRippleBlurMaterial::sampleRadius() const {
    static auto PRIPPLESTRENGTH = CConfigValue<Config::FLOAT>("decoration:blur:ripple:strength");

    return std::ceil(std::clamp(*PRIPPLESTRENGTH, 0.F, MAX_RIPPLE_DISPLACEMENT));
}

void CRippleBlurMaterial::damageImpulse(const SImpulse& impulse) const {
    const auto monitor = impulse.monitor.lock();
    if (!monitor)
        return;

    const auto local  = (impulse.globalPosition - monitor->m_position) * monitor->m_scale;
    const auto left   = std::floor(local.x - impulse.damageReach);
    const auto top    = std::floor(local.y - impulse.damageReach);
    const auto right  = std::ceil(local.x + impulse.damageReach);
    const auto bottom = std::ceil(local.y + impulse.damageReach);

    monitor->m_blurFBDirty = true;
    monitor->addDamage(CBox{left, top, right - left, bottom - top});
}

bool CRippleBlurMaterial::impulseIsActive(const SImpulse& impulse, PHLMONITORREF monitor, const Time::steady_tp& now, float duration) const {
    if (!impulse.occupied || !monitor || impulse.monitor != monitor)
        return false;

    const auto age = std::chrono::duration<float>(now - impulse.started).count();
    return age >= 0.F && age < duration;
}

float Render::GL::rippleDamageRadius(int64_t size, int64_t passes, float displacement) {
    return dualKawaseDamageRadius(size, passes) + std::ceil(std::clamp(displacement, 0.F, MAX_RIPPLE_DISPLACEMENT));
}

float Render::GL::rippleOutputReach(float radius, float width) {
    return std::ceil(std::max(radius, 0.F) + std::max(width, 0.F));
}
