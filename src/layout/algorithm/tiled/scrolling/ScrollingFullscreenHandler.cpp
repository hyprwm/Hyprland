#include "../../../../config/shared/monitor/MonitorRuleManager.hpp"
#include "../../../../render/Renderer.hpp"

#include "../../../../animation/WorkspaceAnimationController.hpp"

#include "../../../../desktop/state/LayerState.hpp"
#include "../../../../desktop/state/WindowState.hpp"

#include "../../../../managers/fullscreen/FullscreenController.hpp"
#include "../../../../managers/fullscreen/handler/FullscreenHandler.hpp"

#include "../../../../layout/algorithm/tiled/scrolling/ScrollingFullscreenHandler.hpp"
#include "../../../../layout/algorithm/tiled/scrolling/ScrollingAlgorithm.hpp"
#include "../../../../layout/target/WindowGroupTarget.hpp"
#include "../../../../config/supplementary/propRefresher/PropRefresher.hpp"
#include <optional>

using namespace Fullscreen;
using namespace Fullscreen::ScrollingFullscreenHandler;

CScrollingFullscreenHandler::CScrollingFullscreenHandler(Layout::Tiled::CScrollingAlgorithm* const algorithm) : IFullscreenHandler(algorithm), m_scrollingAlgorithm(algorithm) {
    if (!m_scrollingAlgorithm)
        Log::logger->log(Log::CRIT, "CScrollingFullscreenHandler failed during construction: Owning layout algorithm does not exist!");
}

CScrollingFullscreenHandler::~CScrollingFullscreenHandler() {

    for (auto it = m_fsTargets.begin(); it != m_fsTargets.end();) {
        const auto NEXT = std::next(it); // save next before removeFsTarget invalidates it
        removeFsTarget(it->first.lock());
        it = NEXT;
    }
    updateFullscreenFade(false);
}

bool CScrollingFullscreenHandler::isFullscreen(SP<Layout::ITarget> target, const std::optional<eFullscreenMode> mode, const std::optional<bool> covering) {
    // Mode checking logic is the same as getFullscreenModes() - keep it in sync

    if (mode.value_or(FSMODE_FULLSCREEN) == FSMODE_NONE) {
        Log::logger->log(Log::ERR, "Passed mode = FSMODE_NONE into isFullscreen(). Negating the result instead");
        !isFullscreen(target, std::nullopt, covering);
    }

    if (!target)
        return false;

    // A window group's FS modes are considered to be owned by its current window
    if (const auto WINDOW_GROUP_TARGET = dc<Layout::CWindowGroupTarget*>(target.get()); WINDOW_GROUP_TARGET && target->type() == Layout::TARGET_TYPE_GROUP) {
        if (WINDOW_GROUP_TARGET->getGroup() && WINDOW_GROUP_TARGET->getGroup()->current() && WINDOW_GROUP_TARGET->getGroup()->current()->m_target)
            target = WINDOW_GROUP_TARGET->getGroup()->current()->m_target;
        else
            return false;
    }

    const auto ITR = m_fsTargets.find(target);

    // Check that the target fits the scrolling definition of FS target

    if (ITR == m_fsTargets.end() || !ITR->first || ITR->second.mode.internal == FSMODE_NONE)
        return false;

    const auto TDATA = m_scrollingAlgorithm->dataFor(target, true);
    if (!TDATA || !TDATA->column)
        return false;

    if (TDATA->column->targetDatas.size() != 1) {
        Log::logger->log(Log::DEBUG, "column->targetDatas != 1 in a column with FS target");
        return false;
    }

    if (!covering.has_value())
        return mode.has_value() ? ITR->second.mode.internal == mode.value() : true;

    const auto FSMODE = ITR->second.mode.internal;
    if (mode.has_value() && mode.value() != FSMODE)
        return false;

    return FSMODE == FSMODE_FULLSCREEN ? (covering.value() ? columnCoversMonitor(TDATA->column.lock()) : !columnCoversMonitor(TDATA->column.lock())) :
                                         (covering.value() ? columnCoversWorkArea(TDATA->column.lock()) : !columnCoversWorkArea(TDATA->column.lock()));
}

