#include "DwindleAlgorithm.hpp"

#include "../../Algorithm.hpp"
#include "../../../space/Space.hpp"
#include "../../../target/WindowTarget.hpp"
#include "../../../LayoutManager.hpp"

#include "../../../../config/ConfigValue.hpp"
#include "../../../../desktop/state/FocusState.hpp"
#include "../../../../helpers/Monitor.hpp"
#include "../../../../Compositor.hpp"

#include <hyprutils/utils/ScopeGuard.hpp>

using namespace Layout;
using namespace Layout::Tiled;

struct Layout::Tiled::SDwindleNodeData {
    WP<SDwindleNodeData>                pParent;
    bool                                isNode = false;
    WP<ITarget>                         pTarget;
    std::array<WP<SDwindleNodeData>, 2> children = {};
    WP<SDwindleNodeData>                self;
    bool                                splitTop               = false; // for preserve_split
    CBox                                box                    = {0};
    float                               splitRatio             = 1.f;
    bool                                valid                  = true;
    bool                                ignoreFullscreenChecks = false;

    // For list lookup
    bool operator==(const SDwindleNodeData& rhs) const {
        return pTarget.lock() == rhs.pTarget.lock() && box == rhs.box && pParent == rhs.pParent && children[0] == rhs.children[0] && children[1] == rhs.children[1];
    }

    void recalcSizePosRecursive(bool force = false, bool horizontalOverride = false, bool verticalOverride = false) {
        if (children[0]) {
            static auto PSMARTSPLIT    = CConfigValue<Hyprlang::INT>("dwindle:smart_split");
            static auto PPRESERVESPLIT = CConfigValue<Hyprlang::INT>("dwindle:preserve_split");
            static auto PFLMULT        = CConfigValue<Hyprlang::FLOAT>("dwindle:split_width_multiplier");

            if (*PPRESERVESPLIT == 0 && *PSMARTSPLIT == 0)
                splitTop = box.h * *PFLMULT > box.w;

            if (verticalOverride)
                splitTop = true;
            else if (horizontalOverride)
                splitTop = false;

            const auto SPLITSIDE = !splitTop;

            if (SPLITSIDE) {
                // split left/right
                const float FIRSTSIZE = box.w / 2.0 * splitRatio;
                children[0]->box      = CBox{box.x, box.y, FIRSTSIZE, box.h}.noNegativeSize();
                children[1]->box      = CBox{box.x + FIRSTSIZE, box.y, box.w - FIRSTSIZE, box.h}.noNegativeSize();
            } else {
                // split top/bottom
                const float FIRSTSIZE = box.h / 2.0 * splitRatio;
                children[0]->box      = CBox{box.x, box.y, box.w, FIRSTSIZE}.noNegativeSize();
                children[1]->box      = CBox{box.x, box.y + FIRSTSIZE, box.w, box.h - FIRSTSIZE}.noNegativeSize();
            }

            children[0]->recalcSizePosRecursive(force);
            children[1]->recalcSizePosRecursive(force);
        } else
            pTarget->setPositionGlobal(box);
    }
};

void CDwindleAlgorithm::newTarget(SP<ITarget> target) {
    addTarget(target);
}

