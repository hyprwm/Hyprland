#pragma once

#include "../../TiledAlgorithm.hpp"
#include "../../../../layout/algorithm/Algorithm.hpp"
#include "ScrollTapeController.hpp"

#include <lua.h>
#include "../../../../config/lua/bindings/LuaBindingsInternal.hpp"


namespace Fullscreen::ScrollingFullscreenHandler {
    class CScrollingFullscreenHandler;
}

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

        virtual void                               newTarget(SP<ITarget> target);
        virtual void                               movedTarget(SP<ITarget> target, std::optional<Vector2D> focalPoint = std::nullopt);
        virtual void                               removeTarget(SP<ITarget> target);

        virtual void                               resizeTarget(const Vector2D& Δ, SP<ITarget> target, eRectCorner corner = CORNER_NONE);
        virtual void                               recalculate(eRecalculateReason reason = RECALCULATE_REASON_UNKNOWN);

        virtual SP<ITarget>                        getNextCandidate(SP<ITarget> old);

        virtual Config::ErrorResult                layoutMsg(const std::string_view& sv);
        virtual std::optional<Vector2D>            predictSizeForNewTarget();

        virtual void                               swapTargets(SP<ITarget> a, SP<ITarget> b);
        virtual void                               moveTargetInDirection(SP<ITarget> t, Math::eDirection dir, bool silent);

        virtual WP<Fullscreen::IFullscreenHandler> getFSHandler();

        void                                       moveTape(float delta);
        void                                       moveTapeNormalized(double delta);
        void                                       snapToGrid();
        SP<SColumnData>                            snapToProjectedOffset(double projectedNormalizedOffset);
        void                                       focusColumn(SP<SColumnData> column);
        SP<SColumnData>                            getColumnAtViewportCenter();
        SP<SColumnData>                            currentColumn();

        double                                     primaryViewportSize();
        double                                     normalizedTapeOffset();

        CBox                                       usableArea() const;
        SP<SScrollingTargetData>                   dataFor(SP<ITarget> t, bool stepIntoGroups = false) const;

        void                                       inhibitScroll();
        void                                       uninhibitScroll();

        enum eInputMode : uint8_t {
            INPUT_MODE_SOFT = 0,
            INPUT_MODE_CLICK,
            INPUT_MODE_HARD
        };




        // lua config helper functions

        static int hlScrollingGetOffset(lua_State* L) {
            const auto   WORKSPACE = L ? Config::Lua::Bindings::Internal::workspaceFromLuaSelectorOrObject(L, 1, "hl.get_scrolling_offset") : Desktop::focusState()->monitor()->getCurrentWorkspace();
            if (!WORKSPACE || !WORKSPACE->space() || !WORKSPACE->space()->algorithm()) {
                lua_pushnil(L);
                return 1;
            }

            const auto ALGO = WORKSPACE->space()->algorithm();
            if (ALGO->tiledAlgo()->layoutName() != "scrolling") {
                lua_pushnil(L);
                return 1;
            }

            lua_pushnumber(L, dynamic_cast<Layout::Tiled::CScrollingAlgorithm*>(ALGO->tiledAlgo().get())->m_scrollingData->controller->getOffset());
            return 1;
        }

        
        static int hlScrollingSetOffset(lua_State* L) {

            const auto   WORKSPACE = L ? Config::Lua::Bindings::Internal::workspaceFromLuaSelectorOrObject(L, 1, "hl.get_scrolling_offset") : Desktop::focusState()->monitor()->getCurrentWorkspace();
            if (!WORKSPACE || !WORKSPACE->space() || !WORKSPACE->space()->algorithm()) {
                lua_pushnil(L);
                return 1;
            }

            const auto ALGO = WORKSPACE->space()->algorithm();
            if (ALGO->tiledAlgo()->layoutName() != "scrolling") {
                lua_pushnil(L);
                return 1;
            }

            if (!lua_isnumber(L, 0))
                return Config::Lua::Bindings::Internal::configError(L, "hl.set_scrolling_offset expects a number as argument.");

            const double NEW_OFFSET = sc<double>(lua_tonumber(L, 0));

            dynamic_cast<Layout::Tiled::CScrollingAlgorithm*>(ALGO->tiledAlgo().get())->m_scrollingData->controller->setOffset(NEW_OFFSET);
            
            return 1;
        }



      private:
        SP<SScrollingData>                                                            m_scrollingData;

        CHyprSignalListener                                                           m_configCallback;
        CHyprSignalListener                                                           m_focusCallback;
        CHyprSignalListener                                                           m_mouseButtonCallback;

        const UP<Fullscreen::ScrollingFullscreenHandler::CScrollingFullscreenHandler> m_scrollingFullscreenHandler;

        struct {
            std::vector<float> configuredWidths;
        } m_config;

        eScrollDirection         getDynamicDirection();

        SP<SScrollingTargetData> findBestNeighbor(SP<SScrollingTargetData> pCurrent, SP<SColumnData> pTargetCol);
        SP<SScrollingTargetData> closestNode(const Vector2D& posGlobglobgabgalab);

        void                     focusTargetUpdate(SP<ITarget> target);
        void                     moveTargetTo(SP<ITarget> t, Math::eDirection dir, bool silent);
        void                     focusOnInput(SP<ITarget> target, eInputMode input);

        void                     expelTarget(SP<SScrollingTargetData> tdata, SP<SColumnData> srcCol, std::optional<int64_t> insertIdx);

        float                    defaultColumnWidth();

        friend struct SScrollingData;
        friend class Fullscreen::ScrollingFullscreenHandler::CScrollingFullscreenHandler;
    };
};
