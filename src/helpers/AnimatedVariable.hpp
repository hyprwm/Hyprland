#pragma once

#include <hyprutils/animation/AnimatedVariable.hpp>

#include "Color.hpp"
#include "../defines.hpp"
#include "../desktop/DesktopTypes.hpp"

enum eAVarDamagePolicy : int8_t {
    AVARDAMAGE_NONE   = -1,
    AVARDAMAGE_ENTIRE = 0,
    AVARDAMAGE_BORDER,
    AVARDAMAGE_SHADOW
};

enum eAnimatedVarType : int8_t {
    AVARTYPE_INVALID = -1,
    AVARTYPE_FLOAT,
    AVARTYPE_VECTOR,
    AVARTYPE_COLOR
};

// Utility to bind a type with its corresponding eAnimatedVarType
template <class T>
// NOLINTNEXTLINE(readability-identifier-naming)
struct STypeToAnimatedVarType_t {
    static constexpr eAnimatedVarType value = AVARTYPE_INVALID;
};

template <>
struct STypeToAnimatedVarType_t<float> {
    static constexpr eAnimatedVarType value = AVARTYPE_FLOAT;
};

template <>
struct STypeToAnimatedVarType_t<Vector2D> {
    static constexpr eAnimatedVarType value = AVARTYPE_VECTOR;
};

template <>
struct STypeToAnimatedVarType_t<CHyprColor> {
    static constexpr eAnimatedVarType value = AVARTYPE_COLOR;
};

template <class T>
inline constexpr eAnimatedVarType typeToeAnimatedVarType = STypeToAnimatedVarType_t<T>::value;

// Utility to define a concept as a list of possible type
template <class T, class... U>
concept OneOf = (... or std::same_as<T, U>);

// Concept to describe which type can be placed into CAnimatedVariable
// This is mainly to get better errors if we put a type that's not supported
// Otherwise template errors are ugly
template <class T>
concept Animable = OneOf<T, Vector2D, float, CHyprColor>;

struct SAnimationContext {
    PHLWINDOWREF      pWindow;
    PHLWORKSPACEREF   pWorkspace;
    PHLLSREF          pLayer;

    eAVarDamagePolicy eDamagePolicy = AVARDAMAGE_NONE;
};

template <Animable VarType>
using CAnimatedVariable = Hyprutils::Animation::CGenericAnimatedVariable<VarType, SAnimationContext>;