void CDwindleAlgorithm::addTarget(SP<ITarget> target, bool newTarget) {
    const auto WORK_AREA = m_parent->space()->workArea();

    const auto PNODE = m_dwindleNodesData.emplace_back(makeShared<SDwindleNodeData>());
    PNODE->self      = PNODE;

    const auto  PMONITOR   = m_parent->space()->workspace()->m_monitor;
    const auto  PWORKSPACE = m_parent->space()->workspace();

    static auto PUSEACTIVE    = CConfigValue<Hyprlang::INT>("dwindle:use_active_for_splits");
    static auto PDEFAULTSPLIT = CConfigValue<Hyprlang::FLOAT>("dwindle:default_split_ratio");

    // Populate the node with our window's data
    PNODE->pTarget = target;
    PNODE->isNode  = false;

    SP<SDwindleNodeData> OPENINGON;

    const auto           MOUSECOORDS = m_overrideFocalPoint.value_or(g_pInputManager->getMouseCoordsInternal());
    const auto           ACTIVE_MON  = Desktop::focusState()->monitor();

    if ((PWORKSPACE == ACTIVE_MON->m_activeWorkspace || (PWORKSPACE->m_isSpecialWorkspace && PMONITOR->m_activeSpecialWorkspace)) && !*PUSEACTIVE) {
        OPENINGON = getNodeFromWindow(
            g_pCompositor->vectorToWindowUnified(MOUSECOORDS, Desktop::View::RESERVED_EXTENTS | Desktop::View::INPUT_EXTENTS | Desktop::View::SKIP_FULLSCREEN_PRIORITY));

        if (!OPENINGON && g_pCompositor->isPointOnReservedArea(MOUSECOORDS, ACTIVE_MON))
            OPENINGON = getClosestNode(MOUSECOORDS);

    } else if (*PUSEACTIVE) {
        if (Desktop::focusState()->window() && !Desktop::focusState()->window()->m_isFloating && Desktop::focusState()->window() != target->window() &&
            Desktop::focusState()->window()->m_workspace == PWORKSPACE && Desktop::focusState()->window()->m_isMapped) {
            OPENINGON = getNodeFromWindow(Desktop::focusState()->window());
        } else {
            OPENINGON = getNodeFromWindow(g_pCompositor->vectorToWindowUnified(MOUSECOORDS, Desktop::View::RESERVED_EXTENTS | Desktop::View::INPUT_EXTENTS));
        }

        if (!OPENINGON && g_pCompositor->isPointOnReservedArea(MOUSECOORDS, ACTIVE_MON))
            OPENINGON = getClosestNode(MOUSECOORDS);

    } else
        OPENINGON = getFirstNode();

    // first, check if OPENINGON isn't too big.
    const auto PREDSIZEMAX = OPENINGON ? Vector2D(OPENINGON->box.w, OPENINGON->box.h) : PMONITOR->m_size;
    if (const auto MAXSIZE = target->maxSize().value_or(Math::VECTOR2D_MAX); MAXSIZE.x < PREDSIZEMAX.x || MAXSIZE.y < PREDSIZEMAX.y) {
        // we can't continue. make it floating.
        std::erase(m_dwindleNodesData, PNODE);
        m_parent->setFloating(target, true, true);
        return;
    }

    // last fail-safe to avoid duplicate fullscreens
    if ((!OPENINGON || OPENINGON->pTarget.lock() == target) && getNodes() > 1) {
        for (auto& node : m_dwindleNodesData) {
            if (node->pTarget.lock() && node->pTarget.lock() != target) {
                OPENINGON = node;
                break;
            }
        }
    }

    // if it's the first, it's easy. Make it fullscreen.
    if (!OPENINGON || OPENINGON->pTarget.lock() == target) {
        PNODE->box = WORK_AREA;
        PNODE->pTarget->setPositionGlobal(PNODE->box);
        return;
    }

    // get the node under our cursor

    const auto NEWPARENT = m_dwindleNodesData.emplace_back(makeShared<SDwindleNodeData>());

    // make the parent have the OPENINGON's stats
    NEWPARENT->box        = OPENINGON->box;
    NEWPARENT->pParent    = OPENINGON->pParent;
    NEWPARENT->isNode     = true; // it is a node
    NEWPARENT->splitRatio = std::clamp(*PDEFAULTSPLIT, 0.1F, 1.9F);

    static auto PWIDTHMULTIPLIER = CConfigValue<Hyprlang::FLOAT>("dwindle:split_width_multiplier");

    // if cursor over first child, make it first, etc
    const auto SIDEBYSIDE = NEWPARENT->box.w > NEWPARENT->box.h * *PWIDTHMULTIPLIER;
    NEWPARENT->splitTop   = !SIDEBYSIDE;

    static auto PFORCESPLIT                = CConfigValue<Hyprlang::INT>("dwindle:force_split");
    static auto PERMANENTDIRECTIONOVERRIDE = CConfigValue<Hyprlang::INT>("dwindle:permanent_direction_override");
    static auto PSMARTSPLIT                = CConfigValue<Hyprlang::INT>("dwindle:smart_split");
    static auto PSPLITBIAS                 = CConfigValue<Hyprlang::INT>("dwindle:split_bias");

    bool        horizontalOverride = false;
    bool        verticalOverride   = false;

    // let user select position -> top, right, bottom, left
    if (m_overrideDirection != Math::DIRECTION_DEFAULT) {

        // this is horizontal
        if (m_overrideDirection % 2 == 0)
            verticalOverride = true;
        else
            horizontalOverride = true;

        // 0 -> top and left | 1,2 -> right and bottom
        if (m_overrideDirection % 3 == 0) {
            NEWPARENT->children[1] = OPENINGON;
            NEWPARENT->children[0] = PNODE;
        } else {
            NEWPARENT->children[0] = OPENINGON;
            NEWPARENT->children[1] = PNODE;
        }

        // whether or not the override persists after opening one window
        if (*PERMANENTDIRECTIONOVERRIDE == 0)
            m_overrideDirection = Math::DIRECTION_DEFAULT;
    } else if (*PSMARTSPLIT == 1) {
        const auto PARENT_CENTER      = NEWPARENT->box.pos() + NEWPARENT->box.size() / 2;
        const auto PARENT_PROPORTIONS = NEWPARENT->box.h / NEWPARENT->box.w;
        const auto DELTA              = MOUSECOORDS - PARENT_CENTER;
        const auto DELTA_SLOPE        = DELTA.y / DELTA.x;

        if (abs(DELTA_SLOPE) < PARENT_PROPORTIONS) {
            if (DELTA.x > 0) {
                // right
                NEWPARENT->splitTop    = false;
                NEWPARENT->children[0] = OPENINGON;
                NEWPARENT->children[1] = PNODE;
            } else {
                // left
                NEWPARENT->splitTop    = false;
                NEWPARENT->children[0] = PNODE;
                NEWPARENT->children[1] = OPENINGON;
            }
        } else {
            if (DELTA.y > 0) {
                // bottom
                NEWPARENT->splitTop    = true;
                NEWPARENT->children[0] = OPENINGON;
                NEWPARENT->children[1] = PNODE;
            } else {
                // top
                NEWPARENT->splitTop    = true;
                NEWPARENT->children[0] = PNODE;
                NEWPARENT->children[1] = OPENINGON;
            }
        }
    } else if (*PFORCESPLIT == 0 || !newTarget) {
        if ((SIDEBYSIDE &&
             VECINRECT(MOUSECOORDS, NEWPARENT->box.x, NEWPARENT->box.y / *PWIDTHMULTIPLIER, NEWPARENT->box.x + NEWPARENT->box.w / 2.f, NEWPARENT->box.y + NEWPARENT->box.h)) ||
            (!SIDEBYSIDE &&
             VECINRECT(MOUSECOORDS, NEWPARENT->box.x, NEWPARENT->box.y / *PWIDTHMULTIPLIER, NEWPARENT->box.x + NEWPARENT->box.w, NEWPARENT->box.y + NEWPARENT->box.h / 2.f))) {
            // we are hovering over the first node, make PNODE first.
            NEWPARENT->children[1] = OPENINGON;
            NEWPARENT->children[0] = PNODE;
        } else {
            // we are hovering over the second node, make PNODE second.
            NEWPARENT->children[0] = OPENINGON;
            NEWPARENT->children[1] = PNODE;
        }
    } else {
        if (*PFORCESPLIT == 1) {
            NEWPARENT->children[1] = OPENINGON;
            NEWPARENT->children[0] = PNODE;
        } else {
            NEWPARENT->children[0] = OPENINGON;
            NEWPARENT->children[1] = PNODE;
        }
    }

    // split in favor of a specific window
    if (*PSPLITBIAS && NEWPARENT->children[0] == PNODE)
        NEWPARENT->splitRatio = 2.f - NEWPARENT->splitRatio;

    // and update the previous parent if it exists
    if (OPENINGON->pParent) {
        if (OPENINGON->pParent->children[0] == OPENINGON) {
            OPENINGON->pParent->children[0] = NEWPARENT;
        } else {
            OPENINGON->pParent->children[1] = NEWPARENT;
        }
    }

    // Update the children
    if (!verticalOverride && (NEWPARENT->box.w * *PWIDTHMULTIPLIER > NEWPARENT->box.h || horizontalOverride)) {
        // split left/right -> forced
        OPENINGON->box = {NEWPARENT->box.pos(), Vector2D(NEWPARENT->box.w / 2.f, NEWPARENT->box.h)};
        PNODE->box     = {Vector2D(NEWPARENT->box.x + NEWPARENT->box.w / 2.f, NEWPARENT->box.y), Vector2D(NEWPARENT->box.w / 2.f, NEWPARENT->box.h)};
    } else {
        // split top/bottom
        OPENINGON->box = {NEWPARENT->box.pos(), Vector2D(NEWPARENT->box.w, NEWPARENT->box.h / 2.f)};
        PNODE->box     = {Vector2D(NEWPARENT->box.x, NEWPARENT->box.y + NEWPARENT->box.h / 2.f), Vector2D(NEWPARENT->box.w, NEWPARENT->box.h / 2.f)};
    }

    OPENINGON->pParent = NEWPARENT;
    PNODE->pParent     = NEWPARENT;

    NEWPARENT->recalcSizePosRecursive(false, horizontalOverride, verticalOverride);

    calculateWorkspace();
}

