#pragma once

#include <functional>
#include <any>
#include <chrono>
#include <type_traits>
#include "Vector2D.hpp"
#include "Color.hpp"
#include "../macros.hpp"
#include "../debug/Log.hpp"

enum ANIMATEDVARTYPE {
    AVARTYPE_INVALID = -1,
    AVARTYPE_FLOAT,
    AVARTYPE_VECTOR,
    AVARTYPE_COLOR
};

// Utility to bind a type with its corresponding ANIMATEDVARTYPE
template <class T>
struct typeToANIMATEDVARTYPE_t {
    static constexpr ANIMATEDVARTYPE value = AVARTYPE_INVALID;
};

template <>
struct typeToANIMATEDVARTYPE_t<float> {
    static constexpr ANIMATEDVARTYPE value = AVARTYPE_FLOAT;
};

template <>
struct typeToANIMATEDVARTYPE_t<Vector2D> {
    static constexpr ANIMATEDVARTYPE value = AVARTYPE_VECTOR;
};

template <>
struct typeToANIMATEDVARTYPE_t<CColor> {
    static constexpr ANIMATEDVARTYPE value = AVARTYPE_COLOR;
};

template <class T>
inline constexpr ANIMATEDVARTYPE typeToANIMATEDVARTYPE = typeToANIMATEDVARTYPE_t<T>::value;

enum AVARDAMAGEPOLICY {
    AVARDAMAGE_NONE   = -1,
    AVARDAMAGE_ENTIRE = 0,
    AVARDAMAGE_BORDER,
    AVARDAMAGE_SHADOW
};

class CAnimationManager;
class CWorkspace;
struct SLayerSurface;
struct SAnimationPropertyConfig;
class CHyprRenderer;

// Utility to define a concept as a list of possible type
template <class T, class... U>
concept OneOf = (... or std::same_as<T, U>);

// Concept to describe which type can be placed into CAnimatedVariable
// This is mainly to get better errors if we put a type that's not supported
// Otherwise template errors are ugly
template <class T>
concept Animable = OneOf<T, Vector2D, float, CColor>;

class CBaseAnimatedVariable {
  public:
    CBaseAnimatedVariable(ANIMATEDVARTYPE type);
    void create(SAnimationPropertyConfig* pAnimConfig, void* pWindow, AVARDAMAGEPOLICY policy);

    CBaseAnimatedVariable(const CBaseAnimatedVariable&)            = delete;
    CBaseAnimatedVariable(CBaseAnimatedVariable&&)                 = delete;
    CBaseAnimatedVariable& operator=(const CBaseAnimatedVariable&) = delete;
    CBaseAnimatedVariable& operator=(CBaseAnimatedVariable&&)      = delete;

    virtual ~CBaseAnimatedVariable();

    void         unregister();
    void         registerVar();

    virtual void warp(bool endCallback = true) = 0;

    //
    void setConfig(SAnimationPropertyConfig* pConfig) {
        m_pConfig = pConfig;
    }

    SAnimationPropertyConfig* getConfig() {
        return m_pConfig;
    }

    int getDurationLeftMs();

    /* returns the spent (completion) % */
    float getPercent();

    /* returns the current curve value */
    float getCurveValue();

    // checks if an animation is in progress
    inline bool isBeingAnimated() {
        return m_bIsBeingAnimated;
    }

    /*  sets a function to be ran when the animation finishes.
        if an animation is not running, runs instantly.
        if "remove" is set to true, will remove the callback when ran. */
    void setCallbackOnEnd(std::function<void(void* thisptr)> func, bool remove = true) {
        m_fEndCallback       = func;
        m_bRemoveEndAfterRan = remove;

        if (!isBeingAnimated())
            onAnimationEnd();
    }

    /*  sets a function to be ran when an animation is started.
        if "remove" is set to true, will remove the callback when ran. */
    void setCallbackOnBegin(std::function<void(void* thisptr)> func, bool remove = true) {
        m_fBeginCallback       = func;
        m_bRemoveBeginAfterRan = remove;
    }

    /*  Sets the update callback, called every time the value is animated and a step is done
        Warning: calling unregisterVar/registerVar in this handler will cause UB */
    void setUpdateCallback(std::function<void(void* thisptr)> func) {
        m_fUpdateCallback = func;
    }