bool CScrollingFullscreenHandler::hasFullscreen(const std::optional<bool> covering) {
    return std::ranges::any_of(m_fsTargets, [&](const auto& e) { return isFullscreen(e.first.lock(), std::nullopt, covering); });
}

SP<Layout::ITarget> CScrollingFullscreenHandler::getFullscreen(const std::optional<bool> covering) {
    for (const auto& e : m_fsTargets) {
        if (const auto TARGET = e.first.lock(); isFullscreen(TARGET, std::nullopt, covering))
            return TARGET;
    }
    return nullptr;
}

SFullscreenMode CScrollingFullscreenHandler::getFullscreenModes(SP<Layout::ITarget> target) {
    if (!target)
        return {};

    if (const auto WINDOW_GROUP_TARGET = dc<Layout::CWindowGroupTarget*>(target.get()); WINDOW_GROUP_TARGET && target->type() == Layout::TARGET_TYPE_GROUP) {
        if (WINDOW_GROUP_TARGET->getGroup() && WINDOW_GROUP_TARGET->getGroup()->current() && WINDOW_GROUP_TARGET->getGroup()->current()->m_target)
            target = WINDOW_GROUP_TARGET->getGroup()->current()->m_target;
        else
            return {};
    }

    const auto ITR = m_fsTargets.find(target);

    if (ITR == m_fsTargets.end())
        return {};
    return ITR->second.mode;
}

