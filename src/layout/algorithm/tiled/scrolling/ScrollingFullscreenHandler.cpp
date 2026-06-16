#include "Compositor.hpp"
#include "config/shared/monitor/MonitorRuleManager.hpp"
#include "debug/log/Logger.hpp"
#include "managers/fullscreen/FullscreenController.hpp"
#include "managers/fullscreen/handler/FullscreenHandler.hpp"
#include "layout/algorithm/tiled/scrolling/ScrollingFullscreenHandler.hpp"
#include "layout/algorithm/tiled/scrolling/ScrollingAlgorithm.hpp"
#include "render/Renderer.hpp"

using namespace Fullscreen;
using namespace Fullscreen::ScrollingFullscreenHandler;

// ERSTARR TODO - this should work. need to rebuild to see if LSP gets the correct inheritence chain.
CScrollingFullscreenHandler::CScrollingFullscreenHandler(Layout::IModeAlgorithm* algorithm) :
    IFullscreenHandler(algorithm), m_scrollingAlgorithm(static_cast<Layout::Tiled::CScrollingAlgorithm*>(algorithm)) {}

CScrollingFullscreenHandler::~CScrollingFullscreenHandler() {

    // ERSTARR TODO - ADJUST THIS! FOR HANDLER
    clearFullscreenTarget(m_maximizeTargets);
    clearFullscreenTarget(m_fullscreenTargets);
    updateFullscreenFade(false);

}

// --------------
// Public methods
// --------------

bool CScrollingFullscreenHandler::isFullscreen(const PHLWINDOW window, const std::optional<eFullscreenMode> mode, const std::optional<bool> covering) {

    if (mode.value() != FSMODE_NONE) {
        Log::logger->log(Log::ERR, "Passed mode = FSMODE_NONE into isFullscreen. This must never happpen.");
        return false;
    }

    const auto WINITR = m_fsWindows.find(window);

    if (WINITR == m_fsWindows.end())
        return false;

    // FS window must be the sole window in its column
    const auto TDATA = m_scrollingAlgorithm->dataFor(window->m_target);
    if (!TDATA || !TDATA->column)
        false;

    if (TDATA->column->targetDatas.size() > 1) {
        // ERSTARR TODO - handle this in recalculate call. This, and resizing a FS window should dispel: this'll most likely be fine. Prob only need to handle moving a FS window to another column dispelling it
        Log::logger->log(Log::DEBUG, "More than one target data in a col with FS window. This should have been handled earlier. Handling...");
        // error correction
        syncFullscreenWindows();
        return false;
    }

    if (!covering.has_value())
        return mode.has_value() ? WINITR->second.mode.internal == mode.value() : true;
    if (covering.value()) {
        // is window covering function dispatch
    }
    if (!covering.value()) {
        // !is window covering function dispatch
    }
}

bool CScrollingFullscreenHandler::hasFullscreen(const std::optional<bool> covering) {
    // TODO like abnove - i need the covering function
}

PHLWINDOW CScrollingFullscreenHandler::getFullscreen(const std::optional<bool> covering) {
    // TODO like abnove - i need the covering function

    if (covering) {

        // ERSTARR NEEDS TO BE ADJUSTED
        for (const auto& COL : m_scrollingData->columns) {
            for (const auto& TDATA : COL->targetDatas) {
                if (!isFullscreenTarget(TDATA))
                    continue;

                if (const auto TARGET = TDATA->target; TARGET && TARGET->fullscreenMode() == FSMODE_FULLSCREEN ? fullscreenColumnCoversMonitor(TDATA->column.lock()) :
                                                                                                                fullscreenColumnCoversWorkArea(TDATA->column.lock()))
                    return TARGET.lock();
            }
        }

        return nullptr;
    }


}

SFullscreenMode CScrollingFullscreenHandler::getFullscreenMode(const PHLWINDOW window) {

    return isFullscreen(window) ? m_fsWindows.find(window)->second.mode : SFullscreenMode{};
}