void CDwindleAlgorithm::movedTarget(SP<ITarget> target, std::optional<Vector2D> focalPoint) {
    m_overrideFocalPoint = focalPoint;
    addTarget(target, false);
    m_overrideFocalPoint.reset();
}

void CDwindleAlgorithm::removeTarget(SP<ITarget> target) {
    const auto PNODE = getNodeFromTarget(target);

    if (!PNODE) {
        Log::logger->log(Log::ERR, "onWindowRemovedTiling node null?");
        return;
    }

    if (target->fullscreenMode() != FSMODE_NONE)
        g_pCompositor->setWindowFullscreenInternal(target->window(), FSMODE_NONE);

    const auto PPARENT = PNODE->pParent;

    if (!PPARENT) {
        Log::logger->log(Log::DEBUG, "Removing last node (dwindle)");
        std::erase(m_dwindleNodesData, PNODE);
        return;
    }

    const auto PSIBLING = PPARENT->children[0] == PNODE ? PPARENT->children[1] : PPARENT->children[0];

    PSIBLING->pParent = PPARENT->pParent;

    if (PPARENT->pParent != nullptr) {
        if (PPARENT->pParent->children[0] == PPARENT)
            PPARENT->pParent->children[0] = PSIBLING;
        else
            PPARENT->pParent->children[1] = PSIBLING;
    }

    PPARENT->valid = false;
    PNODE->valid   = false;

    std::erase(m_dwindleNodesData, PPARENT);
    std::erase(m_dwindleNodesData, PNODE);

    recalculate();
}

