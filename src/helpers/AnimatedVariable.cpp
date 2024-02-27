#include "AnimatedVariable.hpp"
#include "../managers/AnimationManager.hpp"
#include "../config/ConfigManager.hpp"

CAnimatedVariable::CAnimatedVariable() {
    ; // dummy var
}

void CAnimatedVariable::create(ANIMATEDVARTYPE type, SAnimationPropertyConfig* pAnimConfig, void* pWindow, AVARDAMAGEPOLICY policy) {
    m_eVarType      = type;
    m_eDamagePolicy = policy;
    m_pConfig       = pAnimConfig;
    m_pWindow       = pWindow;

    m_bDummy = false;
}

void CAnimatedVariable::create(ANIMATEDVARTYPE type, std::any val, SAnimationPropertyConfig* pAnimConfig, void* pWindow, AVARDAMAGEPOLICY policy) {
    create(type, pAnimConfig, pWindow, policy);

    try {
        switch (type) {
            case AVARTYPE_FLOAT: {
                const auto V = std::any_cast<float>(val);
                m_fValue     = V;
                m_fGoal      = V;
                break;
            }
            case AVARTYPE_VECTOR: {
                const auto V = std::any_cast<Vector2D>(val);
                m_vValue     = V;
                m_vGoal      = V;
                break;
            }
            case AVARTYPE_COLOR: {
                const auto V = std::any_cast<CColor>(val);
                m_cValue     = V;
                m_cGoal      = V;
                break;
            }
            default: ASSERT(false); break;
        }
    } catch (std::exception& e) {
        Debug::log(ERR, "CAnimatedVariable create error: {}", e.what());
        RASSERT(false, "CAnimatedVariable create error: {}", e.what());
    }
}

CAnimatedVariable::~CAnimatedVariable() {
    unregister();
}

void CAnimatedVariable::unregister() {
    if (!g_pAnimationManager)
        return;
    std::erase_if(g_pAnimationManager->m_vAnimatedVariables, [&](const auto& other) { return other == this; });
    m_bIsRegistered = false;
    disconnectFromActive();
}

void CAnimatedVariable::registerVar() {
    if (!m_bIsRegistered)
        g_pAnimationManager->m_vAnimatedVariables.push_back(this);
    m_bIsRegistered = true;
}

int CAnimatedVariable::getDurationLeftMs() {
    return std::max(
        (int)(m_pConfig->pValues->internalSpeed * 100) - (int)std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - animationBegin).count(), 0);
}

float CAnimatedVariable::getPercent() {
    const auto DURATIONPASSED = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - animationBegin).count();
    return std::clamp((DURATIONPASSED / 100.f) / m_pConfig->pValues->internalSpeed, 0.f, 1.f);
}

float CAnimatedVariable::getCurveValue() {
    if (!m_bIsBeingAnimated)
        return 1.f;

    const auto SPENT = getPercent();

    if (SPENT >= 1.f)
        return 1.f;

    return g_pAnimationManager->getBezier(m_pConfig->pValues->internalBezier)->getYForPoint(SPENT);
}

void CAnimatedVariable::connectToActive() {
    g_pAnimationManager->scheduleTick(); // otherwise the animation manager will never pick this up

    if (!m_bIsConnectedToActive)
        g_pAnimationManager->m_vActiveAnimatedVariables.push_back(this);

    m_bIsConnectedToActive = true;
}

void CAnimatedVariable::disconnectFromActive() {
    std::erase_if(g_pAnimationManager->m_vActiveAnimatedVariables, [&](const auto& other) { return other == this; });
    m_bIsConnectedToActive = false;
}