eFullscreenRequestResult CScrollingFullscreenHandler::requestFullscreen(const SFullscreenRequest& request) {

    /*
        Setting global position is done in SScrollingData::recalculate()
    */

    if (!request.target || !getSpace() || request.target->space() != getSpace() || !m_scrollingAlgorithm->dataFor(request.target, true) || !request.target->window() ||
        !request.target->window()->m_workspace || !request.target->window()->m_workspace->m_monitor)
        return FULLSCREEN_REQUEST_FAILED;

    const auto TARGET = request.target;
    const auto TDATA  = m_scrollingAlgorithm->dataFor(request.target, true);

    const auto WINDOW    = TARGET->window();
    const auto WORKSPACE = WINDOW->m_workspace;
    const auto MONITOR   = WORKSPACE->m_monitor.lock();

    const auto REQUESTED_MODE = request.mode;

    // lambda for expelling if there is more than one target in a column when FSing a target.
    const auto expelIfMoreThanOneTargetInColDuringFS = [&]() -> void {
        const auto CURRENTCOL = TDATA->column.lock();

        if (CURRENTCOL && CURRENTCOL->targetDatas.size() > 1) {
            const auto TDATA      = m_scrollingAlgorithm->dataFor(TARGET, true);
            const auto currentIdx = m_scrollingAlgorithm->m_scrollingData->idx(CURRENTCOL);

            // acts like 'promote' layout dispatch
            m_scrollingAlgorithm->expelTarget(TDATA, CURRENTCOL, currentIdx == -1 ? std::nullopt : std::optional<int64_t>{currentIdx});
        }
    };

    /* Setting DS and VRR */

    // If a window is being un-FSed, set its DS and VRR in requestFullscreen()
    // If we are scrolling away from an FS window that is not unFSed yet, we set its DS and VRR in sScrollingDataRecalculateHelper()
    // If a window is being FS-ed, or we are scrolling onto a FSed window, we set its DS and VRR in sScrollingDataRecalculateHelper()

    if (REQUESTED_MODE == FSMODE_NONE) {

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

    if (REQUESTED_MODE == FSMODE_FULLSCREEN) {

        if (!isFullscreen(TARGET, FSMODE_FULLSCREEN)) {

            float targetColumnWidth = 0.0F;

            if (isFullscreen(TARGET, FSMODE_MAXIMIZED, std::nullopt))
                targetColumnWidth = getTargetColumnWidthBeforeFullscreenOrMaximise(TARGET);
            else {
                targetColumnWidth = CURRENT_COL ?
                    // 0.5f as the fallback - but it won't matter here since if current col doesn't exist here restoreColumnWidth will be = nullptr anyway
                    CURRENT_COL->getColumnWidth() :
                    0.5f;
            }

            const auto ITR = m_fsTargets.find(TARGET);
            if (ITR != m_fsTargets.end())
                ITR->second.restoreColumnWidth = CURRENT_COL ? std::optional<float>{targetColumnWidth} : std::nullopt;
            else
                // setting the mode will be done later
                m_fsTargets.emplace(TARGET, SFullscreenScrollState{.restoreColumnWidth = CURRENT_COL ? std::optional<float>{targetColumnWidth} : std::nullopt});
        }

        expelIfMoreThanOneTargetInColDuringFS();

        const auto CURRENTCOL = TDATA->column.lock();
        if (!CURRENTCOL || CURRENTCOL->targetDatas.size() > 1)
            return FULLSCREEN_REQUEST_FAILED;

        CURRENTCOL->setColumnWidth(fullscreenColumnWidth());

        m_scrollingAlgorithm->m_scrollingData->centerOrFitCol(CURRENTCOL);

        setTargetFullscreenModeInternal(TARGET, FSMODE_FULLSCREEN);

        setNoMembersAboveFullscreen();

        return FULLSCREEN_REQUEST_LAYOUT_HANDLED;

    } else if (REQUESTED_MODE == FSMODE_MAXIMIZED) {

        if (!isFullscreen(TARGET, FSMODE_MAXIMIZED)) {

            float targetColumnWidth = 0.0F;

            if (isFullscreen(TARGET, FSMODE_FULLSCREEN, std::nullopt))
                targetColumnWidth = getTargetColumnWidthBeforeFullscreenOrMaximise(TARGET);
            else
                targetColumnWidth = CURRENT_COL->getColumnWidth();

            const auto ITR = m_fsTargets.find(TARGET);
            if (ITR != m_fsTargets.end())
                ITR->second.restoreColumnWidth = CURRENT_COL ? std::optional<float>{targetColumnWidth} : std::nullopt;
            else
                m_fsTargets.emplace(TARGET, SFullscreenScrollState{.restoreColumnWidth = CURRENT_COL ? std::optional<float>{targetColumnWidth} : std::nullopt});
        }

        expelIfMoreThanOneTargetInColDuringFS();

        const auto CURRENTCOL = TDATA->column.lock();

        if (!CURRENTCOL || CURRENTCOL->targetDatas.size() > 1)
            return FULLSCREEN_REQUEST_FAILED;

        CURRENTCOL->setColumnWidth(1.F);

        m_scrollingAlgorithm->m_scrollingData->centerOrFitCol(CURRENTCOL);

        setTargetFullscreenModeInternal(TARGET, FSMODE_MAXIMIZED);

        setNoMembersAboveFullscreen();

        return FULLSCREEN_REQUEST_LAYOUT_HANDLED;
    }

    // Because we use MONBOX for a fullscreen window, we need to offset the fact that it's larger than a 1.F column and thus leave the viewport a few pixels to the right when unFullscreened
    if (request.currentMode == FSMODE_FULLSCREEN)
        m_scrollingAlgorithm->m_scrollingData->controller->adjustOffset(MONITOR->logicalBox().x - WORKSPACE->m_space->workArea().x);

    // UnFS target
    setTargetFullscreenModeInternal(TARGET, FSMODE_NONE);
    setNoMembersAboveFullscreen();
    return (REQUESTED_MODE == FSMODE_NONE && !isFullscreen(TARGET)) ? FULLSCREEN_REQUEST_LAYOUT_HANDLED : FULLSCREEN_REQUEST_FAILED;
}

void CScrollingFullscreenHandler::setTargetFullscreenModeInternal(const SP<Layout::ITarget> target, const eFullscreenMode mode) {

    const auto& ITR = m_fsTargets.find(target);

    if (mode == FSMODE_NONE) {
        if (ITR != m_fsTargets.end())
            ITR->second.mode.internal = FSMODE_NONE;
    } else if (ITR == m_fsTargets.end())
        m_fsTargets.emplace(target, SFullscreenScrollState{.mode = {.internal = mode}});
    else
        ITR->second.mode.internal = mode;

    syncFullscreenTargets();
}

void CScrollingFullscreenHandler::setTargetFullscreenModeClient(const SP<Layout::ITarget> target, const eFullscreenMode mode) {

    const auto& ITR = m_fsTargets.find(target);

    if (mode == FSMODE_NONE) {
        if (ITR != m_fsTargets.end())
            ITR->second.mode.client = FSMODE_NONE;
    } else if (ITR == m_fsTargets.end())
        m_fsTargets.emplace(target, SFullscreenScrollState{.mode = {.client = mode}});
    else
        ITR->second.mode.client = mode;

    syncFullscreenTargets();
}

void CScrollingFullscreenHandler::updateTargetRulesAndDecos(const SP<Layout::ITarget> target) {
    if (!target || !target->window() || !target->workspace() || !target->workspace()->m_monitor)
        return;

    const auto MONITOR = target->workspace()->m_monitor.lock();
    const auto WINDOW  = target->window();
    
    // If window is in a group, we need to update these values for ALL members of the group.
    if (WINDOW->m_group) {
        for (const auto& gm : WINDOW->m_group->windows()) {
            gm->m_ruleApplicator->propertiesChanged(Desktop::Rule::RULE_PROP_FULLSCREEN | Desktop::Rule::RULE_PROP_FULLSCREENSTATE_CLIENT |
                                                        Desktop::Rule::RULE_PROP_FULLSCREENSTATE_INTERNAL | Desktop::Rule::RULE_PROP_ON_WORKSPACE);
            gm->updateDecorationValues();
        }
    }
    else {
        WINDOW->m_ruleApplicator->propertiesChanged(Desktop::Rule::RULE_PROP_FULLSCREEN | Desktop::Rule::RULE_PROP_FULLSCREENSTATE_CLIENT |
                                                    Desktop::Rule::RULE_PROP_FULLSCREENSTATE_INTERNAL | Desktop::Rule::RULE_PROP_ON_WORKSPACE);
        WINDOW->updateDecorationValues();
    }

    // Normally, FS controller's FS state setter's method of handling window rules should be used; but calling g_layoutManager->recalculateMonitor(MONITOR) and getSpace()->recalculate()
    // here would lead to an inf recursion
    // Concern: if the user executes a premature prop refresh, this might cause another prop refresh to be enqueued if the variables in the if cond aren't properly updated by setNoMembersAboveFullscreen()
    Config::Supplementary::refresher()->scheduleRefresh(Config::Supplementary::REFRESH_WINDOW_STATES | Config::Supplementary::REFRESH_MONITOR_STATES);
}

void CScrollingFullscreenHandler::setTargetSizeAndPosition(const SP<Layout::ITarget> target) {
    // We don't need to do anything explicitly here because in scrolling, pos/size setting as well as managing window/workspace rules are done in scrolling's recalculate()
    ;
}

void CScrollingFullscreenHandler::syncTargetSizeAndPosition() {
    // We don't need anything here as scrolling handled pos setting in its recalculate() anyway
    ;
}

void CScrollingFullscreenHandler::setNoMembersAboveFullscreen() {
    if (!m_scrollingAlgorithm->m_parent || !getSpace() || !getSpace()->workspace() || !getSpace()->workspace()->m_monitor)
        return;

    const auto WORKSPACE = getSpace()->workspace();
    const auto MONITOR   = WORKSPACE->m_monitor;

    // This should be in sync with default FS handling of setting all members below FS (IFullscreenHandler::setNoMembersAboveFullscreen())
    // For simply setting or unsetting no members above the FS window without scrolling specific logic
    const auto setNoMembersAboveFS_layoutUnaware = [&](const bool SET) {
        if (!getSpace() || !getSpace()->workspace() || !getSpace()->workspace()->m_monitor)
            return;

        const auto SPACE     = getSpace();
        const auto WORKSPACE = SPACE->workspace();
        const auto MONITOR   = WORKSPACE->m_monitor;

        for (auto const& w : Desktop::windowState()->windows()) {
            if (w && w->m_workspace == getSpace()->workspace() && !isFullscreen(w->m_target) && !w->m_pinned) {
                w->m_allowedOverFullscreen = !SET;
                w->updateFullscreenInputState();
            }
        }
        for (auto const& ls : Desktop::layerState()->layers()) {
            if (ls->m_monitor == MONITOR)
                ls->m_aboveFullscreen = !SET;
        }
    };

    const auto clear_hiddenFloatingWindowsUnderFSWindow = [&]() {
        m_fullscreenWindowHidingState.hiddenFloatingWindowsUnderFSWindow.clear();
        if (!m_fullscreenWindowHidingState.hiddenFloatingWindowsUnderFSWindow.empty())
            Log::logger->log(
                Log::WARN,
                "hiddenFloatingWindowsUnderFSWindow.clear() failed. Will likely cause the mishandling of floating window hiding upon fullscreening on scrolling layout. This is an "
                "error but is not critical.");
    };

    /*
    In scrolling layout, a fully in view tiled FS window may exist underneath a fullscreen floating window. We must keep the floating windows that were opened ontop of the tiled FS window, as well as those
    ontop of the floating FS windows that were layered ontop of the tiled FS window.
    */

    const auto COVERING_FULLSCREEN_WINDOW      = Fullscreen::controller()->getFullscreenWindow(WORKSPACE, true);
    const auto LAYOUT_TILED_COVERING_FS_TARGET = getFullscreen(true);
    const auto LAYOUT_TILED_COVERING_FS_WINDOW = LAYOUT_TILED_COVERING_FS_TARGET && LAYOUT_TILED_COVERING_FS_TARGET->window() ? LAYOUT_TILED_COVERING_FS_TARGET->window() : nullptr;

    const auto LAST_SCROLL_HANDLED_TILED_FS_WINDOW         = m_fullscreenWindowHidingState.lastTiledLayoutManagedFsWindow.lock();
    const auto LAST_SCROLL_HANDLED_TILED_FS_WINDOW_FS_MODE = m_fullscreenWindowHidingState.lastTiledLayoutManagedFsWindowMode;

    if (!COVERING_FULLSCREEN_WINDOW && LAYOUT_TILED_COVERING_FS_WINDOW) {
        // This means that controller doesn't recognise tiled layout handled FS window as fullscreen.
        Log::logger->log(Log::WARN,
                         "Workspace doesn't recognise a tiled layout handled fullscreen/maximised window as such! This is a an error! We will attempt to recover by ignoring "
                         "the request to setNoMembersAboveFullscreen");
        return;
    }

    // There is no FS window; tiled or floating
    if (!COVERING_FULLSCREEN_WINDOW) {
        m_fullscreenWindowHidingState.lastTiledLayoutManagedFsWindow = nullptr;
        clear_hiddenFloatingWindowsUnderFSWindow();
        setNoMembersAboveFS_layoutUnaware(false);
        return;
    }

    if (Fullscreen::controller()->getFullscreenHandlerName(COVERING_FULLSCREEN_WINDOW) != FULLSCREEN_HANDLER_SCROLLING) {
        // TODO - this will fire when default handling FS windows in scrolling workspace. When FS related logic is properly extracted from recalculate(), this will no longer fire when no errors occur, and can be turned into a WARN
        Log::logger->log(Log::DEBUG, "Default handled FS window called CScrollingFullscreenHandler::setNoMembersAboveFullscreen(). Recovering...");

        setNoMembersAboveFS_layoutUnaware(true);
        return;
    }

    // There's only a floating FS window
    if (COVERING_FULLSCREEN_WINDOW && !LAYOUT_TILED_COVERING_FS_WINDOW) {
        Log::logger->log(Log::WARN,
                         "non-scroll-handled FS window called CScrollingFullscreenHandler::setNoMembersAboveFullscreen(). This should never happen: setNoMembersAboveFullscreen() "
                         "call should have been dispatched to default FS handler. This is a bug, but is not fatal. Recovering...");

        clear_hiddenFloatingWindowsUnderFSWindow();
        setNoMembersAboveFS_layoutUnaware(true);
        return;
    }

    // There is an FS window ontop of the layout handled tiled FS window
    if (COVERING_FULLSCREEN_WINDOW != LAYOUT_TILED_COVERING_FS_WINDOW) {
        Log::logger->log(Log::WARN,
                         "non-scroll-handled FS window called CScrollingFullscreenHandler::setNoMembersAboveFullscreen(). This should never happen: setNoMembersAboveFullscreen() "
                         "call should have been dispatched to default FS handler. This is a bug, but is not fatal. Recovering...");
        setNoMembersAboveFS_layoutUnaware(true);
        return;
    }

    // layout handled tiled FS window is the only covering FS window
    if (COVERING_FULLSCREEN_WINDOW == LAYOUT_TILED_COVERING_FS_WINDOW) {
        // we are newly scrolling onto this tiled layout handled FS window, or we are changing from maximised to fullscreen or vice versa while in the same FS window
        if (!LAST_SCROLL_HANDLED_TILED_FS_WINDOW || LAST_SCROLL_HANDLED_TILED_FS_WINDOW != LAYOUT_TILED_COVERING_FS_WINDOW ||
            getFullscreenModes(LAYOUT_TILED_COVERING_FS_TARGET).internal != LAST_SCROLL_HANDLED_TILED_FS_WINDOW_FS_MODE) {
            clear_hiddenFloatingWindowsUnderFSWindow();
            saveCurrentFsAndAllHiddenFloatingWindows(LAYOUT_TILED_COVERING_FS_WINDOW);
            setNoMembersAboveFS_layoutUnaware(true);
            return;
        } else {

            for (auto const& w : Desktop::windowState()->windows()) {
                if (!w || w->m_workspace != getSpace()->workspace())
                    continue;

                if (w != COVERING_FULLSCREEN_WINDOW && !w->m_pinned) {
                    // If it the window was hidden when the UNDERLYING_FS_WINDOW was FSed, or it is a tiled window; it remains hidden. else, it is allowed ontop of it
                    w->m_allowedOverFullscreen = w->m_isFloating && !m_fullscreenWindowHidingState.hiddenFloatingWindowsUnderFSWindow.contains(w);
                    w->updateFullscreenInputState();
                }
            }
            for (auto const& ls : Desktop::layerState()->layers()) {
                if (ls->m_monitor == MONITOR)
                    ls->m_aboveFullscreen = false;
            }

            return;
        }
    }

    if (!LAST_SCROLL_HANDLED_TILED_FS_WINDOW) {
        if (LAYOUT_TILED_COVERING_FS_WINDOW)
            m_fullscreenWindowHidingState.lastTiledLayoutManagedFsWindow = LAYOUT_TILED_COVERING_FS_WINDOW;
        else
            m_fullscreenWindowHidingState.lastTiledLayoutManagedFsWindow = nullptr;

        clear_hiddenFloatingWindowsUnderFSWindow();
        return;
    }

    Log::logger->log(Log::ERR,
                     "setNoMembersAboveFullscreen() failed to correctly execute. Current FS window: {} Current Tiled layout handled FS window: {} Current last focused tiled "
                     "layout handled FS window: {}",
                     COVERING_FULLSCREEN_WINDOW, LAYOUT_TILED_COVERING_FS_WINDOW, LAST_SCROLL_HANDLED_TILED_FS_WINDOW);
}

void CScrollingFullscreenHandler::syncFullscreenTargets() {

    // to prevent a rehash
    std::vector<std::pair<WP<Layout::ITarget>, SFullscreenMode>> toInsert;

    for (auto it = m_fsTargets.begin(); it != m_fsTargets.end();) {

        // Somehow happens sometimes and causes WP<> to segfault
        if (m_fsTargets.empty())
            return;

        // Rigorously check if WP<> is valid as WP<> randomly segfaults sometimes without this
        const auto TARGET = !it->first.expired() && it->first.valid() && it->first ? it->first.lock() : nullptr;

        if (!TARGET || !TARGET->window() || TARGET->space() != getSpace() || !m_scrollingAlgorithm->dataFor(TARGET, true)) {
            // simply erase from list. no need to re-set its prev col width as the TARGET is 'invalid'
            const auto NEXT = std::next(it);
            removeFsTarget(TARGET, true);
            it = NEXT;
            continue;
        }

        if ((!isFullscreen(TARGET) && getFullscreenModes(TARGET).client == FSMODE_NONE)) {
            const auto NEXT = std::next(it);
            removeFsTarget(TARGET, true);
            it = NEXT;
            continue;
        }

        // This might happen if the target was moved to another col, so we need to unFS it properly
        if (const auto TARGET_WINDOW = TARGET->window(); TARGET_WINDOW && m_scrollingAlgorithm->dataFor(TARGET, true)->column) {
            const auto STDATA = m_scrollingAlgorithm->dataFor(TARGET, true);
            if (STDATA) {
                const auto COL_DATA = m_scrollingAlgorithm->dataFor(TARGET, true)->column;
                if (COL_DATA && getFullscreenModes(TARGET).internal != FSMODE_NONE && COL_DATA->targetDatas.size() != 1)
                    controller()->setFullscreenMode(TARGET_WINDOW, FSMODE_NONE, std::nullopt, true);
            }
        }

        // If ITarget's underlying type is CWindowGroupTarget; only store the current window, NOT the whole group
        if (TARGET->type() == Layout::TARGET_TYPE_GROUP || (TARGET->window()->m_group && TARGET->window()->m_group->current()->m_target != TARGET)) {
            Log::logger->log(Log::WARN, "Handler tracked a window group. This should have never happened. Recovering...");

            const auto TARGET_FS_MODES = getFullscreenModes(it->first.lock());
            const auto WINDOWTARGET    = TARGET->window()->m_target;
            const auto NEXT            = std::next(it);
            removeFsTarget(TARGET, true);
            it = NEXT;
            if (WINDOWTARGET)
                toInsert.emplace_back(WINDOWTARGET, TARGET_FS_MODES);
            continue;
        }

        if (getFullscreenModes(TARGET).internal != FSMODE_NONE) {
            m_scrollingAlgorithm->dataFor(TARGET, true)->column->setColumnWidth((getFullscreenModes(TARGET).internal == FSMODE_FULLSCREEN ? fullscreenColumnWidth() : 1.F));
            ++it;
            continue;
        }

        ++it;
    }

    for (const auto& e : toInsert) {
        m_fsTargets.emplace(e.first, e.second);
    }
}

void CScrollingFullscreenHandler::removeFsTarget(SP<Layout::ITarget> target, const bool recursionGuard) {

    const auto ITR = m_fsTargets.find(target);

    // order of checks is deliberate. checks for expired window in m_fsWindows too
    if (ITR == m_fsTargets.end())
        return;

    if (!target) {
        m_fsTargets.erase(ITR);
        return;
    }

    if (ITR->second.restoreColumnWidth.has_value()) {
        const auto TDATA = m_scrollingAlgorithm->dataFor(target, true);
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

void CScrollingFullscreenHandler::sScrollingDataRecalculateHelper(const SP<Layout::Tiled::SScrollingTargetData> CURRENT_FS_TDATA, const PHLMONITOR MONITOR,
                                                                  const bool TARGET_WORKSPACE_HAS_FS) {
    // TODO Decouple FS logic from SScrollingData::recalculate() to avoid having to schedule a prop refresh: it has to be here and it's a mess because recalculate() handled scrolling
    // onto/away from FS windows and this process doesn't call the controller's FS setters which are normally responsible for handling window rule checks.
    
    // Scrolling away from an FS window
    if (m_fullscreenWindowHidingState.lastTiledLayoutManagedFsWindow && !TARGET_WORKSPACE_HAS_FS) {
        const auto LAST_FS_TARGET = m_fullscreenWindowHidingState.lastTiledLayoutManagedFsWindow->m_target;
        updateTargetRulesAndDecos(LAST_FS_TARGET);
    }

    /* Setting DS and VRR */

    // If a window is being un-FSed, set its DS and VRR in requestFullscreen()
    // If we are scrolling away from an FS window that is not unFSed yet, we set its DS and VRR in sScrollingDataRecalculateHelper()
    // If a window is being FS-ed, or we are scrolling onto a FSed window, we set its DS and VRR in sScrollingDataRecalculateHelper()

    static auto PDIRECTSCANOUT = CConfigValue<Config::INTEGER>("render:direct_scanout");

    // If we are scrolling away from a layout managed tiled FS window, send it a regular tranche
    // we need only check its internal state as if the window was saved in lastTiledLayoutManagedFsWindow, it is guaranteed to be as its name suggests
    if (const auto LAST_FS_LAYOUTMANAGED_TILED_WINDOW = m_fullscreenWindowHidingState.lastTiledLayoutManagedFsWindow.lock();
        // check if LAST_FS_LAYOUTMANAGED_TILED_WINDOW exists, and that we either don't have a covering FS window, or if we do; it's not the same as LAST_FS_LAYOUTMANAGED_TILED_WINDOW
        LAST_FS_LAYOUTMANAGED_TILED_WINDOW &&
        (!CURRENT_FS_TDATA || (CURRENT_FS_TDATA && LAST_FS_LAYOUTMANAGED_TILED_WINDOW != CURRENT_FS_TDATA->target->window()))
        // check that LAST_FS_LAYOUTMANAGED_TILED_WINDOW was not unfullscreened (requestFullscreen() would have handled setting its DSO/VRR)
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
        const auto  CURRENTLY_FS_WINDOW = CURRENT_FS_TDATA->target->window();

        static auto PDIRECTSCANOUT = CConfigValue<Config::INTEGER>("render:direct_scanout");
        if (*PDIRECTSCANOUT == 1 || (*PDIRECTSCANOUT == 2 && CURRENTLY_FS_WINDOW->getContentType() == NContentType::CONTENT_TYPE_GAME)) {
            // send a scanout tranche
            // ignore if DS is disabled.
            auto surf = CURRENTLY_FS_WINDOW->getSolitaryResource();
            if (surf)
                g_pHyprRenderer->setSurfaceScanoutMode(surf, MONITOR->m_self.lock());
        }
    }

    Config::monitorRuleMgr()->ensureVRR(MONITOR);

    // DS and VRR setting must run before setNoMembersAboveFullscreen() because we need the last tiled layout managed fullscreen window before it is reset when no fullscreen
    setNoMembersAboveFullscreen();

    // Must run after setNoMembersAboveFullscreen() so it can properly set the windows' allowedOverFullscreen attributes
    updateFullscreenFade(TARGET_WORKSPACE_HAS_FS);
}

void CScrollingFullscreenHandler::saveCurrentFsAndAllHiddenFloatingWindows(PHLWINDOW fullscreenWindow) {
    // We are using the same logic as setNoMembersAboveFullscreen() + a float check.
    // keep logic here in sync with setNoMembersAboveFullscreen()

    m_fullscreenWindowHidingState.lastTiledLayoutManagedFsWindow     = fullscreenWindow;
    m_fullscreenWindowHidingState.lastTiledLayoutManagedFsWindowMode = getFullscreenModes(fullscreenWindow->m_target).internal;

    const auto WORKSPACE = fullscreenWindow->m_workspace;

    for (auto const& w : Desktop::windowState()->windows()) {

        if (!w || w->m_workspace != getSpace()->workspace())
            continue;

        if (w != fullscreenWindow && !w->m_pinned && w->m_isFloating)
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

void CScrollingFullscreenHandler::updateFullscreenFade(bool coversMonitor) {

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

    Animation::Workspace::setFullscreenFadeAnimation(getSpace()->workspace(), coversMonitor ? Animation::Workspace::ANIMATION_TYPE_IN : Animation::Workspace::ANIMATION_TYPE_OUT);
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