void CDwindleAlgorithm::resizeTarget(const Vector2D& Δ, SP<ITarget> target, eRectCorner corner) {
    if (!validMapped(target->window()))
        return;

    const auto PNODE = getNodeFromTarget(target);

    if (!PNODE)
        return;

    static auto PANIMATE       = CConfigValue<Hyprlang::INT>("misc:animate_manual_resizes");
    static auto PSMARTRESIZING = CConfigValue<Hyprlang::INT>("dwindle:smart_resizing");

    // get some data about our window
    const auto PMONITOR         = m_parent->space()->workspace()->m_monitor;
    const auto MONITOR_WORKAREA = PMONITOR->logicalBoxMinusReserved();
    const auto BOX              = target->position();
    const bool DISPLAYLEFT      = STICKS(BOX.x, MONITOR_WORKAREA.x);
    const bool DISPLAYRIGHT     = STICKS(BOX.x + BOX.w, MONITOR_WORKAREA.x + MONITOR_WORKAREA.w);
    const bool DISPLAYTOP       = STICKS(BOX.y, MONITOR_WORKAREA.y);
    const bool DISPLAYBOTTOM    = STICKS(BOX.y + BOX.h, MONITOR_WORKAREA.y + MONITOR_WORKAREA.h);

    // construct allowed movement
    Vector2D allowedMovement = Δ;
    if (DISPLAYLEFT && DISPLAYRIGHT)
        allowedMovement.x = 0;

    if (DISPLAYBOTTOM && DISPLAYTOP)
        allowedMovement.y = 0;

    if (*PSMARTRESIZING == 1) {
        // Identify inner and outer nodes for both directions
        SP<SDwindleNodeData> PVOUTER = nullptr;
        SP<SDwindleNodeData> PVINNER = nullptr;
        SP<SDwindleNodeData> PHOUTER = nullptr;
        SP<SDwindleNodeData> PHINNER = nullptr;

        const auto           LEFT   = corner == CORNER_TOPLEFT || corner == CORNER_BOTTOMLEFT || DISPLAYRIGHT;
        const auto           TOP    = corner == CORNER_TOPLEFT || corner == CORNER_TOPRIGHT || DISPLAYBOTTOM;
        const auto           RIGHT  = corner == CORNER_TOPRIGHT || corner == CORNER_BOTTOMRIGHT || DISPLAYLEFT;
        const auto           BOTTOM = corner == CORNER_BOTTOMLEFT || corner == CORNER_BOTTOMRIGHT || DISPLAYTOP;
        const auto           NONE   = corner == CORNER_NONE;

        for (auto PCURRENT = PNODE; PCURRENT && PCURRENT->pParent; PCURRENT = PCURRENT->pParent.lock()) {
            const auto PPARENT = PCURRENT->pParent;

            if (!PVOUTER && PPARENT->splitTop && (NONE || (TOP && PPARENT->children[1] == PCURRENT) || (BOTTOM && PPARENT->children[0] == PCURRENT)))
                PVOUTER = PCURRENT;
            else if (!PVOUTER && !PVINNER && PPARENT->splitTop)
                PVINNER = PCURRENT;
            else if (!PHOUTER && !PPARENT->splitTop && (NONE || (LEFT && PPARENT->children[1] == PCURRENT) || (RIGHT && PPARENT->children[0] == PCURRENT)))
                PHOUTER = PCURRENT;
            else if (!PHOUTER && !PHINNER && !PPARENT->splitTop)
                PHINNER = PCURRENT;

            if (PVOUTER && PHOUTER)
                break;
        }

        if (PHOUTER) {
            PHOUTER->pParent->splitRatio = std::clamp(PHOUTER->pParent->splitRatio + allowedMovement.x * 2.f / PHOUTER->pParent->box.w, 0.1, 1.9);

            if (PHINNER) {
                const auto ORIGINAL = PHINNER->box.w;
                PHOUTER->pParent->recalcSizePosRecursive(*PANIMATE == 0);
                if (PHINNER->pParent->children[0] == PHINNER)
                    PHINNER->pParent->splitRatio = std::clamp((ORIGINAL - allowedMovement.x) / PHINNER->pParent->box.w * 2.f, 0.1, 1.9);
                else
                    PHINNER->pParent->splitRatio = std::clamp(2 - (ORIGINAL + allowedMovement.x) / PHINNER->pParent->box.w * 2.f, 0.1, 1.9);
                PHINNER->pParent->recalcSizePosRecursive(*PANIMATE == 0);
            } else
                PHOUTER->pParent->recalcSizePosRecursive(*PANIMATE == 0);
        }

        if (PVOUTER) {
            PVOUTER->pParent->splitRatio = std::clamp(PVOUTER->pParent->splitRatio + allowedMovement.y * 2.f / PVOUTER->pParent->box.h, 0.1, 1.9);

            if (PVINNER) {
                const auto ORIGINAL = PVINNER->box.h;
                PVOUTER->pParent->recalcSizePosRecursive(*PANIMATE == 0);
                if (PVINNER->pParent->children[0] == PVINNER)
                    PVINNER->pParent->splitRatio = std::clamp((ORIGINAL - allowedMovement.y) / PVINNER->pParent->box.h * 2.f, 0.1, 1.9);
                else
                    PVINNER->pParent->splitRatio = std::clamp(2 - (ORIGINAL + allowedMovement.y) / PVINNER->pParent->box.h * 2.f, 0.1, 1.9);
                PVINNER->pParent->recalcSizePosRecursive(*PANIMATE == 0);
            } else
                PVOUTER->pParent->recalcSizePosRecursive(*PANIMATE == 0);
        }
    } else {
        // get the correct containers to apply splitratio to
        const auto PPARENT = PNODE->pParent;

        if (!PPARENT)
            return; // the only window on a workspace, ignore

        const bool PARENTSIDEBYSIDE = !PPARENT->splitTop;

        // Get the parent's parent
        auto                          PPARENT2 = PPARENT->pParent;

        Hyprutils::Utils::CScopeGuard x([target, this] {
            // snap all windows, don't animate resizes if they are manual
            if (target == g_layoutManager->dragController()->target()) {
                for (const auto& w : m_dwindleNodesData) {
                    if (w->isNode)
                        continue;

                    w->pTarget->warpPositionSize();
                }
            }
        });

        // No parent means we have only 2 windows, and thus one axis of freedom
        if (!PPARENT2) {
            if (PARENTSIDEBYSIDE) {
                allowedMovement.x *= 2.f / PPARENT->box.w;
                PPARENT->splitRatio = std::clamp(PPARENT->splitRatio + allowedMovement.x, 0.1, 1.9);
                PPARENT->recalcSizePosRecursive(*PANIMATE == 0);
            } else {
                allowedMovement.y *= 2.f / PPARENT->box.h;
                PPARENT->splitRatio = std::clamp(PPARENT->splitRatio + allowedMovement.y, 0.1, 1.9);
                PPARENT->recalcSizePosRecursive(*PANIMATE == 0);
            }

            return;
        }

        // Get first parent with other split
        while (PPARENT2 && PPARENT2->splitTop == !PARENTSIDEBYSIDE)
            PPARENT2 = PPARENT2->pParent;

        // no parent, one axis of freedom
        if (!PPARENT2) {
            if (PARENTSIDEBYSIDE) {
                allowedMovement.x *= 2.f / PPARENT->box.w;
                PPARENT->splitRatio = std::clamp(PPARENT->splitRatio + allowedMovement.x, 0.1, 1.9);
                PPARENT->recalcSizePosRecursive(*PANIMATE == 0);
            } else {
                allowedMovement.y *= 2.f / PPARENT->box.h;
                PPARENT->splitRatio = std::clamp(PPARENT->splitRatio + allowedMovement.y, 0.1, 1.9);
                PPARENT->recalcSizePosRecursive(*PANIMATE == 0);
            }

            return;
        }

        // 2 axes of freedom
        const auto SIDECONTAINER = PARENTSIDEBYSIDE ? PPARENT : PPARENT2;
        const auto TOPCONTAINER  = PARENTSIDEBYSIDE ? PPARENT2 : PPARENT;

        allowedMovement.x *= 2.f / SIDECONTAINER->box.w;
        allowedMovement.y *= 2.f / TOPCONTAINER->box.h;

        SIDECONTAINER->splitRatio = std::clamp(SIDECONTAINER->splitRatio + allowedMovement.x, 0.1, 1.9);
        TOPCONTAINER->splitRatio  = std::clamp(TOPCONTAINER->splitRatio + allowedMovement.y, 0.1, 1.9);
        SIDECONTAINER->recalcSizePosRecursive(*PANIMATE == 0);
        TOPCONTAINER->recalcSizePosRecursive(*PANIMATE == 0);
    }

    // snap all windows, don't animate resizes if they are manual
    if (target == g_layoutManager->dragController()->target()) {
        for (const auto& w : m_dwindleNodesData) {
            if (w->isNode)
                continue;

            w->pTarget->warpPositionSize();
        }
    }
}

