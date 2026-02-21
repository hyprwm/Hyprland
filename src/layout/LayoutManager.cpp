#include "LayoutManager.hpp"

#include "space/Space.hpp"
#include "target/Target.hpp"

#include "../config/ConfigManager.hpp"
#include "../Compositor.hpp"
#include "../managers/HookSystemManager.hpp"
#include "../desktop/state/FocusState.hpp"
#include "../desktop/view/Group.hpp"

using namespace Layout;

CLayoutManager::CLayoutManager() {
    static auto P = g_pHookSystem->hookDynamic("monitorLayoutChanged", [](void* hk, SCallbackInfo& info, std::any param) {
        for (const auto& ws : g_pCompositor->getWorkspaces()) {
            ws->m_space->recheckWorkArea();
        }
    });
}

void CLayoutManager::newTarget(SP<ITarget> target, SP<CSpace> space) {
    // on a new target: remember desired pos for float, if available
    if (const auto DESIRED_GEOM = target->desiredGeometry(); DESIRED_GEOM)
        target->rememberFloatingSize(DESIRED_GEOM->size);

    target->assignToSpace(space);
}

void CLayoutManager::removeTarget(SP<ITarget> target) {
    target->assignToSpace(nullptr);
}

void CLayoutManager::changeFloatingMode(SP<ITarget> target) {
    if (!target->space())
        return;

    target->space()->toggleTargetFloating(target);
}

void CLayoutManager::beginDragTarget(SP<ITarget> target, eMouseBindMode mode) {
    m_dragStateController->dragBegin(target, mode);
}

void CLayoutManager::moveMouse(const Vector2D& mousePos) {
    m_dragStateController->mouseMove(mousePos);
}

void CLayoutManager::resizeTarget(const Vector2D& Δ, SP<ITarget> target, eRectCorner corner) {
    if (target->isPseudo()) {
        auto fixedΔ = Δ;
        if (corner == CORNER_TOPLEFT || corner == CORNER_BOTTOMLEFT)
            fixedΔ.x = -fixedΔ.x;
        if (corner == CORNER_TOPLEFT || corner == CORNER_TOPRIGHT)
            fixedΔ.y = -fixedΔ.y;

        auto       newPseudoSize    = target->pseudoSize() + fixedΔ;
        const auto TARGET_TILE_SIZE = target->position().size();
        newPseudoSize.x             = std::clamp(newPseudoSize.x, MIN_WINDOW_SIZE, TARGET_TILE_SIZE.x);
        newPseudoSize.y             = std::clamp(newPseudoSize.y, MIN_WINDOW_SIZE, TARGET_TILE_SIZE.y);

        target->setPseudoSize(newPseudoSize);

        return;
    }

    target->space()->resizeTarget(Δ, target, corner);
}

std::expected<void, std::string> CLayoutManager::layoutMsg(const std::string_view& sv) {

    const auto MONITOR = Desktop::focusState()->monitor();
    // forward to the active workspace
    if (!MONITOR)
        return std::unexpected("No monitor, can't find ws to target");

    auto ws = MONITOR->m_activeSpecialWorkspace ? MONITOR->m_activeSpecialWorkspace : MONITOR->m_activeWorkspace;

    if (!ws)
        return std::unexpected("No workspace, can't target");

    return ws->m_space->layoutMsg(sv);
}

void CLayoutManager::moveTarget(const Vector2D& Δ, SP<ITarget> target) {
    if (!target->floating())
        return;

    target->space()->moveTarget(Δ, target);
}

void CLayoutManager::endDragTarget() {
    m_dragStateController->dragEnd();
}

void CLayoutManager::fullscreenRequestForTarget(SP<ITarget> target, eFullscreenMode currentEffectiveMode, eFullscreenMode effectiveMode) {
    target->space()->setFullscreen(target, effectiveMode);
}

void CLayoutManager::switchTargets(SP<ITarget> a, SP<ITarget> b, bool preserveFocus) {

    if (preserveFocus) {
        a->swap(b);
        return;
    }

    const auto IS_A_ACTIVE = Desktop::focusState()->window() == a->window();
    const auto IS_B_ACTIVE = Desktop::focusState()->window() == b->window();

    a->swap(b);

    if (IS_A_ACTIVE && b->window())
        Desktop::focusState()->fullWindowFocus(b->window(), Desktop::FOCUS_REASON_KEYBIND);

    if (IS_B_ACTIVE && a->window())
        Desktop::focusState()->fullWindowFocus(a->window(), Desktop::FOCUS_REASON_KEYBIND);
}

