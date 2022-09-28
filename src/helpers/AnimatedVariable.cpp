#include "AnimatedVariable.hpp"
#include "../managers/AnimationManager.hpp"
#include "../config/ConfigManager.hpp"

CAnimatedVariable::CAnimatedVariable() {
    ; // dummy var
}

void CAnimatedVariable::create(ANIMATEDVARTYPE type, SAnimationPropertyConfig* pAnimConfig, void* pWindow, AVARDAMAGEPOLICY policy) {
    m_eVarType = type;
    m_eDamagePolicy = policy;
    m_pConfig = pAnimConfig;
    m_pWindow = pWindow;

    g_pAnimationManager->m_lAnimatedVariables.push_back(this);

    m_bDummy = false;
}

void CAnimatedVariable::create(ANIMATEDVARTYPE type, std::any val, SAnimationPropertyConfig* pAnimConfig, void* pWindow, AVARDAMAGEPOLICY policy) {
    create(type, pAnimConfig, pWindow, policy);

    try {
        switch (type) {
            case AVARTYPE_FLOAT: {
                const auto V = std::any_cast<float>(val);
                m_fValue = V;
                m_fGoal = V;
                break;
            }
            case AVARTYPE_VECTOR: {
                const auto V = std::any_cast<Vector2D>(val);
                m_vValue = V;
                m_vGoal = V;
                break;
            }
            case AVARTYPE_COLOR: {
                const auto V = std::any_cast<CColor>(val);
                m_cValue = V;
                m_cGoal = V;
                break;
            }
            default:
                ASSERT(false);
                break;
        }
    } catch (std::exception& e) {
        Debug::log(ERR, "CAnimatedVariable create error: %s", e.what());
        RASSERT(false, "CAnimatedVariable create error: %s", e.what());
    }
}

CAnimatedVariable::~CAnimatedVariable() {
    unregister();
}

void CAnimatedVariable::unregister() {
    g_pAnimationManager->m_lAnimatedVariables.remove(this);
}

int CAnimatedVariable::getDurationLeftMs() {
    return std::max((int)(m_pConfig->pValues->internalSpeed * 100) - (int)std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - animationBegin).count(), 0);
}
