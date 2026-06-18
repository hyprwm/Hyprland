#include "Compositor.hpp"
#include "config/shared/monitor/MonitorRuleManager.hpp"
#include "debug/log/Logger.hpp"
#include "managers/fullscreen/FullscreenController.hpp"
#include "managers/fullscreen/handler/FullscreenHandler.hpp"
#include "layout/algorithm/tiled/scrolling/ScrollingFullscreenHandler.hpp"
#include "layout/algorithm/tiled/scrolling/ScrollingAlgorithm.hpp"
#include "render/Renderer.hpp"
#include <algorithm>
#include <cstddef>
#include <optional>

using namespace Fullscreen;
using namespace Fullscreen::ScrollingFullscreenHandler;

// ERSTARR TODO - this should work. need to rebuild to see if LSP gets the correct inheritence chain.
CScrollingFullscreenHandler::CScrollingFullscreenHandler(Layout::IModeAlgorithm* algorithm) :
    IFullscreenHandler(algorithm), m_scrollingAlgorithm(static_cast<Layout::Tiled::CScrollingAlgorithm*>(algorithm)) {}

CScrollingFullscreenHandler::~CScrollingFullscreenHandler() {

    // ERSTARR TODO - ADJUST THIS! FOR HANDLER
    std::ranges::for_each(m_fsWindows, [&](const auto& e) { removeWindowFromHandler(e.first.lock()); });
    updateFullscreenFade(false);
}


// ERSTARR TODO - In all cases where window weakptr may be nullptr, i need to add a check!!!!!!
// ERSTARR TODO - IN RECALCULATE'S FUNCTION, MUST NECESSARILY DISPATCH SYNCFULLSCREENS BEFORE AND AFTER(? ideally not necessary but meh) FS OPERATIONS
    //--> [optimise: if no FS change is made, just dispatching before should be enough]
// ERSTARR TODO - Include all checks about what a FS window must be (in the list, only target in its column, etc...). The final list must be noted down in the header file

// --------------
// Public methods
// --------------

bool CScrollingFullscreenHandler::isFullscreen(const PHLWINDOW window, const std::optional<eFullscreenMode> mode, const std::optional<bool> covering) {

    if (mode.has_value() && mode.value() == FSMODE_NONE) {
        Log::logger->log(Log::ERR, "Passed mode = FSMODE_NONE into isFullscreen. This must never happpen.");
        return false;
    }

    if (!window)
        return false;


    const auto WINITR = m_fsWindows.find(window);

    // Check that the window fits the scrolling definition of FS window

    // window must exist in list (if exists it didn't expire)
    // window's internal mode must not be FSMODE_NONE
    if (WINITR == m_fsWindows.end() || WINITR->second.mode.internal == FSMODE_NONE)
        return false;

    // FS window must be the sole window in its column - covering or not
    const auto TDATA = m_scrollingAlgorithm->dataFor(window->m_target);
    if (!TDATA || !TDATA->column)
        return false;

    if (TDATA->column->targetDatas.size() != 1) {
        // ERSTARR TODO - handle this in recalculate call. This, and resizing a FS window should dispel: this'll most likely be fine. Prob only need to handle moving a FS window to another column dispelling it
        Log::logger->log(Log::DEBUG, "column->targetDatas != 1 in a column with FS window. This should have been handled earlier. Handling...");
        // error correction
        syncFullscreenWindows();
        return false;
    }


    // covering specific logic

    if (!covering.has_value()) {
        return mode.has_value() ? getFullscreenMode(WINITR->first.lock()).internal == mode.value() : true;
    }


    const auto FSMODE = getFullscreenMode(WINITR->first.lock()).internal;
    if (mode.has_value() && mode.value() != FSMODE)
        return false;

    return FSMODE == FSMODE_FULLSCREEN ? (covering.value() ? fullscreenColumnCoversMonitor(TDATA->column.lock()) : !fullscreenColumnCoversMonitor(TDATA->column.lock())) :
                                         (covering.value() ? fullscreenColumnCoversWorkArea(TDATA->column.lock()) : !fullscreenColumnCoversWorkArea(TDATA->column.lock()));
}

