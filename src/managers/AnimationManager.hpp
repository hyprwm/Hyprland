#pragma once

#include <hyprutils/animation/AnimationManager.hpp>
#include <hyprutils/animation/AnimatedVariable.hpp>

#include "../defines.hpp"
#include "../helpers/AnimatedVariable.hpp"
#include "../desktop/DesktopTypes.hpp"
#include "eventLoop/EventLoopTimer.hpp"

class CHyprAnimationManager : public Hyprutils::Animation::CAnimationManager {
  public:
    CHyprAnimationManager();

    void         tick();
    virtual void scheduleTick();
    virtual void onTicked();

    using SAnimationPropertyConfig = Hyprutils::Animation::SAnimationPropertyConfig;
    template <Animable VarType>
    void createAnimation(const VarType& v, PHLANIMVAR<VarType>& pav, SP<SAnimationPropertyConfig> pConfig, eAVarDamagePolicy policy) {
        constexpr const eAnimatedVarType EAVTYPE = typeToeAnimatedVarType<VarType>;
        const auto                       PAV     = makeShared<CAnimatedVariable<VarType>>();

        PAV->create(EAVTYPE, static_cast<Hyprutils::Animation::CAnimationManager*>(this), PAV, v);
        PAV->setConfig(pConfig);
        PAV->m_Context.eDamagePolicy = policy;

        pav = std::move(PAV);
    }

    template <Animable VarType>
    void createAnimation(const VarType& v, PHLANIMVAR<VarType>& pav, SP<SAnimationPropertyConfig> pConfig, PHLWINDOW pWindow, eAVarDamagePolicy policy) {
        createAnimation(v, pav, pConfig, policy);
        pav->m_Context.pWindow = pWindow;
    }
    template <Animable VarType>
    void createAnimation(const VarType& v, PHLANIMVAR<VarType>& pav, SP<SAnimationPropertyConfig> pConfig, PHLWORKSPACE pWorkspace, eAVarDamagePolicy policy) {
        createAnimation(v, pav, pConfig, policy);
        pav->m_Context.pWorkspace = pWorkspace;
    }
    template <Animable VarType>
    void createAnimation(const VarType& v, PHLANIMVAR<VarType>& pav, SP<SAnimationPropertyConfig> pConfig, PHLLS pLayer, eAVarDamagePolicy policy) {
        createAnimation(v, pav, pConfig, policy);
        pav->m_Context.pLayer = pLayer;
    }

    void                onWindowPostCreateClose(PHLWINDOW, bool close = false);

    std::string         styleValidInConfigVar(const std::string&, const std::string&);

    SP<CEventLoopTimer> m_animationTimer;

    float               m_lastTickTimeMs;

  private:
    bool m_tickScheduled = false;

    // Anim stuff
    void animationPopin(PHLWINDOW, bool close = false, float minPerc = 0.f);
    void animationSlide(PHLWINDOW, std::string force = "", bool close = false);
    void animationGnomed(PHLWINDOW, bool close = false);
    void animationWobbly(PHLWINDOW pWindow, bool close, float intensity = 1.0f);
};

inline UP<CHyprAnimationManager> g_pAnimationManager;