void CLayoutManager::moveInDirection(SP<ITarget> target, const std::string& direction, bool silent) {
    Math::eDirection dir = Math::fromChar(direction.at(0));
    if (dir == Math::DIRECTION_DEFAULT) {
        Log::logger->log(Log::ERR, "invalid direction for moveInDirection: {}", direction);
        return;
    }

    target->space()->moveTargetInDirection(target, dir, silent);
}

SP<ITarget> CLayoutManager::getNextCandidate(SP<CSpace> space, SP<ITarget> from) {
    return space->getNextCandidate(from);
}

bool CLayoutManager::isReachable(SP<ITarget> target) {
    return true;
}

void CLayoutManager::bringTargetToTop(SP<ITarget> target) {
    if (!target)
        return;

    if (target->window()->m_group) {
        // grouped, change the current to this window
        target->window()->m_group->setCurrent(target->window());
    }
}

std::optional<Vector2D> CLayoutManager::predictSizeForNewTiledTarget() {
    const auto FOCUSED_MON = Desktop::focusState()->monitor();

    if (!FOCUSED_MON || !FOCUSED_MON->m_activeWorkspace)
        return std::nullopt;

    if (FOCUSED_MON->m_activeSpecialWorkspace)
        return FOCUSED_MON->m_activeSpecialWorkspace->m_space->predictSizeForNewTiledTarget();

    return FOCUSED_MON->m_activeWorkspace->m_space->predictSizeForNewTiledTarget();
}

const UP<Supplementary::CDragStateController>& CLayoutManager::dragController() {
    return m_dragStateController;
}

static inline bool canSnap(const double SIDEA, const double SIDEB, const double GAP) {
    return std::abs(SIDEA - SIDEB) < GAP;
}

static void snapMove(double& start, double& end, const double P) {
    end   = P + (end - start);
    start = P;
}

static void snapResize(double& start, double& end, const double P) {
    start = P;
}

using SnapFn = std::function<void(double&, double&, const double)>;