bool CScrollingFullscreenHandler::hasFullscreen(const std::optional<bool> covering) {
    for (const auto& e : m_fsWindows) {
        if (isFullscreen(e.first.lock(),std::nullopt,covering))
            return true;
    }
}

PHLWINDOW CScrollingFullscreenHandler::getFullscreen(const std::optional<bool> covering) {
    for (const auto& e : m_fsWindows) {
        if (const auto WINDOWREF = e.first.lock(); isFullscreen(WINDOWREF, std::nullopt, covering))
            return WINDOWREF;
    }
    return nullptr;
}

SFullscreenMode CScrollingFullscreenHandler::getFullscreenMode(const PHLWINDOW window) {
    const auto WINITR = m_fsWindows.find(window);

    return WINITR == m_fsWindows.end() ? SFullscreenMode{} : WINITR->second.mode;
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

    // If a window is being un-FSed, set its DSO and VRR in requestFullscreen()
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
                // setting the mode will be done later
                m_fsWindows.emplace(WINDOW, SFullscreenScrollState{.restoreColumnWidth = CURRENT_COL ? std::optional<float>{targetColumnWidth} : std::nullopt});
            }
        }

        expelIfMoreThanOneWindowInColDuringFS(FSMODE_FULLSCREEN);

        const auto  CURRENTCOL        = TDATA->column.lock();

        CURRENTCOL->setColumnWidth(fullscreenColumnWidth());

        // move new column into view
        m_scrollingAlgorithm->m_scrollingData->centerOrFitCol(CURRENTCOL);

        // set internal fullscreen mode
        setWindowFullscreenModeInternal(WINDOW, FSMODE_FULLSCREEN);

        // Hide all members below the FS window
        setNoMembersAboveFullscreen();

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

        const auto  CURRENTCOL        = TDATA->column.lock();

        CURRENTCOL->setColumnWidth(1.F);
        // move new column into view
        m_scrollingAlgorithm->m_scrollingData->centerOrFitCol(CURRENTCOL);

        // set internal fullscreen mode
        setWindowFullscreenModeInternal(WINDOW, FSMODE_MAXIMIZED);

        // Hide all members below the FS window
        setNoMembersAboveFullscreen();

        return FULLSCREEN_REQUEST_HANDLED_BY_LAYOUT;
    }

    // UnFS target
    // ERSTARR TODO - DO THIS
    setWindowFullscreenModeInternal(WINDOW, FSMODE_NONE);
    setNoMembersAboveFullscreen();
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


    // This should be in sync with default FS handling of setting all members below FS (IFullscreenHandler::setNoMembersAboveFullscreen())
    // for simply setting or unsetting no members above the FS window without scrolling specific logic
    const auto setNoMembersAboveFS_layoutUnaware = [&](const bool SET) {
        if (!getSpace() || !getSpace()->workspace() || !getSpace()->workspace()->m_monitor)
            return;

        const auto SPACE = getSpace();
        const auto WORKSPACE = SPACE->workspace();
        const auto MONITOR = WORKSPACE->m_monitor;

        // make all windows and layers on the same workspace under the fullscreen window
        for (const auto& e : WORKSPACE->getWindows()) {
            if (e && !isFullscreen(e) && !e->m_fadingOut && !e->m_pinned) {
                e->m_allowedOverFullscreen = !SET;
                e->updateFullscreenInputState();
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
    There is no custom layout FS behaviour for floating FS windows (always uses default behaviour).

    In scrolling layout, a fully in view tiled FS window may exist underneath a fullscreen floating window. We must keep the floating windows that were opened ontop of the tiled FS window, as well as those
    ontop of the floating FS windows that were layered ontop of the tiled FS window.
    
    To this end, we maintain a list of floating windows that are allowed over the currently FSed window.

    COVERING_FULLSCREEN_WINDOW = the covering FS window. If a covering floating FS window exists above a covering tiled FS window, this is the floating window.
                               If a defaut handled tiled FS window exists above a layout handled tiled FS window, this is the default handled tiled FS window
    LAYOUT_TILED_COVERING_FS_WINDOW = layout handled tiled covering FS window.

    If in the future floating windows can also be layout handled this logic has to be redone

    */

    const auto COVERING_FULLSCREEN_WINDOW = g_pfullscreenController->getFullscreenWindow(WORKSPACE,true);
    const auto LAYOUT_TILED_COVERING_FS_WINDOW = getFullscreen(true);

    const auto LAST_SCROLL_HANDLED_TILED_FS_WINDOW        = m_fullscreenWindowHidingState.lastTiledLayoutManagedFsWindow.lock();
    const auto LAST_SCROLL_HANDLED_TILED_FS_WINDOW_FS_MODE = m_fullscreenWindowHidingState.lastTiledLayoutManagedFsWindowMode;

    // This should never happen
    if (!COVERING_FULLSCREEN_WINDOW && LAYOUT_TILED_COVERING_FS_WINDOW) {
        // This means that controller doesn't recognise tiled layout handled FS window as fullscreen.
        Log::logger->log(Log::CRIT,
                         "Workspace doesn't recognise a tiled layout handled fullscreen/maximised window as such! This is a critical error! We will attempt to recover by ignoring "
                         "the request to setNoMembersAboveFullscreen");
        return;
    }

    // There is no FS window; tiled or floating
    if (!COVERING_FULLSCREEN_WINDOW) {
        // reset the struct
        m_fullscreenWindowHidingState.lastTiledLayoutManagedFsWindow = nullptr;
        clear_hiddenFloatingWindowsUnderFSWindow();
        // set all members as allowed over FS
        setNoMembersAboveFS_layoutUnaware(false);
        return;
    }

    // Below cases should not happen; Default handled FS windows should dispatch to their own handler's setNoMembersAboveFullscreen(). The cases below are for redundancy and error recovery.

    // If the COVERING_FULLSCREEN_WINDOW is default handled (this should not dispatch to this method at all with the new FS framework but this check is redundancy)
    if (g_pfullscreenController->getFullscreenHandler(COVERING_FULLSCREEN_WINDOW) != FULLSCREEN_HANDLER_SCROLLING) {
        Log::logger->log(Log::ERR,
                         "Default handled FS window called CScrollingFullscreenHandler::setNoMembersAboveFullscreen(). This should never happen: setNoMembersAboveFullscreen() "
                         "call should have been dispatched to default FS handler. This is a bug, but is not fatal. Recovering...");

        // same as default handling
        setNoMembersAboveFS_layoutUnaware(true);
        return;
    }

    // There's only a floating FS window
    if (COVERING_FULLSCREEN_WINDOW && !LAYOUT_TILED_COVERING_FS_WINDOW) {
        Log::logger->log(Log::ERR,
                    "non-scroll-handled FS window called CScrollingFullscreenHandler::setNoMembersAboveFullscreen(). This should never happen: setNoMembersAboveFullscreen() "
                    "call should have been dispatched to default FS handler. This is a bug, but is not fatal. Recovering...");

        // same as default handling
        clear_hiddenFloatingWindowsUnderFSWindow();
        setNoMembersAboveFS_layoutUnaware(true);
        return;
    }

    // there is a default handled FS window ontop of the layout handled tiled FS window
    if (COVERING_FULLSCREEN_WINDOW != LAYOUT_TILED_COVERING_FS_WINDOW && g_pfullscreenController->getFullscreenHandler(COVERING_FULLSCREEN_WINDOW) != FULLSCREEN_HANDLER_SCROLLING) {
        Log::logger->log(Log::ERR,
                    "non-scroll-handled FS window called CScrollingFullscreenHandler::setNoMembersAboveFullscreen(). This should never happen: setNoMembersAboveFullscreen() "
                    "call should have been dispatched to default FS handler. This is a bug, but is not fatal. Recovering...");
        // same as default handling
        setNoMembersAboveFS_layoutUnaware(true);
        return;
    }



    // layout handled tiled FS window is the only covering FS window
    if (COVERING_FULLSCREEN_WINDOW == LAYOUT_TILED_COVERING_FS_WINDOW) {
        if (!LAST_SCROLL_HANDLED_TILED_FS_WINDOW || LAST_SCROLL_HANDLED_TILED_FS_WINDOW != LAYOUT_TILED_COVERING_FS_WINDOW ||
            getFullscreenMode(LAYOUT_TILED_COVERING_FS_WINDOW).internal != LAST_SCROLL_HANDLED_TILED_FS_WINDOW_FS_MODE) {
            // we are newly scrolling onto this tiled layout handled FS window, or we are changing from maximised to fullscreen or vice versa while in the same FS window
            // redundancy - make sure the list is empty
            clear_hiddenFloatingWindowsUnderFSWindow();
            // save all floating window current on screen, then hide all
            m_fullscreenWindowHidingState.saveCurrentFsAndAllHiddenFloatingWindows(LAYOUT_TILED_COVERING_FS_WINDOW);
            setNoMembersAboveFS_layoutUnaware(true);
            return;
        } else {

            // we had a default handled FS window ontop of a fullscreened tiling window. The windows that were open after the scrolling handled tiled window was fullscreened must remain ontop of it, while those that
            // were hidden when it was being unfullscreened must remain hidden ('below' it).
            // i.e. windows that were opened after scroll handled tiled FS window and those opened ontop of the default handled FS window must remain ontop,
            // while those that were hidden under the scroll handled tiled FS window must remain under

            // make all windows and layers on the same workspace under the fullscreen window
            for (auto const& w : WORKSPACE->getWindows()) {
                if (w != COVERING_FULLSCREEN_WINDOW && !w->m_fadingOut && !w->m_pinned) {
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
    if (!LAST_SCROLL_HANDLED_TILED_FS_WINDOW) {
        if (LAYOUT_TILED_COVERING_FS_WINDOW)
            m_fullscreenWindowHidingState.lastTiledLayoutManagedFsWindow = LAYOUT_TILED_COVERING_FS_WINDOW;
        else
            m_fullscreenWindowHidingState.lastTiledLayoutManagedFsWindow = nullptr;

        clear_hiddenFloatingWindowsUnderFSWindow();
        return;
    }

    // If the function doesn't return till here, it's an error

    Log::logger->log(Log::CRIT,
                     "setNoMembersAboveFullscreen() failed to correctly execute. Current FS window: {} Current Tiled layout handled FS window: {} Current last focused tiled "
                     "layout handled FS window: {}",
                     COVERING_FULLSCREEN_WINDOW, LAYOUT_TILED_COVERING_FS_WINDOW, LAST_SCROLL_HANDLED_TILED_FS_WINDOW);
}

void CScrollingFullscreenHandler::syncFullscreenWindows() {

    // ERSTARR TODO - If we are to remove a window from the handler, DO IT VIA THE FUNCION! --> remves the window from the list after setting the col width to prev val is window still exists
    // target->column->targetDatas.size() > 1 --> if you're gettin the col data from a target, that takes care of is it == 0 case - if there's a case which it may not be implicitly checked need to handler that
        // actually probably se != 1 here


    // Remove stale entries / expired windows
    // Remove windows that have internal AND client modes FSMODE_NONE



    for (const auto& e : m_fsWindows) {
        const auto WINDOW = e.first.lock();
        // stale entry
        // window does not have scrollingTargetData (all windows in scrolling layout must)
        // window is not FS at all
        // window's space is not the same as current space
        if (!WINDOW || WINDOW->m_target->space() != getSpace() || !m_scrollingAlgorithm->dataFor(WINDOW->m_target)) {
            // simply erase from list. no need to re-set its prev col width as the window is 'invalid'
            m_fsWindows.erase(e.first);
            continue;
        }

        // window exists propely but no longer FS
        if ((!isFullscreen(WINDOW) && getFullscreenMode(WINDOW).client == FSMODE_NONE)) {
            // sets col width to prev value if present, then removes it from the handler (i.e. remove from list)
            removeWindowFromHandler(WINDOW);
            continue;
        }

        // if internal FS mode set, set its col width accordingly
        if (getFullscreenMode(WINDOW).internal != FSMODE_NONE)
            m_scrollingAlgorithm->dataFor(WINDOW->m_target)->column->setColumnWidth((getFullscreenMode(WINDOW).internal == FSMODE_FULLSCREEN ? fullscreenColumnWidth() : 1.F));

    }

}

void CScrollingFullscreenHandler::removeWindowFromHandler(PHLWINDOW window) {

    // remove from the list, set the value it had to the window if that window still exists


    const auto WINITR = m_fsWindows.find(window);

    if (WINITR == m_fsWindows.end()) {
        return;
    }

    if (!window) {
        m_fsWindows.erase(WINITR);
        return;
    }


    if (WINITR->second.restoreColumnWidth.has_value()) {
        const auto TDATA = m_scrollingAlgorithm->dataFor(window->m_target);
        if (TDATA && TDATA->column)
            TDATA->column->setColumnWidth(WINITR->second.restoreColumnWidth.value());        
    }
    window->m_layoutFlags.cantLockCursor = false;
    m_fsWindows.erase(WINITR);
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
    if (col->targetDatas.size() != 1)
        return nullptr;

    const auto& TDATA = col->targetDatas.at(0);

    return (isFullscreenTarget(TDATA) ? TDATA : nullptr);
}


// ERSTARR TODO - this is redundant - remove
bool CScrollingAlgorithm::isFullscreenTarget(SP<SScrollingTargetData> target, std::optional<eFullscreenMode> mode) const {
    // if (!target)
    //     return false;

    // const auto TARGET = target->target.lock();

    // // Target must exist
    // // target must have window
    // // Only accept layoutmanged fullscreens
    // // For a window to be FS in scrolling, it must be the only one that occupies its own column
    // if (!TARGET || !TARGET->window() || !TARGET->layoutManagedFullscreen() || target->column->targetDatas.size() > 1)
    //     return false;

    // /** If target isn't fullscreen, or @param mode provided and internal fullscreenMode doesn't match */
    // if (TARGET->fullscreenMode() == FSMODE_NONE ||
    //     (mode.has_value() && mode.value() != TARGET->fullscreenMode())) // second part of the or works because FSMODE_NONE = 0. If that changes, this has to be adjusted
    //     return false;

    // // target data doesn't match -- Redundancy
    // if (dataFor(TARGET) != target)
    //     return false;

    // // Check if the target is inside the list of fullscreen targets scrolling maintains
    // const auto& fsTargets = (mode.value_or(TARGET->fullscreenMode()) == FSMODE_FULLSCREEN) ? m_fullscreenTargets : m_maximizeTargets;

    // return std::ranges::any_of(fsTargets.begin(), fsTargets.end(), [&TARGET](const auto& elem) { return elem.target.lock() == TARGET; });
}

void CScrollingAlgorithm::clearFullscreenWindow(std::unordered_map<PHLWINDOWREF, SFullscreenScrollState>& m_fsWindows, PHLWINDOW window) {

    // ERSTARR TODO --> this should be redundant. simply call removeWindowFromHandler for the entire list. Hopefully all the stuff in here is properly transplanted to the other function: double check

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