#include "AnimatedVariable.hpp"
#include "../managers/AnimationManager.hpp"

CAnimatedVariable::CAnimatedVariable() {
    ; // dummy var
}

void CAnimatedVariable::create(ANIMATEDVARTYPE type, float* speed, int64_t* enabled, std::string* pBezier, void* pWindow, AVARDAMAGEPOLICY policy) {
    m_eVarType = type;
    m_eDamagePolicy = policy;
    m_pSpeed = speed;
    m_pEnabled = enabled;
    m_pWindow = pWindow;
    m_pBezier = pBezier;

    g_pAnimationManager->m_lAnimatedVariables.push_back(this);

    m_bDummy = false;
}

void CAnimatedVariable::create(ANIMATEDVARTYPE type, std::any val, float* speed, int64_t* enabled, std::string* pBezier, void* pWindow, AVARDAMAGEPOLICY policy) {
    create(type, speed, enabled, pBezier, pWindow, policy);

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