void CLayoutManager::performSnap(Vector2D& sourcePos, Vector2D& sourceSize, SP<ITarget> DRAGGINGTARGET, const eMouseBindMode MODE, const int CORNER, const Vector2D& BEGINSIZE) {

    const auto DRAGGINGWINDOW = DRAGGINGTARGET->window();

    if (!Desktop::View::validMapped(DRAGGINGWINDOW))
        return;

    static auto  SNAPWINDOWGAP     = CConfigValue<Hyprlang::INT>("general:snap:window_gap");
    static auto  SNAPMONITORGAP    = CConfigValue<Hyprlang::INT>("general:snap:monitor_gap");
    static auto  SNAPBORDEROVERLAP = CConfigValue<Hyprlang::INT>("general:snap:border_overlap");
    static auto  SNAPRESPECTGAPS   = CConfigValue<Hyprlang::INT>("general:snap:respect_gaps");

    static auto  PGAPSIN  = CConfigValue<Hyprlang::CUSTOMTYPE>("general:gaps_in");
    static auto  PGAPSOUT = CConfigValue<Hyprlang::CUSTOMTYPE>("general:gaps_out");
    const auto   GAPSNONE = CCssGapData{0, 0, 0, 0};

    const SnapFn SNAP  = (MODE == MBIND_MOVE) ? snapMove : snapResize;
    int          snaps = 0;

    struct SRange {
        double start = 0;
        double end   = 0;
    };
    const auto EXTENTS = DRAGGINGWINDOW->getWindowExtentsUnified(Desktop::View::RESERVED_EXTENTS | Desktop::View::INPUT_EXTENTS);
    SRange     sourceX = {sourcePos.x - EXTENTS.topLeft.x, sourcePos.x + sourceSize.x + EXTENTS.bottomRight.x};
    SRange     sourceY = {sourcePos.y - EXTENTS.topLeft.y, sourcePos.y + sourceSize.y + EXTENTS.bottomRight.y};

    if (*SNAPWINDOWGAP) {
        const double GAPSIZE       = *SNAPWINDOWGAP;
        const auto   WSID          = DRAGGINGWINDOW->workspaceID();
        const bool   HASFULLSCREEN = DRAGGINGWINDOW->m_workspace && DRAGGINGWINDOW->m_workspace->m_hasFullscreenWindow;

        const auto*  GAPSIN = *SNAPRESPECTGAPS ? sc<CCssGapData*>(PGAPSIN.ptr()->getData()) : &GAPSNONE;
        const double GAPSX  = GAPSIN->m_left + GAPSIN->m_right;
        const double GAPSY  = GAPSIN->m_top + GAPSIN->m_bottom;

        for (auto& other : g_pCompositor->m_windows) {
            if ((HASFULLSCREEN && !other->m_createdOverFullscreen) || other == DRAGGINGWINDOW || other->workspaceID() != WSID || !other->m_isMapped || other->m_fadingOut ||
                other->isX11OverrideRedirect())
                continue;

            const CBox   SURF   = other->getWindowBoxUnified(Desktop::View::RESERVED_EXTENTS);
            const SRange SURFBX = {SURF.x - GAPSX, SURF.x + SURF.w + GAPSX};
            const SRange SURFBY = {SURF.y - GAPSY, SURF.y + SURF.h + GAPSY};

            // only snap windows if their ranges overlap in the opposite axis
            if (sourceY.start <= SURFBY.end && SURFBY.start <= sourceY.end) {
                if (CORNER & (CORNER_TOPLEFT | CORNER_BOTTOMLEFT) && canSnap(sourceX.start, SURFBX.end, GAPSIZE)) {
                    SNAP(sourceX.start, sourceX.end, SURFBX.end);
                    snaps |= SNAP_LEFT;
                } else if (CORNER & (CORNER_TOPRIGHT | CORNER_BOTTOMRIGHT) && canSnap(sourceX.end, SURFBX.start, GAPSIZE)) {
                    SNAP(sourceX.end, sourceX.start, SURFBX.start);
                    snaps |= SNAP_RIGHT;
                }
            }
            if (sourceX.start <= SURFBX.end && SURFBX.start <= sourceX.end) {
                if (CORNER & (CORNER_TOPLEFT | CORNER_TOPRIGHT) && canSnap(sourceY.start, SURFBY.end, GAPSIZE)) {
                    SNAP(sourceY.start, sourceY.end, SURFBY.end);
                    snaps |= SNAP_UP;
                } else if (CORNER & (CORNER_BOTTOMLEFT | CORNER_BOTTOMRIGHT) && canSnap(sourceY.end, SURFBY.start, GAPSIZE)) {
                    SNAP(sourceY.end, sourceY.start, SURFBY.start);
                    snaps |= SNAP_DOWN;
                }
            }

            // corner snapping
            if (sourceX.start == SURFBX.end || SURFBX.start == sourceX.end) {
                const SRange SURFY = {SURFBY.start + GAPSY, SURFBY.end - GAPSY};
                if (CORNER & (CORNER_TOPLEFT | CORNER_TOPRIGHT) && !(snaps & SNAP_UP) && canSnap(sourceY.start, SURFY.start, GAPSIZE)) {
                    SNAP(sourceY.start, sourceY.end, SURFY.start);
                    snaps |= SNAP_UP;
                } else if (CORNER & (CORNER_BOTTOMLEFT | CORNER_BOTTOMRIGHT) && !(snaps & SNAP_DOWN) && canSnap(sourceY.end, SURFY.end, GAPSIZE)) {
                    SNAP(sourceY.end, sourceY.start, SURFY.end);
                    snaps |= SNAP_DOWN;
                }
            }
            if (sourceY.start == SURFBY.end || SURFBY.start == sourceY.end) {
                const SRange SURFX = {SURFBX.start + GAPSX, SURFBX.end - GAPSX};
                if (CORNER & (CORNER_TOPLEFT | CORNER_BOTTOMLEFT) && !(snaps & SNAP_LEFT) && canSnap(sourceX.start, SURFX.start, GAPSIZE)) {
                    SNAP(sourceX.start, sourceX.end, SURFX.start);
                    snaps |= SNAP_LEFT;
                } else if (CORNER & (CORNER_TOPRIGHT | CORNER_BOTTOMRIGHT) && !(snaps & SNAP_RIGHT) && canSnap(sourceX.end, SURFX.end, GAPSIZE)) {
                    SNAP(sourceX.end, sourceX.start, SURFX.end);
                    snaps |= SNAP_RIGHT;
                }
            }
        }
    }

    if (*SNAPMONITORGAP) {
        const double GAPSIZE    = *SNAPMONITORGAP;
        const auto   EXTENTNONE = SBoxExtents{{0, 0}, {0, 0}};
        const auto*  EXTENTDIFF = *SNAPBORDEROVERLAP ? &EXTENTS : &EXTENTNONE;
        const auto   MON        = DRAGGINGWINDOW->m_monitor.lock();

        const auto*  GAPSOUT   = *SNAPRESPECTGAPS ? sc<CCssGapData*>(PGAPSOUT.ptr()->getData()) : &GAPSNONE;
        const auto   WORK_AREA = Desktop::CReservedArea{GAPSOUT->m_top, GAPSOUT->m_right, GAPSOUT->m_bottom, GAPSOUT->m_left}.apply(MON->logicalBoxMinusReserved());

        SRange       monX = {WORK_AREA.x, WORK_AREA.x + WORK_AREA.w};
        SRange       monY = {WORK_AREA.y, WORK_AREA.y + WORK_AREA.h};

        const bool   HAS_LEFT   = MON->m_reservedArea.left() > 0;
        const bool   HAS_TOP    = MON->m_reservedArea.top() > 0;
        const bool   HAS_BOTTOM = MON->m_reservedArea.bottom() > 0;
        const bool   HAS_RIGHT  = MON->m_reservedArea.right() > 0;

        if (CORNER & (CORNER_TOPLEFT | CORNER_BOTTOMLEFT) &&
            ((HAS_LEFT && canSnap(sourceX.start, monX.start, GAPSIZE)) || canSnap(sourceX.start, (monX.start -= MON->m_reservedArea.left() + EXTENTDIFF->topLeft.x), GAPSIZE))) {
            SNAP(sourceX.start, sourceX.end, monX.start);
            snaps |= SNAP_LEFT;
        }
        if (CORNER & (CORNER_TOPRIGHT | CORNER_BOTTOMRIGHT) &&
            ((HAS_RIGHT && canSnap(sourceX.end, monX.end, GAPSIZE)) || canSnap(sourceX.end, (monX.end += MON->m_reservedArea.right() + EXTENTDIFF->bottomRight.x), GAPSIZE))) {
            SNAP(sourceX.end, sourceX.start, monX.end);
            snaps |= SNAP_RIGHT;
        }
        if (CORNER & (CORNER_TOPLEFT | CORNER_TOPRIGHT) &&
            ((HAS_TOP && canSnap(sourceY.start, monY.start, GAPSIZE)) || canSnap(sourceY.start, (monY.start -= MON->m_reservedArea.top() + EXTENTDIFF->topLeft.y), GAPSIZE))) {
            SNAP(sourceY.start, sourceY.end, monY.start);
            snaps |= SNAP_UP;
        }
        if (CORNER & (CORNER_BOTTOMLEFT | CORNER_BOTTOMRIGHT) &&
            ((HAS_BOTTOM && canSnap(sourceY.end, monY.end, GAPSIZE)) || canSnap(sourceY.end, (monY.end += MON->m_reservedArea.bottom() + EXTENTDIFF->bottomRight.y), GAPSIZE))) {
            SNAP(sourceY.end, sourceY.start, monY.end);
            snaps |= SNAP_DOWN;
        }
    }

    // remove extents from main surface
    sourceX = {sourceX.start + EXTENTS.topLeft.x, sourceX.end - EXTENTS.bottomRight.x};
    sourceY = {sourceY.start + EXTENTS.topLeft.y, sourceY.end - EXTENTS.bottomRight.y};

    if (MODE == MBIND_RESIZE_FORCE_RATIO) {
        if ((CORNER & (CORNER_TOPLEFT | CORNER_BOTTOMLEFT) && snaps & SNAP_LEFT) || (CORNER & (CORNER_TOPRIGHT | CORNER_BOTTOMRIGHT) && snaps & SNAP_RIGHT)) {
            const double SIZEY = (sourceX.end - sourceX.start) * (BEGINSIZE.y / BEGINSIZE.x);
            if (CORNER & (CORNER_TOPLEFT | CORNER_TOPRIGHT))
                sourceY.start = sourceY.end - SIZEY;
            else
                sourceY.end = sourceY.start + SIZEY;
        } else if ((CORNER & (CORNER_TOPLEFT | CORNER_TOPRIGHT) && snaps & SNAP_UP) || (CORNER & (CORNER_BOTTOMLEFT | CORNER_BOTTOMRIGHT) && snaps & SNAP_DOWN)) {
            const double SIZEX = (sourceY.end - sourceY.start) * (BEGINSIZE.x / BEGINSIZE.y);
            if (CORNER & (CORNER_TOPLEFT | CORNER_BOTTOMLEFT))
                sourceX.start = sourceX.end - SIZEX;
            else
                sourceX.end = sourceX.start + SIZEX;
        }
    }

    sourcePos  = {sourceX.start, sourceY.start};
    sourceSize = {sourceX.end - sourceX.start, sourceY.end - sourceY.start};
}

void CLayoutManager::recalculateMonitor(PHLMONITOR m) {
    if (m->m_activeSpecialWorkspace)
        m->m_activeSpecialWorkspace->m_space->recalculate();
    if (m->m_activeWorkspace)
        m->m_activeWorkspace->m_space->recalculate();
}

void CLayoutManager::invalidateMonitorGeometries(PHLMONITOR m) {
    for (const auto& ws : g_pCompositor->getWorkspaces()) {
        if (ws && ws->m_monitor == m) {
            ws->m_space->recheckWorkArea();
            ws->m_space->recalculate();
        }
    }
}
