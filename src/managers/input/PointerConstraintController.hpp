#pragma once

#include "../../helpers/memory/Memory.hpp"
#include "../../helpers/signal/Signal.hpp"
#include "PointerConstraintState.hpp"
#include <hyprutils/utils/ScopeGuard.hpp>
#include <hyprutils/math/Vector2D.hpp>
#include <cstddef>
#include <cstdint>

class CPointerConstraint;
class CWLSurfaceResource;
namespace Desktop::View {
    class CWLSurface;
}

class CPointerConstraintController {
  public:
    void                                        init();
    void                                        release();
    void                                        refresh(bool rearm = false);

    [[nodiscard]] Hyprutils::Utils::CScopeGuard suspend();

    bool                                        apply(uint32_t time, const Hyprutils::Math::Vector2D& position);
    void                                        enforce();
    bool                                        nativeActive() const;
    bool                                        locked() const;
    bool                                        blocksAbsoluteMotion() const;
    bool                                        preservesPointerFocus(SP<CWLSurfaceResource> target) const;

  private:
    SP<Desktop::View::CWLSurface> activeSurface() const;
    void                          resume();

    WP<CPointerConstraint>        m_native;
    WP<Desktop::View::CWLSurface> m_ruleSurface;
    CPointerConstraintState       m_ruleState;
    bool                          m_released      = false;
    size_t                        m_suspensions   = 0;
    bool                          m_hadConstraint = false;
    WP<CWLSurfaceResource>        m_keyboardBefore;
    Hyprutils::Math::Vector2D     m_positionBefore;

    struct {
        CHyprSignalListener pointerFocus;
        CHyprSignalListener keyboardFocus;
        CHyprSignalListener rules;
    } m_listeners;
};
