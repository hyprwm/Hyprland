#pragma once

#include <functional>
#include <any>
#include <chrono>
#include "Vector2D.hpp"
#include "Color.hpp"
#include "../macros.hpp"

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

    CAnimatedVariable(const CAnimatedVariable&)            = delete;
    CAnimatedVariable(CAnimatedVariable&&)                 = delete;
    CAnimatedVariable& operator=(const CAnimatedVariable&) = delete;
    CAnimatedVariable& operator=(CAnimatedVariable&&)      = delete;

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
    inline bool isBeingAnimated() {
        return m_bIsBeingAnimated;
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

        m_bIsBeingAnimated = false;

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

    bool                                  m_bDummy           = true;
    bool                                  m_bIsRegistered    = false;
    bool                                  m_bIsBeingAnimated = false;

    std::chrono::system_clock::time_point animationBegin;

    ANIMATEDVARTYPE                       m_eVarType      = AVARTYPE_INVALID;
    AVARDAMAGEPOLICY                      m_eDamagePolicy = AVARDAMAGE_NONE;

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
