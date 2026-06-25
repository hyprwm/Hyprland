#include "Compositor.hpp"
#include "config/shared/monitor/MonitorRuleManager.hpp"
#include "debug/log/Logger.hpp"
#include "desktop/DesktopTypes.hpp"
#include "managers/animation/DesktopAnimationManager.hpp"
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
CScrollingFullscreenHandler::CScrollingFullscreenHandler(Layout::Tiled::CScrollingAlgorithm* const algorithm) :
    IFullscreenHandler(algorithm), m_scrollingAlgorithm(algorithm) {
    if (!m_scrollingAlgorithm) {
        Log::logger->log(Log::CRIT, "CScrollingFullscreenHandler failed during construction: Owning layout algorithm does not exist!");
        throw std::runtime_error("CScrollingFullscreenHandler: bad algorithm type");
    }
}

CScrollingFullscreenHandler::~CScrollingFullscreenHandler() {

    // ERSTARR TODO - ADJUST THIS! FOR HANDLER
    for (auto it = m_fsTargets.begin(); it != m_fsTargets.end(); ) {
        const auto NEXT = std::next(it); // save next before removeFsTarget invalidates it
        removeFsTarget(it->first.lock());
        it = NEXT;
    }
    updateFullscreenFade(false);
}


// ERSTARR TODO - In all cases where target weakptr may be nullptr, i need to add a check!!!!!!
// ERSTARR TODO - IN RECALCULATE'S FUNCTION, MUST NECESSARILY DISPATCH SYNCFULLSCREENS BEFORE AND AFTER(? ideally not necessary but meh) FS OPERATIONS
    //--> [optimise: if no FS change is made, just dispatching before should be enough]
// ERSTARR TODO - Include all checks about what a FS target must be (in the list, only target in its column, etc...). The final list must be noted down in the header file




bool CScrollingFullscreenHandler::isFullscreen(const SP<Layout::ITarget> target, const std::optional<eFullscreenMode> mode, const std::optional<bool> covering) {

    if (mode.has_value() && mode.value() == FSMODE_NONE) {
        Log::logger->log(Log::ERR, "Passed mode = FSMODE_NONE into isFullscreen. This must never happpen.");
        return false;
    }

    if (!target)
        return false;


    const auto ITR = m_fsTargets.find(target);

    // Check that the target fits the scrolling definition of FS target

    // target must exist in list (if exists it didn't expire)
    // target's internal mode must not be FSMODE_NONE
    if (ITR == m_fsTargets.end() || ITR->second.mode.internal == FSMODE_NONE)
        return false;

    // FS target must be the sole target in its column - covering or not
    const auto TDATA = m_scrollingAlgorithm->dataFor(target);
    if (!TDATA || !TDATA->column)
        return false;

    if (TDATA->column->targetDatas.size() != 1) {
        // ERSTARR TODO - handle this in recalculate call. This, and resizing a FS target should dispel: this'll most likely be fine. Prob only need to handle moving a FS window to another column dispelling it
        Log::logger->log(Log::DEBUG, "column->targetDatas != 1 in a column with FS target");
        return false;
    }


    // covering specific logic

    if (!covering.has_value()) {
        return mode.has_value() ? getFullscreenModes(ITR->first.lock()).internal == mode.value() : true;
    }


    const auto FSMODE = getFullscreenModes(ITR->first.lock()).internal;
    if (mode.has_value() && mode.value() != FSMODE)
        return false;

    return FSMODE == FSMODE_FULLSCREEN ? (covering.value() ? columnCoversMonitor(TDATA->column.lock()) : !columnCoversMonitor(TDATA->column.lock())) :
                                         (covering.value() ? columnCoversWorkArea(TDATA->column.lock()) : !columnCoversWorkArea(TDATA->column.lock()));
}

bool CScrollingFullscreenHandler::hasFullscreen(const std::optional<bool> covering) {
    return std::ranges::any_of(m_fsTargets, [&](const auto& e){return isFullscreen(e.first.lock(),std::nullopt,covering);});
}

