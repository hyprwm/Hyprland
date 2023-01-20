#pragma once

#include "../defines.hpp"
#include <any>

enum ANIMATEDVARTYPE {
    AVARTYPE_INVALID = -1,
    AVARTYPE_FLOAT,
    AVARTYPE_VECTOR,
    AVARTYPE_COLOR
};

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

class CAnimatedVariable {
  public:
    CAnimatedVariable(); // dummy var

    void create(ANIMATEDVARTYPE, SAnimationPropertyConfig*, void* pWindow, AVARDAMAGEPOLICY);
    void create(ANIMATEDVARTYPE, std::any val, SAnimationPropertyConfig*, void* pWindow, AVARDAMAGEPOLICY);

    ~CAnimatedVariable();

    void unregister();
    void registerVar();

    // gets the current vector value (real time)
    const Vector2D& vec() const {
        return m_vValue;
    }

    // gets the current float value (real time)
    const float& fl() const {
        return m_fValue;
    }

    // gets the current color value (real time)
    const CColor& col() const {
        return m_cValue;
    }

    // gets the goal vector value
    const Vector2D& goalv() const {
        return m_vGoal;
    }

    // gets the goal float value
    const float& goalf() const {
        return m_fGoal;
    }

    // gets the goal color value
    const CColor& goalc() const {
        return m_cGoal;
    }

    CAnimatedVariable& operator=(const Vector2D& v) {
        m_vGoal        = v;
        animationBegin = std::chrono::system_clock::now();
        m_vBegun       = m_vValue;

        onAnimationBegin();

        return *this;
    }

    CAnimatedVariable& operator=(const float& v) {
        m_fGoal        = v;
        animationBegin = std::chrono::system_clock::now();
        m_fBegun       = m_fValue;

        onAnimationBegin();

        return *this;
    }

    CAnimatedVariable& operator=(const CColor& v) {
        m_cGoal        = v;
        animationBegin = std::chrono::system_clock::now();
        m_cBegun       = m_cValue;

        onAnimationBegin();

        return *this;
    }

    // Sets the actual stored value, without affecting the goal, but resets the timer
    void setValue(const Vector2D& v) {
        m_vValue       = v;
        animationBegin = std::chrono::system_clock::now();
        m_vBegun       = m_vValue;

        onAnimationBegin();
    }

    // Sets the actual stored value, without affecting the goal, but resets the timer
    void setValue(const float& v) {
        m_fValue       = v;
        animationBegin = std::chrono::system_clock::now();
        m_vBegun       = m_vValue;

        onAnimationBegin();
    }

    // Sets the actual stored value, without affecting the goal, but resets the timer
    void setValue(const CColor& v) {
        m_cValue       = v;
        animationBegin = std::chrono::system_clock::now();
        m_vBegun       = m_vValue;

        onAnimationBegin();
    }

    // Sets the actual value and goal
    void setValueAndWarp(const Vector2D& v) {
        m_vGoal = v;
        warp();
    }

    // Sets the actual value and goal
    void setValueAndWarp(const float& v) {
        m_fGoal = v;
        warp();
    }

    // Sets the actual value and goal
    void setValueAndWarp(const CColor& v) {
        m_cGoal = v;
        warp();
    }

    // checks if an animation is in progress
    bool isBeingAnimated() {
        switch (m_eVarType) {
            case AVARTYPE_FLOAT: return m_fValue != m_fGoal;
            case AVARTYPE_VECTOR: return m_vValue != m_vGoal;
            case AVARTYPE_COLOR: return m_cValue != m_cGoal;
            default: UNREACHABLE();
        }

        UNREACHABLE();

        return false; // just so that the warning is suppressed
    }

    void warp(bool endCallback = true) {
        switch (m_eVarType) {
            case AVARTYPE_FLOAT: {
                m_fValue = m_fGoal;
                break;
            }
            case AVARTYPE_VECTOR: {
                m_vValue = m_vGoal;
                break;
            }
            case AVARTYPE_COLOR: {
                m_cValue = m_cGoal;
                break;
            }
            default: UNREACHABLE();
        }

        if (endCallback)
            onAnimationEnd();
    }

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

    /*  resets all callbacks. Does not call any. */
    void resetAllCallbacks() {
        m_fBeginCallback       = nullptr;
        m_fEndCallback         = nullptr;
        m_bRemoveBeginAfterRan = false;
        m_bRemoveEndAfterRan   = false;
    }

  private:
    Vector2D m_vValue = Vector2D(0, 0);
    float    m_fValue = 0;
    CColor   m_cValue;

    Vector2D m_vGoal = Vector2D(0, 0);
    float    m_fGoal = 0;
    CColor   m_cGoal;

    Vector2D m_vBegun = Vector2D(0, 0);
    float    m_fBegun = 0;
    CColor   m_cBegun;

    // owners
    void*                                 m_pWindow    = nullptr;
    void*                                 m_pWorkspace = nullptr;
    void*                                 m_pLayer     = nullptr;

    SAnimationPropertyConfig*             m_pConfig = nullptr;

    bool                                  m_bDummy        = true;
    bool                                  m_bIsRegistered = false;

    std::chrono::system_clock::time_point animationBegin;

    ANIMATEDVARTYPE                       m_eVarType      = AVARTYPE_INVALID;
    AVARDAMAGEPOLICY                      m_eDamagePolicy = AVARDAMAGE_NONE;

    bool                                  m_bRemoveEndAfterRan   = true;
    bool                                  m_bRemoveBeginAfterRan = true;
    std::function<void(void* thisptr)>    m_fEndCallback;
    std::function<void(void* thisptr)>    m_fBeginCallback;

    // methods
    void onAnimationEnd() {
        if (m_fEndCallback) {
            m_fEndCallback(this);
            if (m_bRemoveEndAfterRan)
                m_fEndCallback = nullptr; // reset
        }
    }

    void onAnimationBegin() {
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
