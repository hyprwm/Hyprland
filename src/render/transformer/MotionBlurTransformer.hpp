#pragma once

#include "Transformer.hpp"

namespace Render {
    class CMotionBlurTransformer : public IWindowTransformer {
      public:
        CMotionBlurTransformer(PHLWINDOWREF window);
        virtual ~CMotionBlurTransformer();

        static bool                       shouldEnable(PHLWINDOW window);

        virtual SP<Render::IFramebuffer>  transform(SP<Render::IFramebuffer> in);
        virtual void                      amendTransformedRenderData(const CBox& currentBox, SMotionBlurData* pMotionBlurData);

        void                              record(const CBox& previous, const CBox& current);
        void                              reset();

        std::optional<MotionBlur::SState> state(bool allowStale = false) const;

      private:
        void                 armExpiryTimer();
        void                 disarmExpiryTimer();

        PHLWINDOWREF         m_window;
        MotionBlur::CTracker m_motionBlur;
        SP<CEventLoopTimer>  m_expiryTimer;
    };
}