SP<Layout::ITarget> CScrollingFullscreenHandler::getFullscreen(const std::optional<bool> covering) {
    for (const auto& e : m_fsTargets) {
        if (const auto TARGET = e.first.lock(); isFullscreen(TARGET, std::nullopt, covering))
            return TARGET;
    }
    return nullptr;
}

SFullscreenMode CScrollingFullscreenHandler::getFullscreenModes(const SP<Layout::ITarget> target) {
    const auto ITR = m_fsTargets.find(target);

    return ITR == m_fsTargets.end() ? SFullscreenMode{} : ITR->second.mode;
}


eFullscreenRequestResult CScrollingFullscreenHandler::requestFullscreen(const SFullscreenRequest& request) {

    if (!request.target || !getSpace() || request.target->space() != getSpace() || !m_scrollingAlgorithm->dataFor(request.target) || !request.target->window() ||
        !request.target->window()->m_workspace || request.target->window()->m_workspace->m_monitor.lock())
        return FULLSCREEN_REQUEST_FAILED;

    const auto TARGET = request.target;
    const auto TDATA  = m_scrollingAlgorithm->dataFor(request.target);

    const auto WINDOW    = TARGET->window();
    const auto WORKSPACE = WINDOW->m_workspace;
    const auto MONITOR   = WORKSPACE->m_monitor.lock();

    // lambda for expelling if there is more than one target in a column when FSing a target.
    const auto expelIfMoreThanOneTargetInColDuringFS = [&](eFullscreenMode mode) -> void {
        const auto CURRENTCOL = TDATA->column.lock();

        // more that one target in column
        if (CURRENTCOL && CURRENTCOL->targetDatas.size() > 1) {
            const auto TDATA = m_scrollingAlgorithm->dataFor(TARGET);
            const auto currentIdx = m_scrollingAlgorithm->m_scrollingData->idx(CURRENTCOL);

            // acts like 'promote' layout dispatch
            m_scrollingAlgorithm->expelTarget(TDATA, CURRENTCOL, currentIdx == -1 ? std::nullopt : std::optional<int64_t>{currentIdx});
        }
    };

    /* Setting DSO and VRR */

    // If a window is being un-FSed, set its DSO and VRR in requestFullscreen()
    // If we are scrolling away from an FS window that is not unFSed yet, we set its DSO and VRR in sScrollingDataRecalculateHelper()
    // If a window is being FS-ed, or we are scrolling onto a FSed window, we set its DSO and VRR in sScrollingDataRecalculateHelper()

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

    const auto CURRENT_COL = TDATA->column.lock();

    // ERSTARR TODO much of the code below is repeated more then it has to; refactor

    if (request.mode == FSMODE_FULLSCREEN) {

        // if current target isn't fullscreen, save its column width
        if (!isFullscreen(TARGET, FSMODE_FULLSCREEN)) {

            float targetColumnWidth = 0.0F;

            // If the target was maximised, save the col width it had before being FSed at all
            if (isFullscreen(TARGET, FSMODE_MAXIMIZED))
                targetColumnWidth = getTargetColumnWidthBeforeFullscreenOrMaximise(TARGET);
            else {
                targetColumnWidth = CURRENT_COL ?
                    CURRENT_COL->getColumnWidth() :
                    0.5f; // 0.5f as the fallback - but it won't matter here since if current col doesn't exist here restoreColumnWidth will be = nullptr anyway
            }

            const auto ITR = m_fsTargets.find(TARGET);
            if (ITR != m_fsTargets.end()) {
                ITR->second.restoreColumnWidth = CURRENT_COL ? std::optional<float>{targetColumnWidth} : std::nullopt;
            } else {
                // setting the mode will be done later
                m_fsTargets.emplace(TARGET, SFullscreenScrollState{.restoreColumnWidth = CURRENT_COL ? std::optional<float>{targetColumnWidth} : std::nullopt});
            }
        }

        expelIfMoreThanOneTargetInColDuringFS(FSMODE_FULLSCREEN);

        const auto  CURRENTCOL        = TDATA->column.lock();
        if (!CURRENTCOL || CURRENTCOL->targetDatas.size() > 1)
            return FULLSCREEN_REQUEST_FAILED;

        CURRENTCOL->setColumnWidth(fullscreenColumnWidth());

        // move new column into view
        m_scrollingAlgorithm->m_scrollingData->centerOrFitCol(CURRENTCOL);

        // set internal fullscreen mode
        setTargetFullscreenModeInternal(TARGET, FSMODE_FULLSCREEN);

        // Hide all members below the FS target
        setNoMembersAboveFullscreen();

        return FULLSCREEN_REQUEST_LAYOUT_HANDLED;

    } else if (request.mode == FSMODE_MAXIMIZED) {

        if (!isFullscreen(TARGET, FSMODE_MAXIMIZED)) {

            float targetColumnWidth = 0.0F;

            // If the target was fullscreened, save the col width it had before being FSed at all
            if (isFullscreen(TARGET, FSMODE_FULLSCREEN))
                targetColumnWidth = getTargetColumnWidthBeforeFullscreenOrMaximise(TARGET);
            else
                targetColumnWidth = CURRENT_COL->getColumnWidth();

            const auto ITR = m_fsTargets.find(TARGET);
            if (ITR != m_fsTargets.end()) {
                ITR->second.restoreColumnWidth = CURRENT_COL ? std::optional<float>{targetColumnWidth} : std::nullopt;
            } else {
                // redundancy as it should already be saved as FSMODE_MAXIMIZED by now.
                m_fsTargets.emplace(TARGET, SFullscreenScrollState{.restoreColumnWidth = CURRENT_COL ? std::optional<float>{targetColumnWidth} : std::nullopt});
            }
        }

        expelIfMoreThanOneTargetInColDuringFS(FSMODE_MAXIMIZED);

        const auto  CURRENTCOL        = TDATA->column.lock();

        if (!CURRENTCOL || CURRENTCOL->targetDatas.size() > 1)
            return FULLSCREEN_REQUEST_FAILED;

        CURRENTCOL->setColumnWidth(1.F);
        // move new column into view
        m_scrollingAlgorithm->m_scrollingData->centerOrFitCol(CURRENTCOL);

        // set internal fullscreen mode
        setTargetFullscreenModeInternal(TARGET, FSMODE_MAXIMIZED);

        // Hide all members below the FS target
        setNoMembersAboveFullscreen();

        return FULLSCREEN_REQUEST_LAYOUT_HANDLED;
    }

    // UnFS target
    setTargetFullscreenModeInternal(TARGET, FSMODE_NONE);
    setNoMembersAboveFullscreen();
    // final check to see if the target was correctly FSed.
    return (request.mode == FSMODE_NONE && !isFullscreen(TARGET)) ? FULLSCREEN_REQUEST_LAYOUT_HANDLED : FULLSCREEN_REQUEST_FAILED;
}