/*template <Animable VarType>
class CAnimatedVariable : public Hyprutils::Animation::CBaseAnimatedVariable {
  public:
    CAnimatedVariable() {
        ; // dummy var
    }

    void baseCreate(const Hyprutils::Animation::SAnimationConfig& pAnimConfig) {
        CBaseAnimatedVariable::create(pAnimConfig);
    }

    void create(const VarType& value, const SAnimationPropertyConfig* pAnimConfig, eAVarDamagePolicy policy) {
        const auto CONFIG = getAnimationConfig(pAnimConfig);
        baseCreate(CONFIG);

        m_pConfigProperties = pAnimConfig;
        m_Value             = value;
        m_Goal              = value;
        m_eDamagePolicy     = policy;
    }

    void create(const VarType& value, const SAnimationPropertyConfig* pAnimConfig, PHLWINDOW pWindow, eAVarDamagePolicy policy) {
        create(value, pAnimConfig, policy);
        m_pWindow = pWindow;
    }

    void create(const VarType& value, const SAnimationPropertyConfig* pAnimConfig, PHLLS pLayer, eAVarDamagePolicy policy) {
        create(value, pAnimConfig, policy);
        m_pLayer = pLayer;
    }

    void create(const VarType& value, const SAnimationPropertyConfig* pAnimConfig, PHLWORKSPACE pWorkspace, eAVarDamagePolicy policy) {
        create(value, pAnimConfig, policy);
        m_pWorkspace = pWorkspace;
    }

    //
    virtual void update(const float SPENT) {
        // for disabled anims just warp
        if (!g_pAnimationManager->animationsEnabled() || !m_sConfig.enabled) {
            warp(false);
            return;
        }

        if (SPENT >= 1.f || m_Begun == m_Goal) {
            warp(false);
            return;
        }

        PHLWINDOW    PWINDOW    = m_pWindow.lock();
        PHLWORKSPACE PWORKSPACE = m_pWorkspace.lock();
        PHLLS        PLAYER     = m_pLayer.lock();

        if (m_eDamagePolicy == AVARDAMAGE_NONE)
            return;

        if (PWINDOW)
            g_pAnimationManager->damageAnimatedWindow(PWINDOW, m_eDamagePolicy);
        else if (PWORKSPACE)
            g_pAnimationManager->damageAnimatedWorkspace(PWORKSPACE, m_eDamagePolicy);
        else if (PLAYER)
            g_pAnimationManager->damageAnimatedLayer(PLAYER, m_eDamagePolicy);

        if constexpr (std::same_as<VarType, CHyprColor>)
            updateColorValue(*this, SPENT);
        else
            updateGenericValue(*this, SPENT);
    };

    CAnimatedVariable(const CAnimatedVariable&)            = delete;
    CAnimatedVariable(CAnimatedVariable&&)                 = delete;
    CAnimatedVariable& operator=(const CAnimatedVariable&) = delete;
    CAnimatedVariable& operator=(CAnimatedVariable&&)      = delete;

    virtual ~CAnimatedVariable() {
        disconnectFromActive(); // Important!!! Otherwise the g_pAnimationManager might reference a dangling pointer
    };

    void setConfig(const SAnimationPropertyConfig* pConfig) {
        m_sConfig           = getAnimationConfig(pConfig);
        m_pConfigProperties = pConfig;
    };

    // gets the current vector value (real time)
    const VarType& value() const {
        return m_Value;
    }

    // gets the goal vector value
    const VarType& goal() const {
        return m_Goal;
    }

    CAnimatedVariable& operator=(const VarType& v) {
        if (v == m_Goal)
            return *this;

        m_Goal  = v;
        m_Begun = m_Value;

        CBaseAnimatedVariable::onAnimationBegin();

        return *this;
    }

    // Sets the actual stored value, without affecting the goal, but resets the timer
    void setValue(const VarType& v) {
        if (v == m_Value)
            return;

        m_Value = v;
        m_Begun = m_Value;

        CBaseAnimatedVariable::onAnimationBegin();
    }

    // Sets the actual value and goal
    void setValueAndWarp(const VarType& v) {
        m_Goal             = v;
        m_bIsBeingAnimated = true;
        warp();
    }

    virtual void warp(bool endCallback = true) {
        if (!m_bIsBeingAnimated)
            return;

        m_Value = m_Goal;

        m_bIsBeingAnimated = false;

        if (m_fUpdateCallback)
            m_fUpdateCallback(this);

        if (endCallback)
            onAnimationEnd();
    }

    eAVarDamagePolicy  m_eDamagePolicy = AVARDAMAGE_NONE;

    const std::string& getStyle() const {
        return ::getStyle(m_pConfigProperties);
    };

    PHLWINDOW getWindow() const {
        return m_pWindow.lock();
    }

    VarType m_Value{};
    VarType m_Goal{};
    VarType m_Begun{};

  private:
    const SAnimationPropertyConfig* m_pConfigProperties = nullptr;

    PHLWINDOWREF                    m_pWindow;
    PHLWORKSPACEREF                 m_pWorkspace;
    PHLLSREF                        m_pLayer;

    virtual void                    connectToActive() {
        if (!m_bIsConnectedToActive)
            ::connectToActive(this);
        m_bIsConnectedToActive = true;
    }

    virtual void disconnectFromActive() {
        ::disconnectFromActive(this);
        m_bIsConnectedToActive = false;
    }

    friend class CHyprAnimationManager;
};

template <Animable VarType>
inline void updateGenericValue(CAnimatedVariable<VarType>& av, const float SPENT) {
    const auto PBEZIER = av.getConfig().bezier.lock();
    if (!PBEZIER) {
        Debug::log(WARN, "Bezier curve not found");
        av.warp(false);
        return;
    }

    const auto POINTY = PBEZIER->getYForPoint(SPENT);
    const auto DELTA  = av.m_Goal - av.m_Begun;

    av.m_Value = av.m_Begun + DELTA * POINTY;
}

inline void updateColorValue(CAnimatedVariable<CHyprColor>& av, const float SPENT) {
    const auto PBEZIER = av.getConfig().bezier.lock();
    if (!PBEZIER) {
        Debug::log(WARN, "Bezier curve not found");
        av.warp(false);
        return;
    }

    const auto POINTY = PBEZIER->getYForPoint(SPENT);

    // convert both to OkLab, then lerp that, and convert back.
    // This is not as fast as just lerping rgb, but it's WAY more precise...
    // Use the CHyprColor cache for OkLab

    const auto&                L1 = av.m_Begun.asOkLab();
    const auto&                L2 = av.m_Goal.asOkLab();

    static const auto          lerp = [](const float one, const float two, const float progress) -> float { return one + (two - one) * progress; };

    const Hyprgraphics::CColor lerped = Hyprgraphics::CColor::SOkLab{
        .l = lerp(L1.l, L2.l, POINTY),
        .a = lerp(L1.a, L2.a, POINTY),
        .b = lerp(L1.b, L2.b, POINTY),
    };

    av.m_Value = {lerped, lerp(av.m_Begun.a, av.m_Goal.a, POINTY)};
}*/
