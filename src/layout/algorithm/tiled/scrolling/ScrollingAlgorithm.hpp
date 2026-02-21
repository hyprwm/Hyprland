#pragma once

#include "../../TiledAlgorithm.hpp"
#include "../../../../managers/HookSystemManager.hpp"
#include "../../../../helpers/math/Direction.hpp"
#include "ScrollTapeController.hpp"

#include <vector>

namespace Layout::Tiled {
    class CScrollingAlgorithm;
    struct SColumnData;
    struct SScrollingData;

    struct SScrollingTargetData {
        SScrollingTargetData(SP<ITarget> t, SP<SColumnData> col) : target(t), column(col) {
            ;
        }

        WP<ITarget>     target;
        WP<SColumnData> column;
        bool            ignoreFullscreenChecks = false;

        CBox            layoutBox;
    };

    struct SColumnData {
        SColumnData(SP<SScrollingData> data) : scrollingData(data) {
            ;
        }

        void   add(SP<ITarget> t);
        void   add(SP<ITarget> t, int after);
        void   add(SP<SScrollingTargetData> w);
        void   add(SP<SScrollingTargetData> w, int after);
        void   remove(SP<ITarget> t);
        bool   has(SP<ITarget> t);
        size_t idx(SP<ITarget> t);

        // index of lowest target that is above y.
        size_t                                idxForHeight(float y);

        void                                  up(SP<SScrollingTargetData> w);
        void                                  down(SP<SScrollingTargetData> w);

        SP<SScrollingTargetData>              next(SP<SScrollingTargetData> w);
        SP<SScrollingTargetData>              prev(SP<SScrollingTargetData> w);

        std::vector<SP<SScrollingTargetData>> targetDatas;
        WP<SScrollingData>                    scrollingData;
        WP<SScrollingTargetData>              lastFocusedTarget;

        WP<SColumnData>                       self;

        // Helper methods to access controller-managed data
        float getColumnWidth() const;
        void  setColumnWidth(float width);
        float getTargetSize(size_t idx) const;
        void  setTargetSize(size_t idx, float size);
        float getTargetSize(SP<SScrollingTargetData> target) const;
        void  setTargetSize(SP<SScrollingTargetData> target, float size);
    };

    struct SScrollingData {
        SScrollingData(CScrollingAlgorithm* algo);

        std::vector<SP<SColumnData>> columns;

        UP<CScrollTapeController>    controller;

        SP<SColumnData>              add();
        SP<SColumnData>              add(int after);
        int64_t                      idx(SP<SColumnData> c);
        void                         remove(SP<SColumnData> c);
        double                       maxWidth();
        SP<SColumnData>              next(SP<SColumnData> c);
        SP<SColumnData>              prev(SP<SColumnData> c);
        SP<SColumnData>              atCenter();

        bool                         visible(SP<SColumnData> c);
        void                         centerCol(SP<SColumnData> c);
        void                         fitCol(SP<SColumnData> c);
        void                         centerOrFitCol(SP<SColumnData> c);

        void                         recalculate(bool forceInstant = false);

        CScrollingAlgorithm*         algorithm = nullptr;
        WP<SScrollingData>           self;
        std::optional<double>        lockedCameraOffset;
    };

    class CScrollingAlgorithm : public ITiledAlgorithm {
      public:
        CScrollingAlgorithm();
        virtual ~CScrollingAlgorithm();

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

        CBox                                     usableArea();

      private:
        SP<SScrollingData>   m_scrollingData;

        SP<HOOK_CALLBACK_FN> m_configCallback;
        SP<HOOK_CALLBACK_FN> m_focusCallback;
        SP<HOOK_CALLBACK_FN> m_mouseButtonCallback;

        struct {
            std::vector<float> configuredWidths;
        } m_config;

        eScrollDirection         getDynamicDirection();

        SP<SScrollingTargetData> findBestNeighbor(SP<SScrollingTargetData> pCurrent, SP<SColumnData> pTargetCol);
        SP<SScrollingTargetData> dataFor(SP<ITarget> t);
        SP<SScrollingTargetData> closestNode(const Vector2D& posGlobglobgabgalab);

        void                     focusTargetUpdate(SP<ITarget> target);
        void                     moveTargetTo(SP<ITarget> t, Math::eDirection dir, bool silent);
        void                     focusOnInput(SP<ITarget> target, bool hardInput);

        friend struct SScrollingData;
    };
};