eFullscreenRequestResult CScrollingFullscreenHandler::requestFullscreen(const SFullscreenRequest& request) {

    if (!request.target || !getSpace() || request.target->space() != getSpace() || !m_scrollingAlgorithm->dataFor(request.target) || !request.target->window())
        return FULLSCREEN_REQUEST_FAILED;

    const auto TARGET = request.target;
    const auto TDATA  = m_scrollingAlgorithm->dataFor(request.target);

    const auto WINDOW    = TARGET->window();
    const auto WORKSPACE = WINDOW->m_workspace;
    const auto MONITOR   = WORKSPACE->m_monitor.lock();

    // lambda for expelling if there is more than one window in a column when FSing a target.
    const auto expelIfMoreThanOneWindowInColDuringFS = [&](eFullscreenMode mode) -> void {
        const auto CURRENTCOL = TDATA->column.lock();

        // more that one window in column
        if (CURRENTCOL->targetDatas.size() > 1) {
            const auto lastTarget = CURRENTCOL->targetDatas.back();
            const auto currentIdx = m_scrollingAlgorithm->m_scrollingData->idx(CURRENTCOL);
            const auto NEXT_COL   = m_scrollingAlgorithm->m_scrollingData->next(CURRENTCOL);
            const auto insertIdx  = !NEXT_COL ? std::nullopt : std::optional<int64_t>{currentIdx};

            m_scrollingAlgorithm->expelTarget(lastTarget, CURRENTCOL, insertIdx);
        }
    };

    // If a window is being un-FSed, set its DSO and VRR in CScrollingAlgorithm::requestFullscreen()
    // If we are scrolling away from an FS window that is not unFSed yet, we set its DSO and VRR in SScrollingData::recalculate()
    // If a window is being FS-ed, or we are scrolling onto a FSed window, we set its DSO and VRR in SScrollingData::recalculate()

    if (request.mode == FSMODE_NONE) {

        // send a regular tranche if we are exiting fullscreen.
        // ignore if DS is disabled.
        static auto PDIRECTSCANOUT = CConfigValue<Config::INTEGER>("render:direct_scanout");

        if (*PDIRECTSCANOUT == 1 || (*PDIRECTSCANOUT == 2 && WINDOW->getContentType() == NContentType::CONTENT_TYPE_GAME)) {
            auto surf = WINDOW->getSolitaryResource();
            if (surf)
                g_pHyprRenderer->setSurfaceScanoutMode(surf, nullptr);
        }
        Config::monitorRuleMgr()->ensureVRR(MONITOR);
    }

    // TODO much of the code below is repeated more then it has to; refactor

    if (request.mode == FSMODE_FULLSCREEN) {
        const auto CURRENT_COL = TDATA->column.lock();

        // if current target isn't fullscreen, save its column width
        if (!isFullscreen(WINDOW, FSMODE_FULLSCREEN)) {

            float targetColumnWidth = 0.0F;

            // If the window was maximised, save the col width it had before being FSed at all
            if (isFullscreen(WINDOW, FSMODE_MAXIMIZED))
                targetColumnWidth = getTargetColumnWidthBeforeFullscreenOrMaximise(TARGET);
            else
                targetColumnWidth = CURRENT_COL->getColumnWidth();

            const auto WINITR = m_fsWindows.find(WINDOW);
            if (WINITR != m_fsWindows.end()) {
                WINITR->second.restoreColumnWidth = CURRENT_COL ? std::optional<float>{targetColumnWidth} : std::nullopt;
            } else {
                // redundancy as it should already be saved as FSMODE_FULLSCREEN by now.
                m_fsWindows.emplace(WINDOW, SFullscreenScrollState{.restoreColumnWidth = CURRENT_COL ? std::optional<float>{targetColumnWidth} : std::nullopt});
            }
        }

        expelIfMoreThanOneWindowInColDuringFS(FSMODE_FULLSCREEN);

        const float TARGETCOLUMNWIDTH = fullscreenColumnWidth();
        const auto  CURRENTCOL        = TDATA->column.lock();

        CURRENTCOL->setColumnWidth(TARGETCOLUMNWIDTH);

        // move new column into view
        m_scrollingAlgorithm->m_scrollingData->centerOrFitCol(CURRENTCOL);

        // set internal fullscreen mode
        setWindowFullscreenModeInternal(WINDOW, FSMODE_FULLSCREEN);

        // Hide all members below the FS window
        setNoMembersAboveFullscreen();

        syncFullscreenWindows(); // ERSTARR TODO - redundant as it's called in set internal

        return FULLSCREEN_REQUEST_HANDLED_BY_LAYOUT;

    } else if (request.mode == FSMODE_MAXIMIZED) {

        const auto CURRENT_COL = TDATA->column.lock();

        if (!isFullscreen(WINDOW, FSMODE_MAXIMIZED)) {

            float targetColumnWidth = 0.0F;

            // If the window was fullscreened, save the col width it had before being FSed at all
            if (isFullscreen(WINDOW, FSMODE_FULLSCREEN))
                targetColumnWidth = getTargetColumnWidthBeforeFullscreenOrMaximise(TARGET);
            else
                targetColumnWidth = CURRENT_COL->getColumnWidth();

            const auto WINITR = m_fsWindows.find(WINDOW);
            if (WINITR != m_fsWindows.end()) {
                WINITR->second.restoreColumnWidth = CURRENT_COL ? std::optional<float>{targetColumnWidth} : std::nullopt;
            } else {
                // redundancy as it should already be saved as FSMODE_MAXIMIZED by now.
                m_fsWindows.emplace(WINDOW, SFullscreenScrollState{.restoreColumnWidth = CURRENT_COL ? std::optional<float>{targetColumnWidth} : std::nullopt});
            }
        }

        expelIfMoreThanOneWindowInColDuringFS(FSMODE_MAXIMIZED);

        const float TARGETCOLUMNWIDTH = 1.F;
        const auto  CURRENTCOL        = TDATA->column.lock();

        CURRENTCOL->setColumnWidth(TARGETCOLUMNWIDTH);
        // move new column into view
        m_scrollingAlgorithm->m_scrollingData->centerOrFitCol(CURRENTCOL);

        // set internal fullscreen mode
        setWindowFullscreenModeInternal(WINDOW, FSMODE_MAXIMIZED);

        // Hide all members below the FS window
        setNoMembersAboveFullscreen();

        syncFullscreenWindows(); // ERSTARR TODO - redundant as it's called in set internal

        return FULLSCREEN_REQUEST_HANDLED_BY_LAYOUT;
    }

    // UnFS target
    setWindowFullscreenModeInternal(WINDOW, FSMODE_NONE);

    setNoMembersAboveFullscreen();

    syncFullscreenWindows(); // ERSTARR TODO - redundant as it's called in set internal

    return (request.mode == FSMODE_NONE && !isFullscreen(WINDOW)) ? FULLSCREEN_REQUEST_HANDLED_BY_LAYOUT : FULLSCREEN_REQUEST_FAILED;
}

