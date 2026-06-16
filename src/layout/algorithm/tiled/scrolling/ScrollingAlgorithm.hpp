#pragma once

#include "../../TiledAlgorithm.hpp"
#include "../../../../helpers/math/Direction.hpp"
#include "ScrollTapeController.hpp"
#include "ScrollingSpanLayout.hpp"
#include "../../../../helpers/signal/Signal.hpp"

#include <optional>
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

        SSpanState      span;
        CBox            layoutBox;
    };

    struct SSpanAdjustedTargetBox {
        CBox   box;
        size_t virtualIndex = 0;
        size_t virtualCount = 0;
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

        bool                                  up(SP<SScrollingTargetData> w);
        bool                                  down(SP<SScrollingTargetData> w);

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

        SP<SColumnData>              add(std::optional<float> width = std::nullopt);
        SP<SColumnData>              add(int after, std::optional<float> width = std::nullopt);
        int64_t                      idx(SP<SColumnData> c);
        void                         remove(SP<SColumnData> c);
        double                       maxWidth();
        SP<SColumnData>              next(SP<SColumnData> c);
        SP<SColumnData>              prev(SP<SColumnData> c);
        SP<SColumnData>              atCenter();

        bool                         visible(SP<SColumnData> c, bool full = false);
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

        virtual void                     newTarget(SP<ITarget> target);
        virtual void                     movedTarget(SP<ITarget> target, std::optional<Vector2D> focalPoint = std::nullopt);
        virtual void                     removeTarget(SP<ITarget> target);

        virtual void                     resizeTarget(const Vector2D& Δ, SP<ITarget> target, eRectCorner corner = CORNER_NONE);
        virtual void                     recalculate(eRecalculateReason reason = RECALCULATE_REASON_UNKNOWN);

        virtual SP<ITarget>              getNextCandidate(SP<ITarget> old);

        virtual Config::ErrorResult      layoutMsg(const std::string_view& sv);
        virtual std::optional<Vector2D>  predictSizeForNewTarget();

        virtual void                     swapTargets(SP<ITarget> a, SP<ITarget> b);
        virtual void                     moveTargetInDirection(SP<ITarget> t, Math::eDirection dir, bool silent);

        virtual eFullscreenRequestResult requestFullscreen(const SFullscreenRequest& request);
        virtual SP<ITarget>              layoutFullscreenTarget() const;
        virtual bool                     layoutFullscreenCoversMonitor() const;

        void                             moveTape(float delta);
        void                             moveTapeNormalized(double delta);
        void                             snapToGrid();
        SP<SColumnData>                  snapToProjectedOffset(double projectedNormalizedOffset);
        void                             focusColumn(SP<SColumnData> column);
        SP<SColumnData>                  getColumnAtViewportCenter();
        SP<SColumnData>                  currentColumn();

        double                           primaryViewportSize();
        double                           normalizedTapeOffset();

        CBox                             usableArea() const;
        SP<SScrollingTargetData>         dataFor(SP<ITarget> t) const;

        void                             inhibitScroll();
        void                             uninhibitScroll();

        enum eInputMode : uint8_t {
            INPUT_MODE_SOFT = 0,
            INPUT_MODE_CLICK,
            INPUT_MODE_HARD
        };

      private:
        SP<SScrollingData>  m_scrollingData;

        CHyprSignalListener m_configCallback;
        CHyprSignalListener m_focusCallback;
        CHyprSignalListener m_mouseButtonCallback;

        struct {
            std::vector<float> configuredWidths;
        } m_config;

        eScrollDirection getDynamicDirection();

        struct SFullscreenScrollState {
            WP<ITarget>          target;
            std::optional<float> restoreColumnWidth;
        };

        void                      syncFullscreenTargets();
        SFullscreenScrollState*   fullscreenStateForTarget(SP<ITarget> target, eFullscreenMode targetFullscreenMode);
        SFullscreenScrollState*   fullscreenStateForData(SP<SScrollingTargetData> target, eFullscreenMode targetFullscreenMode);
        SP<SScrollingTargetData>  fullscreenTargetDataForColumn(SP<SColumnData> col) const;
        bool                      isFullscreenTarget(SP<SScrollingTargetData> target) const;
        float                     fullscreenColumnWidth() const;
        bool                      fullscreenColumnCoversMonitor(SP<SColumnData> col) const;
        void                      updateFullscreenFade(bool coversMonitor);
        void                      clearFullscreenTarget(std::vector<SFullscreenScrollState>& fullscreenTargetList, SP<ITarget> target = nullptr);

        SP<SScrollingTargetData>  findBestNeighbor(SP<SScrollingTargetData> pCurrent, SP<SColumnData> pTargetCol);
        SP<SScrollingTargetData>  closestNode(const Vector2D& posGlobglobgabgalab);

        void                      focusTargetUpdate(SP<ITarget> target);
        std::optional<SSpanRange> spanRangeForTargetData(SP<SScrollingTargetData> target) const;
        bool                      targetDataVisible(SP<SScrollingTargetData> target, bool full = false) const;
        void                      centerSpanRange(const SSpanRange& range);
        void                      fitSpanRange(const SSpanRange& range);
        void                      centerTargetData(SP<SScrollingTargetData> target);
        void                      fitTargetData(SP<SScrollingTargetData> target);
        void                      centerOrFitTargetData(SP<SScrollingTargetData> target);
        SP<SColumnData>           nextColumnForTargetData(SP<SScrollingTargetData> target);
        SP<SColumnData>           prevColumnForTargetData(SP<SScrollingTargetData> target);
        bool                      tryUpdateSpan(SP<SScrollingTargetData> target, int deltaLeft, int deltaRight);
        void                      clearSpan(SP<SScrollingTargetData> target);
        void                      clearAllSpans();
        void                      clearSpansCoveringColumn(SP<SColumnData> column);
        void                      adjustSpansAfterColumnInsert(size_t insertedColumn, size_t columnCountBefore, SP<SScrollingTargetData> ignoredTarget = nullptr);
        void                      adjustSpansAfterColumnRemove(size_t removedColumn, size_t columnCountBefore, SP<SScrollingTargetData> ignoredTarget = nullptr);
        void                      sanitizeCurrentSpans();
        bool                      validateCurrentSpans() const;

        struct SSpanResolvedLayout {
            bool                                             valid = true;
            std::vector<std::vector<SSpanAdjustedTargetBox>> boxes;
        };
        SSpanResolvedLayout                              resolveSpanLayout(const CBox& usable, const Vector2D& workspaceOffset) const;
        std::vector<std::vector<SSpanAdjustedTargetBox>> spanAdjustedColumnBoxes(const CBox& usable, const Vector2D& workspaceOffset) const;
        std::optional<CBox>                 spannedLayoutBox(SP<SScrollingTargetData> target, size_t anchorIdx, const CBox& usable, const Vector2D& workspaceOffset) const;
        void                                moveTargetTo(SP<ITarget> t, Math::eDirection dir, bool silent);
        void                                focusOnInput(SP<ITarget> target, eInputMode input);

        SP<SColumnData>                     expelTarget(SP<SScrollingTargetData> tdata, SP<SColumnData> srcCol, std::optional<int64_t> insertIdx);

        float                               defaultColumnWidth();

        std::vector<SFullscreenScrollState> m_fullscreenTargets;
        std::vector<SFullscreenScrollState> m_maximizeTargets;
        bool                                m_lastFullscreenCover = false;

        friend struct SScrollingData;
    };
};
