#pragma once

#include <any>
#include <span>
#include <vector>

#include "../../../helpers/AnimatedVariable.hpp"
#include "../../../render/decorations/IHyprWindowDecoration.hpp"
#include "../../types/MultiAnimatedVariable.hpp"
#include "../animationControllers/WindowAnimationController.hpp"
#include "FloatingOffset.hpp"

class CHyprBorderDecoration;
class CHyprDropShadowDecoration;
class CHyprInnerGlowDecoration;

namespace Workspace {
    class CWorkspacePresentable;
}

namespace Render {
    struct SWindowRenderPresentation;
    struct SWindowPresentationState;
    enum class eSceneMode : uint8_t;
}

namespace Desktop::View {
    class CWindow;
    enum eWindowAlpha : uint8_t;

    class CWindowPresentation {
      public:
        CWindowPresentation(CWindow& window);
        ~CWindowPresentation();

        CWindowPresentation(const CWindowPresentation&)                                         = delete;
        CWindowPresentation(CWindowPresentation&&)                                              = delete;
        CWindowPresentation&                              operator=(const CWindowPresentation&) = delete;
        CWindowPresentation&                              operator=(CWindowPresentation&&)      = delete;

        void                                              initialize();

        std::span<const SP<IHyprWindowDecoration>>        decorations() const;
        bool                                              containsDecoration(const IHyprWindowDecoration* decoration) const;
        SP<IHyprWindowDecoration>                         decoration(eDecorationType type) const;
        void                                              addDecoration(SP<IHyprWindowDecoration> decoration);
        void                                              removeDecoration(IHyprWindowDecoration* decoration);
        void                                              updateDecorations();
        void                                              uncacheDecorations();
        bool                                              checkInputOnDecorations(eInputType type, const Vector2D& coordinates, std::any data = {});

        Types::CMultiAVarContainer<float, uint8_t>&       alpha();
        const Types::CMultiAVarContainer<float, uint8_t>& alpha() const;
        PHLANIMVAR<float>&                                alpha(eWindowAlpha type);
        const PHLANIMVAR<float>&                          alpha(eWindowAlpha type) const;
        float                                             alphaValue(eWindowAlpha type) const;
        float                                             alphaGoal(eWindowAlpha type) const;
        float                                             alphaTotal() const;
        float                                             alphaTotalGoal() const;

        int                                               borderSize() const;
        void                                              invalidateBorderSize();
        bool                                              opaque() const;
        bool                                              opaque(const Render::SWindowRenderPresentation& presentation) const;
        Render::SWindowRenderPresentation                 renderPresentation() const;
        Render::SWindowRenderPresentation                 renderPresentation(Render::eSceneMode mode) const;
        Render::SWindowRenderPresentation                 renderPresentation(const SP<Workspace::CWorkspacePresentable>& presentation) const;
        float                                             rounding();
        float                                             roundingPower();
        bool                                              isInCurvedCorner(double x, double y);
        bool                                              visibleOnMonitor(PHLMONITOR monitor);

        float                                             dimPercent() const;
        void                                              setDimPercent(float percent);
        void                                              warpDimPercent(float percent);
        float                                             notRespondingTint() const;
        void                                              setNotResponding(bool notResponding);

        const Vector2D&                                   floatingOffset() const;
        eFloatingOffsetSource                             floatingOffsetSource() const;
        void                                              setFloatingOffset(const Vector2D& offset);
        void                                              clearFloatingOffset();
        bool                                              movingFromMonitor() const;
        void                                              setMonitorMovedFrom(int monitor);
        void                                              resetMonitorMovedFrom();
        bool                                              animatingIn() const;
        void                                              setAnimatingIn(bool animating);

        void                                              prepareMap();
        void                                              dispatchMap();
        void                                              setAnimationsToMove();
        void                                              onWorkspaceAnimUpdate();
        void                                              onFocusAnimUpdate();
        void                                              refreshValues();

        Animation::SViewAnimationContext                  animateOut() const;
        void                                              applyAnimateIn() const;

      private:
        Render::SWindowPresentationState           renderPresentationState(const SP<Workspace::CWorkspacePresentable>& presentation) const;
        void                                       addDecorationInternal(const SP<IHyprWindowDecoration>& decoration);

        CWindow&                                   m_window;
        std::vector<SP<IHyprWindowDecoration>>     m_windowDecorations;
        std::vector<IHyprWindowDecoration*>        m_decosToRemove;
        SP<CHyprDropShadowDecoration>              m_shadowDecoration;
        SP<CHyprBorderDecoration>                  m_borderDecoration;
        SP<CHyprInnerGlowDecoration>               m_glowDecoration;
        Types::CMultiAVarContainer<float, uint8_t> m_alpha;
        PHLANIMVAR<float>                          m_dimPercent;
        PHLANIMVAR<float>                          m_notRespondingTint;
        CFloatingOffset                            m_floatingOffset;
        int                                        m_monitorMovedFrom = -1;
        bool                                       m_animatingIn      = false;
        CWindowAnimationController                 m_animationController;
    };
}