void CScrollingFullscreenHandler::setWindowFullscreenModeInternal(const PHLWINDOW window, const eFullscreenMode mode) {

    const auto& WINITR = m_fsWindows.find(window);

    if (mode == FSMODE_NONE) {
        if (WINITR != m_fsWindows.end()) {
            WINITR->second.mode.internal = FSMODE_NONE;
        }
    } else if (WINITR == m_fsWindows.end()) {
        m_fsWindows.emplace(window, SFullscreenMode{.internal = mode});
    } else {
        WINITR->second.mode.internal = mode;
    }

    syncFullscreenWindows();
}

void CScrollingFullscreenHandler::setWindowFullscreenModeClient(const PHLWINDOW window, const eFullscreenMode mode) {

    const auto& WINITR = m_fsWindows.find(window);

    if (mode == FSMODE_NONE) {
        if (WINITR != m_fsWindows.end()) {
            WINITR->second.mode.client = FSMODE_NONE;
        }
    } else if (WINITR == m_fsWindows.end()) {
        m_fsWindows.emplace(window, SFullscreenMode{.client = mode});
    } else {
        WINITR->second.mode.client = mode;
    }

    syncFullscreenWindows();
}

void CScrollingFullscreenHandler::moveFullscreenWindowToHandler(const PHLWINDOW window, const std::optional<bool> covering) {

    // On hold for now. To standardise might just say "unfullscreen and refullscreen on target if covering" and let calls outside of the FS framework implement this
}

