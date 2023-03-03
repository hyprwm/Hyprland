#pragma once

#include "../defines.hpp"
#include <list>
#include <unordered_map>
#include "../helpers/AnimatedVariable.hpp"
#include "../helpers/BezierCurve.hpp"
#include "../Window.hpp"
#include "../helpers/Timer.hpp"

class CAnimationManager {
  public:
    CAnimationManager();

    void                                          tick();
    void                                          addBezierWithName(std::string, const Vector2D&, const Vector2D&);
    void                                          removeAllBeziers();

    void                                          onWindowPostCreateClose(CWindow*, bool close = false);

    bool                                          bezierExists(const std::string&);
    CBezierCurve*                                 getBezier(const std::string&);

    std::string                                   styleValidInConfigVar(const std::string&, const std::string&);

    std::unordered_map<std::string, CBezierCurve> getAllBeziers();

    std::list<CAnimatedVariable*>                 m_lAnimatedVariables;

    wl_event_source*                              m_pAnimationTick;

    float                                         m_fLastTickTime; // in ms

  private:
    bool                                          deltaSmallToFlip(const Vector2D& a, const Vector2D& b);
    bool                                          deltaSmallToFlip(const CColor& a, const CColor& b);
    bool                                          deltaSmallToFlip(const float& a, const float& b);
    bool                                          deltazero(const Vector2D& a, const Vector2D& b);
    bool                                          deltazero(const CColor& a, const CColor& b);
    bool                                          deltazero(const float& a, const float& b);

    std::unordered_map<std::string, CBezierCurve> m_mBezierCurves;

    // Anim stuff
    void animationPopin(CWindow*, bool close = false, float minPerc = 0.f);
    void animationSlide(CWindow*, std::string force = "", bool close = false);
};

inline std::unique_ptr<CAnimationManager> g_pAnimationManager;