#pragma once

#include "../../TiledAlgorithm.hpp"
#include "../../../../managers/HookSystemManager.hpp"

#include <vector>

namespace Layout::Tiled {

    struct SMonocleTargetData {
        SMonocleTargetData(SP<ITarget> t) : target(t) {
            ;
        }

        WP<ITarget> target;
        CBox        layoutBox;
    };

    class CMonocleAlgorithm : public ITiledAlgorithm {
      public:
        CMonocleAlgorithm();
        virtual ~CMonocleAlgorithm();

        virtual void                             newTarget(SP<ITarget> target);
        virtual void                             movedTarget(SP<ITarget> target, std::optional<Vector2D> focalPoint = std::nullopt);
        virtual void                             removeTarget(SP<ITarget> target);

        virtual void                             resizeTarget(const Vector2D& Δ, SP<ITarget> target, eRectCorner corner = CORNER_NONE);
        virtual void                             recalculate();

        virtual SP<ITarget>                      getNextCandidate(SP<ITarget> old);

        virtual std::expected<void, std::string> layoutMsg(const std::string_view& sv);
        virtual std::optional<Vector2D>          predictSizeForNewTarget();

        virtual void                             swapTargets(SP<ITarget> a, SP<ITarget> b);
        virtual void                             moveTargetInDirection(SP<ITarget> t, Math::eDirection dir, bool silent);

      private:
        std::vector<SP<SMonocleTargetData>> m_targetDatas;
        SP<HOOK_CALLBACK_FN>                m_focusCallback;

        int                                 m_currentVisibleIndex = 0;

        SP<SMonocleTargetData>              dataFor(SP<ITarget> t);
        void                                cycleNext();
        void                                cyclePrev();
        void                                focusTargetUpdate(SP<ITarget> target);
        void                                updateVisible();
        SP<ITarget>                         getVisibleTarget();
    };
};