void CScrollingFullscreenHandler::moveFullscreenWindowOutOfHandler(const PHLWINDOW window) {
    // On hold for now. To standardise might just say "unfullscreen and refullscreen on target if covering" and let calls outside of the FS framework implement this
}


// ERSTARR TODO - need to adjust the comments 
void CScrollingFullscreenHandler::setNoMembersAboveFullscreen() {

    if (!m_scrollingAlgorithm->m_parent || !getSpace() || !getSpace()->workspace() || !getSpace()->workspace()->m_monitor)
        return;

    const auto WORKSPACE = getSpace()->workspace();
    const auto MONITOR = WORKSPACE->m_monitor;


    // Returns the FS window "ontop": If a floating FS window is layered ontop of a Scrolling Tiled FS window, it returns the floating FS window
    const auto COVERING_FS_WINDOW = g_pfullscreenController->getFullscreenWindow(WORKSPACE,true);

    // This should be in sync with default FS handling of setting all members below FS (IModeAlgorithm::setNoMembersAboveFullscreen())
    // for simply setting or unsetting no members above the FS window without scrolling specific logic
    const auto setNoMembersAboveFS_layoutUnaware = [&](const bool SET) {
        // make all windows and layers on the same workspace under the fullscreen window
        for (auto const& w : WORKSPACE->getWindows()) {
            if (w != FULLSCREEN_WINDOW && !w->m_fadingOut && !w->m_pinned) {
                w->m_allowedOverFullscreen = !SET;
                w->updateFullscreenInputState();
            }
        }
        for (auto const& ls : g_pCompositor->m_layers) {
            if (ls->m_monitor == MONITOR)
                ls->m_aboveFullscreen = !SET;
        }
    };

    const auto clear_hiddenFloatingWindowsUnderFSWindow = [&]() {
        m_fullscreenWindowHidingState.hiddenFloatingWindowsUnderFSWindow.clear();
        if (!m_fullscreenWindowHidingState.hiddenFloatingWindowsUnderFSWindow.empty())
            Log::logger->log(
                Log::ERR,
                "hiddenFloatingWindowsUnderFSWindow.clear() failed. Will likely cause the mishandling of floating window hiding upon fullscreening on scrolling layout. This is an "
                "error but is not critical.");
    };

    /*
    In scrolling layout, a fully in view tiled FS window may exist underneath a fullscreen floating window. We must keep the floating windows that were opened ontop of the tiled FS window, as well as those
    ontop of the floating FS windows that were layered ontop of the tiled FS window.
    
    To this end, we maintain a list of floating windows that are allowed over the currently FSed window
    */

    // In scrolling, there is no custom layout FS behaviour for floating FS windows (always uses default behaviour), and layoutFullscreenTarget() correctly gets the tiled currently FS window under a floating FS window
    // therefore UNDERLYING_FS_WINDOW might be different than FULLSCREEN_WINDOW if there is a floating FS window "ontop" of the tiled currently FS window
    const auto UNDERLYING_FS_WINDOW = [&]() -> PHLWINDOW {
        if (auto const TARGET = layoutFullscreenTarget(); TARGET)
            return TARGET->window();
        return nullptr;
    }();

    const auto LAST_TILED_FS_WINDOW        = m_fullscreenWindowHidingState.lastTiledLayoutManagedFsWindow.lock();
    const auto LAST_TILED_FS_WINDOW_FSMODE = m_fullscreenWindowHidingState.lastTiledLayoutManagedFsWindowMode;

    // This should never happen
    if (!FULLSCREEN_WINDOW && UNDERLYING_FS_WINDOW) {
        // This means that workspace doesn't recognise tiled layout handled FS window as fullscreen.
        Log::logger->log(Log::CRIT,
                         "Workspace doesn't recognise a tiled layout handled fullscreen/maximised window as such! This is a critical error! We will attempt to recover by ignoring "
                         "the request to "
                         "setNoMembersAboveFullscreen");
        return;
    }

    // There is no FS window; tiled or floating
    if (!FULLSCREEN_WINDOW) {
        // reset the struct
        m_fullscreenWindowHidingState.lastTiledLayoutManagedFsWindow = nullptr;
        clear_hiddenFloatingWindowsUnderFSWindow();
        // set all members as allowed over FS
        setNoMembersAboveFS_layoutUnaware(false);
        return;
    }

    // If the tiled FS window is default handled
    if (!FULLSCREEN_WINDOW->m_isFloating && !FULLSCREEN_WINDOW->m_target->layoutManagedFullscreen()) {
        // same as default handling
        setNoMembersAboveFS_layoutUnaware(true);
        return;
    }

    // There's only a floating FS window
    if (COVERING_FS_WINDOW && !UNDERLYING_FS_WINDOW) {
        // same as default handling
        clear_hiddenFloatingWindowsUnderFSWindow();
        setNoMembersAboveFS_layoutUnaware(true);
        return;
    }

    // there is a floating FS window ontop of the tiled layout handled FS window
    if (COVERING_FS_WINDOW != UNDERLYING_FS_WINDOW && FULLSCREEN_WINDOW->m_isFloating) {
        // same as default handling
        setNoMembersAboveFS_layoutUnaware(true);
        return;
    }

    // There is no floating FS ontop of the tiled layout handled FS
    if (COVERING_FS_WINDOW == UNDERLYING_FS_WINDOW) {
        if (!LAST_TILED_FS_WINDOW || LAST_TILED_FS_WINDOW != UNDERLYING_FS_WINDOW || UNDERLYING_FS_WINDOW->m_fullscreenState.internal != LAST_TILED_FS_WINDOW_FSMODE) {
            // we are newly scrolling onto this tiled layout handled FS window, or we are changing from maximised to fullscreen or vice versa while in the same FS window
            // redundancy - make sure the list is empty
            clear_hiddenFloatingWindowsUnderFSWindow();
            // save all floating window current on screen, then hide all
            m_fullscreenWindowHidingState.saveCurrentFsAndAllHiddenFloatingWindows(UNDERLYING_FS_WINDOW);
            setNoMembersAboveFS_layoutUnaware(true);
            return;
        } else {

            // we fullscreened a floating window ontop of a fullscreened tiling window. The windows that were open after that tiling window was fullscreened must remain ontop of it, while those that
            // were hidden when it was being unfullscreened must remain hidden ('below' it)

            // make all windows and layers on the same workspace under the fullscreen window
            for (auto const& w : WORKSPACE->getWindows()) {
                if (w != FULLSCREEN_WINDOW && !w->m_fadingOut && !w->m_pinned) {
                    // If it the window was hidden when the UNDERLYING_FS_WINDOW was FSed, or it is a tiled window; it remains hidden. else, it is allowed ontop of it
                    w->m_allowedOverFullscreen = w->m_isFloating && !m_fullscreenWindowHidingState.hiddenFloatingWindowsUnderFSWindow.contains(w);
                    w->updateFullscreenInputState();
                }
            }
            for (auto const& ls : g_pCompositor->m_layers) {
                if (ls->m_monitor == MONITOR)
                    ls->m_aboveFullscreen = false;
            }

            return;
        }
    }

    // If the window was closed or otherwise not available anymore
    if (!LAST_TILED_FS_WINDOW) {
        if (UNDERLYING_FS_WINDOW)
            m_fullscreenWindowHidingState.lastTiledLayoutManagedFsWindow = UNDERLYING_FS_WINDOW;
        else
            m_fullscreenWindowHidingState.lastTiledLayoutManagedFsWindow = nullptr;

        clear_hiddenFloatingWindowsUnderFSWindow();
        return;
    }

    // If the function doesn't return till here, it's an error

    Log::logger->log(Log::CRIT,
                     "setNoMembersAboveFullscreen() failed to correctly execute. Current FS window: {} Current Tiled layout handled FS window: {} Current last focused tiled "
                     "layout handled FS window: {}",
                     FULLSCREEN_WINDOW, UNDERLYING_FS_WINDOW, LAST_TILED_FS_WINDOW);
}