void CScrollingFullscreenHandler::setTargetFullscreenModeInternal(const SP<Layout::ITarget> target, const eFullscreenMode mode) {

    const auto& ITR = m_fsTargets.find(target);

    if (mode == FSMODE_NONE) {
        if (ITR != m_fsTargets.end()) {
            ITR->second.mode.internal = FSMODE_NONE;
        }
    } else if (ITR == m_fsTargets.end()) {
        m_fsTargets.emplace(target, SFullscreenScrollState{.mode = {.internal = mode}});
    } else {
        ITR->second.mode.internal = mode;
    }

    syncFullscreenTargets();
}

void CScrollingFullscreenHandler::setTargetFullscreenModeClient(const SP<Layout::ITarget> target, const eFullscreenMode mode) {

    const auto& ITR = m_fsTargets.find(target);

    if (mode == FSMODE_NONE) {
        if (ITR != m_fsTargets.end()) {
            ITR->second.mode.client = FSMODE_NONE;
        }
    } else if (ITR == m_fsTargets.end()) {
        m_fsTargets.emplace(target, SFullscreenScrollState{.mode = {.client = mode}});
    } else {
        ITR->second.mode.client = mode;
    }

    syncFullscreenTargets();
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
            if (e && !isFullscreen(e->m_target) && !e->m_fadingOut && !e->m_pinned) {
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
    const auto LAYOUT_TILED_COVERING_FS_TARGET = getFullscreen(true);
    const auto LAYOUT_TILED_COVERING_FS_WINDOW = LAYOUT_TILED_COVERING_FS_TARGET && LAYOUT_TILED_COVERING_FS_TARGET->window() ? LAYOUT_TILED_COVERING_FS_TARGET->window() : nullptr;

    const auto LAST_SCROLL_HANDLED_TILED_FS_WINDOW        = m_fullscreenWindowHidingState.lastTiledLayoutManagedFsWindow.lock();
    const auto LAST_SCROLL_HANDLED_TILED_FS_WINDOW_FS_MODE = m_fullscreenWindowHidingState.lastTiledLayoutManagedFsWindowMode;

    // This should never happen
    if (!COVERING_FULLSCREEN_WINDOW && LAYOUT_TILED_COVERING_FS_WINDOW) {
        // This means that controller doesn't recognise tiled layout handled FS window as fullscreen.
        Log::logger->log(Log::ERR,
                         "Workspace doesn't recognise a tiled layout handled fullscreen/maximised window as such! This is a an error! We will attempt to recover by ignoring "
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
    if (g_pfullscreenController->getFullscreenHandlerName(COVERING_FULLSCREEN_WINDOW) != FULLSCREEN_HANDLER_SCROLLING) {
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

    // there is an FS window ontop of the layout handled tiled FS window
    // Case where COVERING_FULLSCREEN_WINDOW is handled above so this really should never happen. This is pure redundancy
    if (COVERING_FULLSCREEN_WINDOW != LAYOUT_TILED_COVERING_FS_WINDOW) {
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
            getFullscreenModes(LAYOUT_TILED_COVERING_FS_TARGET).internal != LAST_SCROLL_HANDLED_TILED_FS_WINDOW_FS_MODE) {
            // we are newly scrolling onto this tiled layout handled FS window, or we are changing from maximised to fullscreen or vice versa while in the same FS window
            // redundancy - make sure the list is empty
            clear_hiddenFloatingWindowsUnderFSWindow();
            // save all floating window current on screen, then hide all
            saveCurrentFsAndAllHiddenFloatingWindows(LAYOUT_TILED_COVERING_FS_WINDOW);
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

    Log::logger->log(Log::ERR,
                     "setNoMembersAboveFullscreen() failed to correctly execute. Current FS window: {} Current Tiled layout handled FS window: {} Current last focused tiled "
                     "layout handled FS window: {}",
                     COVERING_FULLSCREEN_WINDOW, LAYOUT_TILED_COVERING_FS_WINDOW, LAST_SCROLL_HANDLED_TILED_FS_WINDOW);
}

void CScrollingFullscreenHandler::syncFullscreenTargets() {

    // ERSTARR TODO - If we are to remove a target from the handler, DO IT VIA THE FUNCION! --> remves the target from the list after setting the col width to prev val is target still exists
    // target->column->targetDatas.size() > 1 --> if you're gettin the col data from a target, that takes care of is it == 0 case - if there's a case which it may not be implicitly checked need to handler that
        // actually probably se != 1 here

    // to prevent a rehash - just in case
    std::vector<std::pair<WP<Layout::ITarget>, SFullscreenMode>> toInsert;

    for (auto it = m_fsTargets.begin(); it != m_fsTargets.end(); ) {
        const auto TARGET = it->first.lock();
        // stale entry
        // TARGET doesn't have a window
        // TARGET does not have scrollingTargetData (all TARGETs in scrolling layout must)
        // TARGET is not FS at all
        // TARGET's space is not the same as current space
        if (!TARGET || !TARGET->window() || TARGET->space() != getSpace() || !m_scrollingAlgorithm->dataFor(TARGET)) {
            // simply erase from list. no need to re-set its prev col width as the TARGET is 'invalid'
            const auto NEXT = std::next(it);
            removeFsTarget(it->first.lock(), true);
            it = NEXT;
            continue;
        }

        // TARGET exists propely but no longer FS
        if ((!isFullscreen(TARGET) && getFullscreenModes(TARGET).client == FSMODE_NONE)) {
            const auto NEXT = std::next(it);
            // sets col width to prev value if present, then removes it from the handler (i.e. remove from list)
            removeFsTarget(TARGET, true);
            it = NEXT;
            continue;
        }

        // if internal FS mode set, set its col width accordingly
        if (getFullscreenModes(TARGET).internal != FSMODE_NONE) {
            m_scrollingAlgorithm->dataFor(TARGET)->column->setColumnWidth((getFullscreenModes(TARGET).internal == FSMODE_FULLSCREEN ? fullscreenColumnWidth() : 1.F));
            ++it;
            continue;
        }


        // If ITarget's underlying type is CWindowGroupTarget; only store the current window, NOT the whole group
        // This should never have happened to begin with // ERSTARR TODO - ADD A LOG FOR THIS AS IT SHOULDN'T HAVE HAPPENED - HERE AND IN DEFAULT HANDLER
        if (it->first->type() == Layout::TARGET_TYPE_GROUP) {
            const SFullscreenMode MODE = SFullscreenMode{.internal = it->second.mode.internal, .client = it->second.mode.client};
            const auto WINDOWTARGET = it->first->window()->layoutTarget();
            const auto NEXT = std::next(it);
            removeFsTarget(it->first.lock(), true);
            it = NEXT;
            toInsert.emplace_back(WINDOWTARGET,MODE);
            continue;
        }


        ++it;
    }


    for (const auto& e : toInsert) {
        m_fsTargets.emplace(e.first, e.second);
    }

}

void CScrollingFullscreenHandler::removeFsTarget(SP<Layout::ITarget> target, const bool recursionGuard) {

    // remove from the list, set the value it had to the window if that target still exists

    const auto ITR = m_fsTargets.find(target);

    // order of null checks is deliberate. checks for expired window in m_fsWindows too
    if (ITR == m_fsTargets.end()) {
        return;
    }

    if (!target) {
        m_fsTargets.erase(ITR);
        return;
    }


    if (ITR->second.restoreColumnWidth.has_value()) {
        const auto TDATA = m_scrollingAlgorithm->dataFor(target);
        if (TDATA && TDATA->column)
            TDATA->column->setColumnWidth(ITR->second.restoreColumnWidth.value());        
    }
    if (target->window())
        target->window()->m_layoutFlags.cantLockCursor = false;
    m_fsTargets.erase(ITR);

    if (!recursionGuard)
        syncFullscreenTargets();
}

eFullscreenHandler CScrollingFullscreenHandler::getFullscreenHandlerName() const {
    return FULLSCREEN_HANDLER_TYPE;
}


void CScrollingFullscreenHandler::sScrollingDataRecalculateHelper(const SP<Layout::Tiled::SScrollingTargetData> CURRENT_FS_TDATA, const PHLMONITOR MONITOR, const bool TARGET_WORKSPACE_HAS_FS) {

    /* Setting DSO and VRR */

    // If a window is being un-FSed, set its DSO and VRR in requestFullscreen()
    // If we are scrolling away from an FS window that is not unFSed yet, we set its DSO and VRR in sScrollingDataRecalculateHelper()
    // If a window is being FS-ed, or we are scrolling onto a FSed window, we set its DSO and VRR in sScrollingDataRecalculateHelper()

    static auto PDIRECTSCANOUT = CConfigValue<Config::INTEGER>("render:direct_scanout");

    // If we are scrolling away from a layout managed tiled FS window, send it a regular tranche
    // we need only check its internal state as if the window was saved in lastTiledLayoutManagedFsWindow, it is guaranteed to be as its name suggests
    if (const auto LAST_FS_LAYOUTMANAGED_TILED_WINDOW = m_fullscreenWindowHidingState.lastTiledLayoutManagedFsWindow.lock();
        // check if LAST_FS_LAYOUTMANAGED_TILED_WINDOW exists, and that we either don't have a covering FS window, or if we do; it's not the same as LAST_FS_LAYOUTMANAGED_TILED_WINDOW
        LAST_FS_LAYOUTMANAGED_TILED_WINDOW &&
        (!CURRENT_FS_TDATA || (CURRENT_FS_TDATA && LAST_FS_LAYOUTMANAGED_TILED_WINDOW != CURRENT_FS_TDATA->target->window()))
        // check that LAST_FS_LAYOUTMANAGED_TILED_WINDOW was not unfullscreened (CScrollingAlgorithm::requestFullscreen() would have handled that)
        && getFullscreenModes(LAST_FS_LAYOUTMANAGED_TILED_WINDOW->m_target).internal != FSMODE_NONE) {

        // send a regular tranche
        // ignore if DS is disabled.
        if ((*PDIRECTSCANOUT == 1 || (*PDIRECTSCANOUT == 2 && LAST_FS_LAYOUTMANAGED_TILED_WINDOW->getContentType() == NContentType::CONTENT_TYPE_GAME))) {
            auto surf = LAST_FS_LAYOUTMANAGED_TILED_WINDOW->getSolitaryResource();
            if (surf)
                g_pHyprRenderer->setSurfaceScanoutMode(surf, nullptr);
        }
    }

    // If we are scrolling onto, or have newly FSed a window
    if (CURRENT_FS_TDATA && CURRENT_FS_TDATA->target->window()) {
        const auto CURRENTLY_FS_WINDOW = CURRENT_FS_TDATA->target->window();
        // send a scanout tranche
        // ignore if DS is disabled.
        auto surf = CURRENTLY_FS_WINDOW->getSolitaryResource();
        if (surf)
            g_pHyprRenderer->setSurfaceScanoutMode(surf, MONITOR->m_self.lock());
    }

    Config::monitorRuleMgr()->ensureVRR(MONITOR);

    // DSO and VRR must be above setNoMembersAboveFullscreen() because we need the last tiled layout managed fullscreen window before it is reset when no fullscreen

    // if covering FS, set. If not, unset.
    setNoMembersAboveFullscreen();

    // Must be below setNoMembersAboveFullscreen() so it can properly set the windows' allowedOverFullscreen attributes
    updateFullscreenFade(TARGET_WORKSPACE_HAS_FS);

}



void CScrollingFullscreenHandler::saveCurrentFsAndAllHiddenFloatingWindows(PHLWINDOW fullscreenWindow) {

    // we save all the floating windows that will be hidden under the fullscreen. We are using the same logic that is used to judge which window is to be hidden + a float check.
    // This function must be updated whenever this logic is changed (setNoMembersAboveFullscreen())

    // fullscreenWindow is assumed to be tiled, layout handled, covers the whole monitor or work area.

    m_fullscreenWindowHidingState.lastTiledLayoutManagedFsWindow     = fullscreenWindow;
    m_fullscreenWindowHidingState.lastTiledLayoutManagedFsWindowMode = getFullscreenModes(fullscreenWindow->m_target).internal;

    const auto WORKSPACE = fullscreenWindow->m_workspace;

    for (auto const& w : WORKSPACE->getWindows()) {
        if (w != fullscreenWindow && !w->m_fadingOut && !w->m_pinned && w->m_isFloating)
            m_fullscreenWindowHidingState.hiddenFloatingWindowsUnderFSWindow.emplace(w);
    }
}



float CScrollingFullscreenHandler::fullscreenColumnWidth() const {
    if (!getSpace() || !getSpace()->workspace() || !getSpace()->workspace()->m_monitor || !m_scrollingAlgorithm->m_scrollingData ||
        !m_scrollingAlgorithm->m_scrollingData->controller)
        return 1.F;

    const auto   USABLE         = m_scrollingAlgorithm->usableArea();
    const auto   MONBOX         = getSpace()->workspace()->m_monitor->logicalBox();
    const bool   PRIMARY_HORIZ  = m_scrollingAlgorithm->m_scrollingData->controller->isPrimaryHorizontal();
    const double usablePrimary  = PRIMARY_HORIZ ? USABLE.w : USABLE.h;
    const double monitorPrimary = PRIMARY_HORIZ ? MONBOX.w : MONBOX.h;

    if (usablePrimary <= 0.0)
        return 1.F;

    return std::max(1.F, sc<float>(monitorPrimary / usablePrimary));
}

bool CScrollingFullscreenHandler::columnCoversMonitor(SP<Layout::Tiled::SColumnData> col) const {
    
    const auto SCROLLINGDATA = m_scrollingAlgorithm->m_scrollingData;

    if (!col || !SCROLLINGDATA || !SCROLLINGDATA->controller || !getSpace() || !getSpace()->workspace() || !getSpace()->workspace()->m_monitor)
        return false;

    const int64_t COL_IDX = SCROLLINGDATA->idx(col);
    if (COL_IDX < 0)
        return false;

    static const auto PFSONONE = CConfigValue<Config::INTEGER>("scrolling:fullscreen_on_one_column");

    const auto        USABLE        = m_scrollingAlgorithm->usableArea();
    const bool        PRIMARY_HORIZ = SCROLLINGDATA->controller->isPrimaryHorizontal();
    const double      VIEW_SIZE     = PRIMARY_HORIZ ? USABLE.w : USABLE.h;
    const double      VIEW_START    = SCROLLINGDATA->controller->getOffset();
    const double      VIEW_END      = VIEW_START + VIEW_SIZE;
    const double      COL_START     = SCROLLINGDATA->controller->calculateStripStart(COL_IDX, USABLE, *PFSONONE);
    const double      COL_END       = COL_START + SCROLLINGDATA->controller->calculateStripSize(COL_IDX, USABLE, *PFSONONE);

    return COL_START <= VIEW_START + 1.0 && COL_END >= VIEW_END - 1.0;
}

bool CScrollingFullscreenHandler::columnCoversWorkArea(SP<Layout::Tiled::SColumnData> col) const {
    // Covers Monitor check also works for maximised windows in scrolling layout.
    return columnCoversMonitor(col);

}

void  CScrollingFullscreenHandler::updateFullscreenFade(bool coversMonitor) {

    if (!coversMonitor) {
        // prevent stuck focus
        g_pInputManager->unconstrainMouse();
        for (const auto& fs : m_fsTargets) {
            if (!fs.first)
                continue;
            if (fs.first->window())
                fs.first->window()->m_layoutFlags.cantLockCursor = true;
        }
    } else {
        for (const auto& fs : m_fsTargets) {
            if (!fs.first)
                continue;

            if (fs.first->window())
                fs.first->window()->m_layoutFlags.cantLockCursor = false;
        }
    }

    if (!getSpace() || !getSpace()->workspace())
        return;

    g_pDesktopAnimationManager->setFullscreenFadeAnimation(getSpace()->workspace(),
                                                           coversMonitor ? CDesktopAnimationManager::ANIMATION_TYPE_IN : CDesktopAnimationManager::ANIMATION_TYPE_OUT);
}

float CScrollingFullscreenHandler::getTargetColumnWidthBeforeFullscreenOrMaximise(const SP<Layout::ITarget> target) {
    // fallback to col width of 0.5F

    if (!target || !isFullscreen(target))
        return 0.5F;

    const auto WINITR = m_fsTargets.find(target);

    if (WINITR == m_fsTargets.end())
        return 0.5F;

    return WINITR->second.restoreColumnWidth.value_or(0.5F);
}