    /*  resets all callbacks. Does not call any. */
    void resetAllCallbacks() {
        m_fBeginCallback       = nullptr;
        m_fEndCallback         = nullptr;
        m_fUpdateCallback      = nullptr;
        m_bRemoveBeginAfterRan = false;
        m_bRemoveEndAfterRan   = false;
    }

  protected:
    void*                                 m_pWindow    = nullptr;
    void*                                 m_pWorkspace = nullptr;
    void*                                 m_pLayer     = nullptr;

    SAnimationPropertyConfig*             m_pConfig = nullptr;

    bool                                  m_bDummy           = true;
    bool                                  m_bIsRegistered    = false;
    bool                                  m_bIsBeingAnimated = false;

    std::chrono::system_clock::time_point animationBegin;

    AVARDAMAGEPOLICY                      m_eDamagePolicy = AVARDAMAGE_NONE;
    ANIMATEDVARTYPE                       m_Type;

    bool                                  m_bRemoveEndAfterRan   = true;
    bool                                  m_bRemoveBeginAfterRan = true;
    std::function<void(void* thisptr)>    m_fEndCallback;
    std::function<void(void* thisptr)>    m_fBeginCallback;
    std::function<void(void* thisptr)>    m_fUpdateCallback;

    bool                                  m_bIsConnectedToActive = false;

    void                                  connectToActive();

    void                                  disconnectFromActive();

    // methods
    void onAnimationEnd() {
        m_bIsBeingAnimated = false;
        disconnectFromActive();

        if (m_fEndCallback) {
            // loading m_bRemoveEndAfterRan before calling the callback allows the callback to delete this animation safely if it is false.
            auto removeEndCallback = m_bRemoveEndAfterRan;
            m_fEndCallback(this);
            if (removeEndCallback)
                m_fEndCallback = nullptr; // reset
        }
    }

    void onAnimationBegin() {
        m_bIsBeingAnimated = true;
        connectToActive();

        if (m_fBeginCallback) {
            m_fBeginCallback(this);
            if (m_bRemoveBeginAfterRan)
                m_fBeginCallback = nullptr; // reset
        }
    }

    friend class CAnimationManager;
    friend class CWorkspace;
    friend struct SLayerSurface;
    friend class CHyprRenderer;
};

template <Animable VarType>
class CAnimatedVariable : public CBaseAnimatedVariable {
  public:
    CAnimatedVariable() : CBaseAnimatedVariable(typeToANIMATEDVARTYPE<VarType>) {} // dummy var

    void create(const VarType& value, SAnimationPropertyConfig* pAnimConfig, void* pWindow, AVARDAMAGEPOLICY policy) {
        create(pAnimConfig, pWindow, policy);
        m_Value = value;
    }

    using CBaseAnimatedVariable::create;

    CAnimatedVariable(const CAnimatedVariable&)            = delete;
    CAnimatedVariable(CAnimatedVariable&&)                 = delete;
    CAnimatedVariable& operator=(const CAnimatedVariable&) = delete;
    CAnimatedVariable& operator=(CAnimatedVariable&&)      = delete;

    ~CAnimatedVariable() = default;

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

        m_Goal         = v;
        animationBegin = std::chrono::system_clock::now();
        m_Begun        = m_Value;

        onAnimationBegin();

        return *this;
    }

    // Sets the actual stored value, without affecting the goal, but resets the timer
    void setValue(const VarType& v) {
        if (v == m_Value)
            return;

        m_Value        = v;
        animationBegin = std::chrono::system_clock::now();
        m_Begun        = m_Value;

        onAnimationBegin();
    }

    // Sets the actual value and goal
    void setValueAndWarp(const VarType& v) {
        m_Goal = v;
        warp();
    }

    void warp(bool endCallback = true) override {
        m_Value = m_Goal;

        m_bIsBeingAnimated = false;

        if (endCallback)
            onAnimationEnd();
    }

  private:
    VarType m_Value{};
    VarType m_Goal{};
    VarType m_Begun{};

    // owners

    friend class CAnimationManager;
    friend class CWorkspace;
    friend struct SLayerSurface;
    friend class CHyprRenderer;
};