SP<ITarget> CDwindleAlgorithm::getNextCandidate(SP<ITarget> old) {
    const auto MIDDLE = old->position().middle();

    if (const auto NODE = getClosestNode(MIDDLE); NODE)
        return NODE->pTarget.lock();

    if (const auto NODE = getFirstNode(); NODE)
        return NODE->pTarget.lock();

    return nullptr;
}

void CDwindleAlgorithm::swapTargets(SP<ITarget> a, SP<ITarget> b) {
    auto nodeA = getNodeFromTarget(a);
    auto nodeB = getNodeFromTarget(b);

    if (nodeA)
        nodeA->pTarget = b;
    if (nodeB)
        nodeB->pTarget = a;
}

void CDwindleAlgorithm::recalculate() {
    calculateWorkspace();
}

std::optional<Vector2D> CDwindleAlgorithm::predictSizeForNewTarget() {
    // get window candidate
    PHLWINDOW candidate = Desktop::focusState()->window();

    if (!candidate || candidate->m_workspace != m_parent->space()->workspace())
        candidate = m_parent->space()->workspace()->getFirstWindow();

    // create a fake node
    SDwindleNodeData node;

    if (!candidate)
        return Desktop::focusState()->monitor()->m_size;
    else {
        const auto PNODE = getNodeFromWindow(candidate);

        if (!PNODE)
            return {};

        node = *PNODE;
        node.pTarget.reset();

        CBox        box = PNODE->box;

        static auto PFLMULT = CConfigValue<Hyprlang::FLOAT>("dwindle:split_width_multiplier");

        bool        splitTop = box.h * *PFLMULT > box.w;

        const auto  SPLITSIDE = !splitTop;

        if (SPLITSIDE)
            node.box = {{}, {box.w / 2.0, box.h}};
        else
            node.box = {{}, {box.w, box.h / 2.0}};

        // TODO: make this better and more accurate

        return node.box.size();
    }

    return {};
}

