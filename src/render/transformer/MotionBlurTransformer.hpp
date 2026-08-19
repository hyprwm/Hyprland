#pragma once

#include "Transformer.hpp"

namespace Render {
    class CMotionBlurTransformer : public IWindowTransformer {
      public:
        CMotionBlurTransformer(PHLWINDOWREF window);
        virtual ~CMotionBlurTransformer();

        static bool                       shouldEnable(PHLWINDOW window);

        virtual SWindowTransformBuffer    transform(CRenderingContext&, const SWindowTransformBuffer& in, const SWindowTransformContext& context);
        virtual int                       priority() const;
        virtual bool                      active() const;
        virtual bool                      allocatesOutputBuffer() const;
        virtual CBox                      sourceBoxForOutput(const CBox& outputBox, const CBox& inputBox) const;
        virtual CBox                      transformBoxForDamage(const CBox& currentBox) const;
        virtual void                      amendTransformedRenderData(CRenderingContext&, const CBox& currentBox, SMotionBlurData* pMotionBlurData);

        void                              record(const CBox& previous, const CBox& current);
        void                              reset();

        std::optional<MotionBlur::SState> state(bool allowStale = false) const;

      private:
        std::optional<MotionBlur::SState> state(const CRenderingContext&, bool allowStale) const;
        std::optional<MotionBlur::SState> state(const Vector2D& renderOffset, bool allowStale) const;
        void                              armExpiryTimer();
        void                              disarmExpiryTimer();

        PHLWINDOWREF                      m_window;
        MotionBlur::CTracker              m_motionBlur;
        SP<CEventLoopTimer>               m_expiryTimer;
    };
}