void CScrollingFullscreenHandler::syncFullscreenWindows() {

    // ERSTARR TODO - If we are to remove a window from the handler, DO IT VIA THE FUNCION!



    // This lambda is a copy of CScrollingAlgorithm::isFullscreenTarget() without the check for if the target is in the scrolling maintained fullscreen/maximise target list
    const auto isFullscreenTargetWithoutTargetsListCheck = [&](SP<SScrollingTargetData> target) -> bool {
        if (!target)
            return false;

        const auto TARGET = target->target.lock();

        if (!TARGET || !TARGET->window() || !TARGET->layoutManagedFullscreen() || target->column->targetDatas.size() > 1 || dataFor(TARGET) != target)
            return false;

        return TARGET->fullscreenMode() != FSMODE_NONE;
    };

    // Clean stale entries and restore col width - Fullscreened (mode = FSMODE_FULLSCREEN)
    for (auto it = m_fullscreenTargets.begin(); it != m_fullscreenTargets.end();) {
        const auto TARGET = it->target.lock();

        if (!TARGET || TARGET->space() != m_parent->space() || (TARGET->layoutManagedFullscreen() && !isFullscreenTarget(dataFor(TARGET), FSMODE_FULLSCREEN))) {
            it = m_fullscreenTargets.erase(it);
            continue;
        }

        const auto TDATA = dataFor(TARGET);
        if (!TDATA) {
            ++it;
            continue;
        }

        if (const auto COL = TDATA->column.lock())
            COL->setColumnWidth(fullscreenColumnWidth());

        ++it;
    }

    // Clean stale entries and restore col width - Maximised (mode = FSMODE_MAXIMIZED)
    for (auto it = m_maximizeTargets.begin(); it != m_maximizeTargets.end();) {
        const auto TARGET = it->target.lock();

        if (!TARGET || TARGET->space() != m_parent->space() || (TARGET->layoutManagedFullscreen() && !isFullscreenTarget(dataFor(TARGET), FSMODE_MAXIMIZED))) {
            it = m_maximizeTargets.erase(it);
            continue;
        }

        const auto TDATA = dataFor(TARGET);
        if (!TDATA) {
            ++it;
            continue;
        }

        if (const auto COL = TDATA->column.lock())
            COL->setColumnWidth(1.F);

        ++it;
    }

    for (const auto& COL : m_scrollingData->columns) {
        for (const auto& TDATA : COL->targetDatas) {
            const auto            TARGET = TDATA->target.lock();

            const eFullscreenMode TARGETFULLSCREENMODE = TARGET->fullscreenMode();

            // Possible Optimisation: if target isn't FS, it won't be layoutManagedFullscreen anyway) - but left this way for redundancy
            if (TARGETFULLSCREENMODE == FSMODE_NONE || !TARGET->layoutManagedFullscreen())
                continue;

            auto fsTargets = TARGETFULLSCREENMODE == FSMODE_FULLSCREEN ? m_fullscreenTargets : m_maximizeTargets;

            // If a target is a FS window but not in the fullscreen/maximise target list yet, emplace it.
            const bool INLIST = std::ranges::any_of(fsTargets, [&](const auto& s) { return s.target.lock() == TARGET; });

            if (TARGET->space() == m_parent->space() && isFullscreenTargetWithoutTargetsListCheck(dataFor(TARGET)) && !INLIST) {
                fsTargets.emplace_back(SFullscreenScrollState{.target = TARGET, .restoreColumnWidth = COL ? std::optional<float>{COL->getColumnWidth()} : std::nullopt});

                COL->setColumnWidth((TARGETFULLSCREENMODE == FSMODE_FULLSCREEN ? fullscreenColumnWidth() : 1.F));
            }
        }
    }


}

