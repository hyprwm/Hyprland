#include "AnimatedVariable.hpp"
#include "../managers/AnimationManager.hpp"
#include "../config/ConfigManager.hpp"

CBaseAnimatedVariable::CBaseAnimatedVariable(eAnimatedVarType type) : m_Type(type) {
    ; // dummy var
}

void CBaseAnimatedVariable::create(SAnimationPropertyConfig* pAnimConfig, PHLWINDOW pWindow, eAVarDamagePolicy policy) {
    m_eDamagePolicy = policy;
    m_pConfig       = pAnimConfig;
    m_pWindow       = pWindow;

    m_bDummy = false;
}

void CBaseAnimatedVariable::create(SAnimationPropertyConfig* pAnimConfig, PHLLS pLayer, eAVarDamagePolicy policy) {
    m_eDamagePolicy = policy;
    m_pConfig       = pAnimConfig;
    m_pLayer        = pLayer;

    m_bDummy = false;
}

void CBaseAnimatedVariable::create(SAnimationPropertyConfig* pAnimConfig, PHLWORKSPACE pWorkspace, eAVarDamagePolicy policy) {
    m_eDamagePolicy = policy;
    m_pConfig       = pAnimConfig;
    m_pWorkspace    = pWorkspace;

    m_bDummy = false;
}

void CBaseAnimatedVariable::create(SAnimationPropertyConfig* pAnimConfig, eAVarDamagePolicy policy) {
    m_eDamagePolicy = policy;
    m_pConfig       = pAnimConfig;

    m_bDummy = false;
}

CBaseAnimatedVariable::~CBaseAnimatedVariable() {
    unregister();
}

void CBaseAnimatedVariable::unregister() {
    if (!g_pAnimationManager)
        return;
    std::erase_if(g_pAnimationManager->m_vAnimatedVariables, [&](const auto& other) { return other == this; });
    m_bIsRegistered = false;
    disconnectFromActive();
}

void CBaseAnimatedVariable::registerVar() {
    if (!m_bIsRegistered)
        g_pAnimationManager->m_vAnimatedVariables.push_back(this);
    m_bIsRegistered = true;
}

int CBaseAnimatedVariable::getDurationLeftMs() {
    return std::max(
        (int)(m_pConfig->pValues->internalSpeed * 100) - (int)std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - animationBegin).count(), 0);
}

float CBaseAnimatedVariable::getPercent() {
    const auto DURATIONPASSED = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - animationBegin).count();
    return std::clamp((DURATIONPASSED / 100.f) / m_pConfig->pValues->internalSpeed, 0.f, 1.f);
}

float CBaseAnimatedVariable::getCurveValue() {
    if (!m_bIsBeingAnimated)
        return 1.f;

    const auto SPENT = getPercent();

    if (SPENT >= 1.f)
        return 1.f;

    return g_pAnimationManager->getBezier(m_pConfig->pValues->internalBezier)->getYForPoint(SPENT);
}

void CBaseAnimatedVariable::connectToActive() {
    g_pAnimationManager->scheduleTick(); // otherwise the animation manager will never pick this up

    if (!m_bIsConnectedToActive)
        g_pAnimationManager->m_vActiveAnimatedVariables.push_back(this);

    m_bIsConnectedToActive = true;
}

void CBaseAnimatedVariable::disconnectFromActive() {
    std::erase_if(g_pAnimationManager->m_vActiveAnimatedVariables, [&](const auto& other) { return other == this; });
    m_bIsConnectedToActive = false;
}
