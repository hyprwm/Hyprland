#pragma once

#include <hyprutils/animation/AnimationManager.hpp>
#include <hyprutils/animation/AnimatedVariable.hpp>

#include "../defines.hpp"
#include "desktop/DesktopTypes.hpp"
#include "eventLoop/EventLoopTimer.hpp"
#include "../helpers/AnimatedVariable.hpp"

class CHyprAnimationManager : public Hyprutils::Animation::CAnimationManager {
  public:
    CHyprAnimationManager();

    void         tick();
    virtual void scheduleTick();
    virtual void onTicked();

    using SAnimationPropertyConfig = Hyprutils::Animation::SAnimationPropertyConfig;
    template <Animable VarType>
    inline void addAnimation(const VarType& v, CAnimatedVariable<VarType>& av, SAnimationPropertyConfig* pConfig, eAVarDamagePolicy policy) {
        constexpr const eAnimatedVarType EAVTYPE = typeToeAnimatedVarType<VarType>;
        av.create(EAVTYPE, v, static_cast<Hyprutils::Animation::CAnimationManager*>(this));
        av.setConfig(pConfig);
        av.m_Context.eDamagePolicy = policy;
    }

    template <Animable VarType>
    void addAnimation(const VarType& v, CAnimatedVariable<VarType>& av, SAnimationPropertyConfig* pConfig, PHLWINDOW pWindow, eAVarDamagePolicy policy) {
        addAnimation(v, av, pConfig, policy);
        av.m_Context.pWindow = pWindow;
    }
    template <Animable VarType>
    void addAnimation(const VarType& v, CAnimatedVariable<VarType>& av, SAnimationPropertyConfig* pConfig, PHLWORKSPACE pWorkspace, eAVarDamagePolicy policy) {
        addAnimation(v, av, pConfig, policy);
        av.m_Context.pWorkspace = pWorkspace;
    }
    template <Animable VarType>
    void addAnimation(const VarType& v, CAnimatedVariable<VarType>& av, SAnimationPropertyConfig* pConfig, PHLLS pLayer, eAVarDamagePolicy policy) {
        addAnimation(v, av, pConfig, policy);
        av.m_Context.pLayer = pLayer;
    }

    void                damageAnimatedWindow(PHLWINDOW, eAVarDamagePolicy);
    void                damageAnimatedWorkspace(PHLWORKSPACE, eAVarDamagePolicy);
    void                damageAnimatedLayer(PHLLS, eAVarDamagePolicy);

    void                onWindowPostCreateClose(PHLWINDOW, bool close = false);

    std::string         styleValidInConfigVar(const std::string&, const std::string&);

    SP<CEventLoopTimer> m_pAnimationTimer;

    float               m_fLastTickTime; // in ms

  private:
    bool m_bTickScheduled = false;

    // Anim stuff
    void animationPopin(PHLWINDOW, bool close = false, float minPerc = 0.f);
    void animationSlide(PHLWINDOW, std::string force = "", bool close = false);

    bool handleContext(const SAnimationContext&);
};

inline std::unique_ptr<CHyprAnimationManager> g_pAnimationManager;