void CScrollingFullscreenHandler::removeFSWindowFromHandler(PHLWINDOW window) {

    // remove from the list, set the value it had to the window if that window still exists

    bool cleared = false;

    auto clear = [&](SP<ITarget> t) {
        t->setLayoutManagedFullscreen(false);
        if (t->window())
            t->window()->m_layoutFlags.cantLockCursor = false;
        cleared = true;
    };

    for (auto it = fullscreenTargetList.begin(); it != fullscreenTargetList.end();) {
        const auto TARGET = it->target.lock();

        if (!TARGET || (target && TARGET != target)) {
            if (!TARGET)
                it = fullscreenTargetList.erase(it);
            else
                ++it;
            continue;
        }

        const auto TDATA = dataFor(TARGET);

        clear(TARGET);

        if (const auto COL = TDATA ? TDATA->column.lock() : nullptr; COL && it->restoreColumnWidth)
            COL->setColumnWidth(*it->restoreColumnWidth);

        it = fullscreenTargetList.erase(it);
    }

    if (target && target->layoutManagedFullscreen())
        clear(target);
    else if (!target) {
        for (const auto& COL : m_scrollingData->columns) {
            for (const auto& TDATA : COL->targetDatas) {
                const auto TARGET = TDATA->target.lock();
                if (!TARGET || !TARGET->layoutManagedFullscreen())
                    continue;

                clear(TARGET);
            }
        }
    }
}