void CDwindleAlgorithm::moveTargetInDirection(SP<ITarget> t, Math::eDirection dir, bool silent) {
    const auto     PNODE       = getNodeFromTarget(t);
    const Vector2D originalPos = t->position().middle();

    if (!PNODE || !t->window())
        return;

    Vector2D   focalPoint;

    const auto WINDOWIDEALBB =
        t->fullscreenMode() != FSMODE_NONE ? m_parent->space()->workspace()->m_monitor->logicalBox() : t->window()->getWindowIdealBoundingBoxIgnoreReserved();

    switch (dir) {
        case Math::DIRECTION_UP: focalPoint = WINDOWIDEALBB.pos() + Vector2D{WINDOWIDEALBB.size().x / 2.0, -1.0}; break;
        case Math::DIRECTION_DOWN: focalPoint = WINDOWIDEALBB.pos() + Vector2D{WINDOWIDEALBB.size().x / 2.0, WINDOWIDEALBB.size().y + 1.0}; break;
        case Math::DIRECTION_LEFT: focalPoint = WINDOWIDEALBB.pos() + Vector2D{-1.0, WINDOWIDEALBB.size().y / 2.0}; break;
        case Math::DIRECTION_RIGHT: focalPoint = WINDOWIDEALBB.pos() + Vector2D{WINDOWIDEALBB.size().x + 1.0, WINDOWIDEALBB.size().y / 2.0}; break;
        default: return;
    }

    t->window()->setAnimationsToMove();

    removeTarget(t);

    const auto PMONITORFOCAL = g_pCompositor->getMonitorFromVector(focalPoint);

    if (PMONITORFOCAL != m_parent->space()->workspace()->m_monitor) {
        // move with a focal point

        if (PMONITORFOCAL->m_activeWorkspace)
            t->assignToSpace(PMONITORFOCAL->m_activeWorkspace->m_space);

        return;
    }

    movedTarget(t, focalPoint);

    // restore focus to the previous position
    if (silent) {
        const auto PNODETOFOCUS = getClosestNode(originalPos);
        if (PNODETOFOCUS && PNODETOFOCUS->pTarget)
            Desktop::focusState()->fullWindowFocus(PNODETOFOCUS->pTarget->window(), Desktop::FOCUS_REASON_KEYBIND);
    }
}

