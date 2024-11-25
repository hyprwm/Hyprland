#pragma once

#include "../defines.hpp"
#include <list>
#include <unordered_map>
#include "../helpers/AnimatedVariable.hpp"
#include "../helpers/BezierCurve.hpp"
#include "../helpers/Timer.hpp"
#include "eventLoop/EventLoopTimer.hpp"

class CWindow;

class CAnimationManager {
  public:
    CAnimationManager();

    void                                          tick();
    bool                                          shouldTickForNext();
    void                                          onTicked();
    void                                          scheduleTick();
    void                                          addBezierWithName(std::string, const Vector2D&, const Vector2D&);
    void                                          removeAllBeziers();

    void                                          onWindowPostCreateClose(PHLWINDOW, bool close = false);

    bool                                          bezierExists(const std::string&);
    CBezierCurve*                                 getBezier(const std::string&);

    std::string                                   styleValidInConfigVar(const std::string&, const std::string&);

    std::unordered_map<std::string, CBezierCurve> getAllBeziers();

    std::vector<CBaseAnimatedVariable*>           m_vAnimatedVariables;
    std::vector<CBaseAnimatedVariable*>           m_vActiveAnimatedVariables;

    SP<CEventLoopTimer>                           m_pAnimationTimer;

    float                                         m_fLastTickTime; // in ms

  private:
    bool                                          deltaSmallToFlip(const Vector2D& a, const Vector2D& b);
    bool                                          deltaSmallToFlip(const CColor& a, const CColor& b);
    bool                                          deltaSmallToFlip(const float& a, const float& b);
    bool                                          deltazero(const Vector2D& a, const Vector2D& b);
    bool                                          deltazero(const CColor& a, const CColor& b);
    bool                                          deltazero(const float& a, const float& b);

    std::unordered_map<std::string, CBezierCurve> m_mBezierCurves;

    bool                                          m_bTickScheduled = false;

    // Anim stuff
    void animationPopin(PHLWINDOW, bool close = false, float minPerc = 0.f);
	void animationPopout(PHLWINDOW, bool close = false, float maxPerc = 1.5f);
    void animationSlide(PHLWINDOW, std::string force = "", bool close = false);
};

inline std::unique_ptr<CAnimationManager> g_pAnimationManager;