eFullscreenHandler CScrollingFullscreenHandler::getFullscreenHandlerName() const {
    return FULLSCREEN_HANDLER_TYPE;
}

// ----------------------
// Private helper methods
// ----------------------


SP<Layout::Tiled::SScrollingTargetData> CScrollingFullscreenHandler::fullscreenTargetDataForColumn(SP<Layout::Tiled::SColumnData> col) const {

    if (!col)
        return nullptr;

    // For a column to have a FS target, it must only have one target; as FS windows must occupy a column by themselves
    if (col->targetDatas.size() > 1)
        return nullptr;

    const auto& TDATA = col->targetDatas.at(0);

    return (isFullscreenTarget(TDATA) ? TDATA : nullptr);
}

bool CScrollingAlgorithm::isFullscreenTarget(SP<SScrollingTargetData> target, std::optional<eFullscreenMode> mode) const {
    if (!target)
        return false;

    const auto TARGET = target->target.lock();

    // Target must exist
    // target must have window
    // Only accept layoutmanged fullscreens
    // For a window to be FS in scrolling, it must be the only one that occupies its own column
    if (!TARGET || !TARGET->window() || !TARGET->layoutManagedFullscreen() || target->column->targetDatas.size() > 1)
        return false;

    /** If target isn't fullscreen, or @param mode provided and internal fullscreenMode doesn't match */
    if (TARGET->fullscreenMode() == FSMODE_NONE ||
        (mode.has_value() && mode.value() != TARGET->fullscreenMode())) // second part of the or works because FSMODE_NONE = 0. If that changes, this has to be adjusted
        return false;

    // target data doesn't match -- Redundancy
    if (dataFor(TARGET) != target)
        return false;

    // Check if the target is inside the list of fullscreen targets scrolling maintains
    const auto& fsTargets = (mode.value_or(TARGET->fullscreenMode()) == FSMODE_FULLSCREEN) ? m_fullscreenTargets : m_maximizeTargets;

    return std::ranges::any_of(fsTargets.begin(), fsTargets.end(), [&TARGET](const auto& elem) { return elem.target.lock() == TARGET; });
}


float CScrollingFullscreenHandler::fullscreenColumnWidth() const {
    if (!m_parent || !m_parent->space() || !m_parent->space()->workspace() || !m_parent->space()->workspace()->m_monitor || !m_scrollingData || !m_scrollingData->controller)
        return 1.F;

    const auto   USABLE         = usableArea();
    const auto   MONBOX         = m_parent->space()->workspace()->m_monitor->logicalBox();
    const bool   PRIMARY_HORIZ  = m_scrollingData->controller->isPrimaryHorizontal();
    const double usablePrimary  = PRIMARY_HORIZ ? USABLE.w : USABLE.h;
    const double monitorPrimary = PRIMARY_HORIZ ? MONBOX.w : MONBOX.h;

    if (usablePrimary <= 0.0)
        return 1.F;

    return std::max(1.F, sc<float>(monitorPrimary / usablePrimary));
}