// --------- internal --------- //

void CDwindleAlgorithm::calculateWorkspace() {
    const auto PWORKSPACE = m_parent->space()->workspace();
    const auto PMONITOR   = PWORKSPACE->m_monitor;

    if (!PMONITOR || PWORKSPACE->m_hasFullscreenWindow)
        return;

    const auto TOPNODE = getMasterNode();

    if (TOPNODE) {
        TOPNODE->box = m_parent->space()->workArea();
        TOPNODE->recalcSizePosRecursive();
    }
}

SP<SDwindleNodeData> CDwindleAlgorithm::getNodeFromTarget(SP<ITarget> t) {
    for (const auto& n : m_dwindleNodesData) {
        if (n->pTarget == t)
            return n;
    }

    return nullptr;
}

SP<SDwindleNodeData> CDwindleAlgorithm::getNodeFromWindow(PHLWINDOW w) {
    return w ? getNodeFromTarget(w->layoutTarget()) : nullptr;
}

int CDwindleAlgorithm::getNodes() {
    return m_dwindleNodesData.size();
}

SP<SDwindleNodeData> CDwindleAlgorithm::getFirstNode() {
    return m_dwindleNodesData.empty() ? nullptr : m_dwindleNodesData.at(0);
}

SP<SDwindleNodeData> CDwindleAlgorithm::getClosestNode(const Vector2D& point) {
    SP<SDwindleNodeData> res         = nullptr;
    double               distClosest = -1;
    for (auto& n : m_dwindleNodesData) {
        if (n->pTarget && Desktop::View::validMapped(n->pTarget->window())) {
            auto distAnother = vecToRectDistanceSquared(point, n->box.pos(), n->box.pos() + n->box.size());
            if (!res || distAnother < distClosest) {
                res         = n;
                distClosest = distAnother;
            }
        }
    }
    return res;
}