bool CScrollingFullscreenHandler::fullscreenColumnCoversMonitor(SP<Layout::Tiled::SColumnData> col) const {
    if (!col || !m_scrollingData || !m_scrollingData->controller || !m_parent || !m_parent->space() || !m_parent->space()->workspace() ||
        !m_parent->space()->workspace()->m_monitor)
        return false;

    if (!fullscreenTargetDataForColumn(col))
        return false;

    const int64_t COL_IDX = m_scrollingData->idx(col);
    if (COL_IDX < 0)
        return false;

    static const auto PFSONONE = CConfigValue<Config::INTEGER>("scrolling:fullscreen_on_one_column");

    const auto        USABLE        = usableArea();
    const bool        PRIMARY_HORIZ = m_scrollingData->controller->isPrimaryHorizontal();
    const double      VIEW_SIZE     = PRIMARY_HORIZ ? USABLE.w : USABLE.h;
    const double      VIEW_START    = m_scrollingData->controller->getOffset();
    const double      VIEW_END      = VIEW_START + VIEW_SIZE;
    const double      COL_START     = m_scrollingData->controller->calculateStripStart(COL_IDX, USABLE, *PFSONONE);
    const double      COL_END       = COL_START + m_scrollingData->controller->calculateStripSize(COL_IDX, USABLE, *PFSONONE);

    return COL_START <= VIEW_START + 1.0 && COL_END >= VIEW_END - 1.0;

}

bool CScrollingFullscreenHandler::fullscreenColumnCoversWorkArea(SP<Layout::Tiled::SColumnData> col) const {
    // Covers Monitor check also works for maximised windows in scrolling layout.
    return fullscreenColumnCoversMonitor(col);

}

void  CScrollingFullscreenHandler::updateFullscreenFade(bool coversMonitor) {

    if (!coversMonitor) {
        // prevent stuck focus
        g_pInputManager->unconstrainMouse();
        for (const auto& fs : m_fullscreenTargets) {
            if (!fs.target || !fs.target->window())
                continue;

            auto w = fs.target->window();

            w->m_layoutFlags.cantLockCursor = true;
        }
    } else {
        for (const auto& fs : m_fullscreenTargets) {
            if (!fs.target || !fs.target->window())
                continue;

            auto w = fs.target->window();

            w->m_layoutFlags.cantLockCursor = false;
        }
    }

    if (!m_parent || !m_parent->space() || !m_parent->space()->workspace())
        return;

    g_pDesktopAnimationManager->setFullscreenFadeAnimation(m_parent->space()->workspace(),
                                                           coversMonitor ? CDesktopAnimationManager::ANIMATION_TYPE_IN : CDesktopAnimationManager::ANIMATION_TYPE_OUT);
}

float CScrollingFullscreenHandler::getTargetColumnWidthBeforeFullscreenOrMaximise(SP<Layout::ITarget> target) {
    if (!target || target->fullscreenMode() == FSMODE_NONE)
        return 0.5; // fallback to col width of 0.5

    const auto& fsTargets = (fullscreenStateForTarget(target, FSMODE_FULLSCREEN) ? m_fullscreenTargets : m_maximizeTargets);

    for (auto it = fsTargets.begin(); it != fsTargets.end();) {

        if (it->target.lock() == target)
            return it->restoreColumnWidth.value_or(0.5F);

        ++it;
    }

    return 0.5F;
}

// -----------------------------------------------------------
// Nested struct SScrollingFullscreenWindowHidingState method
// -----------------------------------------------------------

void SScrollingFullscreenWindowHidingState::saveCurrentFsAndAllHiddenFloatingWindows(PHLWINDOW fullscreenWindow) {

    // we save all the floating windows that will be hidden under the fullscreen. We are using the same logic that is used to judge which window is to be hidden + a float check.
    // This function must be updated whenever this logic is changed (IModeAlgorithm::setNoMembersAboveFullscreen(), CScrollingAlgorithm::setNoMembersAboveFullscreen())

    // fullscreenWindow is assumed to be tiled, layout handled, covers the whole monitor or work area.

    lastTiledLayoutManagedFsWindow     = fullscreenWindow;
    lastTiledLayoutManagedFsWindowMode = fullscreenWindow->m_fullscreenState.internal;

    const auto WORKSPACE = fullscreenWindow->m_workspace;

    for (auto const& w : WORKSPACE->getWindows()) {
        if (w != fullscreenWindow && !w->m_fadingOut && !w->m_pinned && w->m_isFloating)
            hiddenFloatingWindowsUnderFSWindow.emplace(w);
    }
}