SP<SDwindleNodeData> CDwindleAlgorithm::getMasterNode() {
    for (auto& n : m_dwindleNodesData) {
        if (!n->pParent)
            return n;
    }
    return nullptr;
}

std::expected<void, std::string> CDwindleAlgorithm::layoutMsg(const std::string_view& sv) {
    const auto ARGS = CVarList2(std::string{sv}, 0, ' ');

    const auto CURRENT_NODE = getNodeFromWindow(Desktop::focusState()->window());

    if (ARGS[0] == "togglesplit") {
        if (CURRENT_NODE)
            toggleSplit(CURRENT_NODE);
    } else if (ARGS[0] == "swapsplit") {
        if (CURRENT_NODE)
            swapSplit(CURRENT_NODE);
    } else if (ARGS[0] == "movetoroot") {
        auto node = CURRENT_NODE;
        if (!ARGS[1].empty()) {
            auto w = g_pCompositor->getWindowByRegex(std::string{ARGS[1]});
            if (w)
                node = getNodeFromWindow(w);
        }

        const auto STABLE = ARGS[2].empty() || ARGS[2] != "unstable";
        moveToRoot(node, STABLE);
    } else if (ARGS[0] == "preselect") {
        auto direction = ARGS[1];

        if (direction.empty()) {
            Log::logger->log(Log::ERR, "Expected direction for preselect");
            return std::unexpected("No direction for preselect");
        }

        switch (direction.front()) {
            case 'u':
            case 't': {
                m_overrideDirection = Math::DIRECTION_UP;
                break;
            }
            case 'd':
            case 'b': {
                m_overrideDirection = Math::DIRECTION_DOWN;
                break;
            }
            case 'r': {
                m_overrideDirection = Math::DIRECTION_RIGHT;
                break;
            }
            case 'l': {
                m_overrideDirection = Math::DIRECTION_LEFT;
                break;
            }
            default: {
                // any other character resets the focus direction
                // needed for the persistent mode
                m_overrideDirection = Math::DIRECTION_DEFAULT;
                break;
            }
        }
    }

    return {};
}

void CDwindleAlgorithm::toggleSplit(SP<SDwindleNodeData> x) {
    if (!x || !x->pParent)
        return;

    if (x->pTarget->fullscreenMode() != FSMODE_NONE)
        return;

    x->pParent->splitTop = !x->pParent->splitTop;

    x->pParent->recalcSizePosRecursive();
}

void CDwindleAlgorithm::swapSplit(SP<SDwindleNodeData> x) {
    if (x->pTarget->fullscreenMode() != FSMODE_NONE)
        return;

    std::swap(x->pParent->children[0], x->pParent->children[1]);

    x->pParent->recalcSizePosRecursive();
}

void CDwindleAlgorithm::moveToRoot(SP<SDwindleNodeData> x, bool stable) {
    if (!x || !x->pParent)
        return;

    if (x->pTarget->fullscreenMode() != FSMODE_NONE)
        return;

    // already at root
    if (!x->pParent->pParent)
        return;

    auto& pNode = x->pParent->children[0] == x ? x->pParent->children[0] : x->pParent->children[1];

    // instead of [getMasterNodeOnWorkspace], we walk back to root since we need
    // to know which children of root is our ancestor
    auto pAncestor = x, pRoot = x->pParent.lock();
    while (pRoot->pParent) {
        pAncestor = pRoot;
        pRoot     = pRoot->pParent.lock();
    }

    auto& pSwap = pRoot->children[0] == pAncestor ? pRoot->children[1] : pRoot->children[0];
    std::swap(pNode, pSwap);
    std::swap(pNode->pParent, pSwap->pParent);

    // [stable] in that the focused window occupies same side of screen
    if (stable)
        std::swap(pRoot->children[0], pRoot->children[1]);

    pRoot->recalcSizePosRecursive();
}
