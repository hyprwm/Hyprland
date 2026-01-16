#include "MasterAlgorithm.hpp"

#include "../../Algorithm.hpp"
#include "../../../space/Space.hpp"
#include "../../../target/WindowTarget.hpp"

#include "../../../../config/ConfigValue.hpp"
#include "../../../../config/ConfigManager.hpp"
#include "../../../../desktop/state/FocusState.hpp"
#include "../../../../helpers/Monitor.hpp"
#include "../../../../Compositor.hpp"

using namespace Layout;
using namespace Layout::Tiled;

struct Layout::Tiled::SMasterNodeData {
    bool        isMaster   = false;
    float       percMaster = 0.5f;

    WP<ITarget> pTarget;

    Vector2D    position;
    Vector2D    size;

    float       percSize = 1.f; // size multiplier for resizing children

    bool        ignoreFullscreenChecks = false;

    //
    bool operator==(const SMasterNodeData& rhs) const {
        return pTarget.lock() == rhs.pTarget.lock();
    }
};

void CMasterAlgorithm::newTarget(SP<ITarget> target) {
    addTarget(target, true);
}

void CMasterAlgorithm::movedTarget(SP<ITarget> target) {
    addTarget(target, false);
}

void CMasterAlgorithm::addTarget(SP<ITarget> target, bool firstMap) {
    static auto PNEWONACTIVE = CConfigValue<std::string>("master:new_on_active");
    static auto PNEWONTOP    = CConfigValue<Hyprlang::INT>("master:new_on_top");
    static auto PNEWSTATUS   = CConfigValue<std::string>("master:new_status");

    const auto  PWORKSPACE = m_parent->space()->workspace();
    const auto  PMONITOR   = PWORKSPACE->m_monitor;

    const bool  BNEWBEFOREACTIVE = *PNEWONACTIVE == "before";
    const bool  BNEWISMASTER     = *PNEWSTATUS == "master";

    const auto  PNODE = [&]() -> SP<SMasterNodeData> {
        if (*PNEWONACTIVE != "none" && !BNEWISMASTER) {
            const auto pLastNode = getNodeFromWindow(Desktop::focusState()->window());
            if (pLastNode && !(pLastNode->isMaster && (getMastersNo() == 1 || *PNEWSTATUS == "slave"))) {
                auto it = std::ranges::find(m_masterNodesData, pLastNode);
                if (!BNEWBEFOREACTIVE)
                    ++it;
                return *m_masterNodesData.emplace(it, makeShared<SMasterNodeData>());
            }
        }
        return *PNEWONTOP ? *m_masterNodesData.emplace(m_masterNodesData.begin(), makeShared<SMasterNodeData>()) : m_masterNodesData.emplace_back(makeShared<SMasterNodeData>());
    }();

    PNODE->pTarget = target;

    const auto   WINDOWSONWORKSPACE = getNodesNo();
    static auto  PMFACT             = CConfigValue<Hyprlang::FLOAT>("master:mfact");
    float        lastSplitPercent   = *PMFACT;

    auto         OPENINGON = isWindowTiled(Desktop::focusState()->window()) && Desktop::focusState()->window()->m_workspace == PWORKSPACE ?
                getNodeFromWindow(Desktop::focusState()->window()) :
                getMasterNode();

    const auto   MOUSECOORDS   = g_pInputManager->getMouseCoordsInternal();
    static auto  PDROPATCURSOR = CConfigValue<Hyprlang::INT>("master:drop_at_cursor");
    eOrientation orientation   = getDynamicOrientation();
    const auto   NODEIT        = std::ranges::find(m_masterNodesData, PNODE);

    bool         forceDropAsMaster = false;
    // if dragging window to move, drop it at the cursor position instead of bottom/top of stack
    if (*PDROPATCURSOR && g_layoutManager->dragController()->mode() == MBIND_MOVE) {
        if (WINDOWSONWORKSPACE > 2) {
            auto&             v = m_masterNodesData;

            const std::size_t srcIndex = static_cast<std::size_t>(std::distance(v.begin(), NODEIT));

            for (std::size_t i = 0; i < v.size(); ++i) {
                const CBox box = v[i]->pTarget->position();
                if (!box.containsPoint(MOUSECOORDS))
                    continue;

                std::size_t insertIndex = i;

                switch (orientation) {
                    case ORIENTATION_LEFT:
                    case ORIENTATION_RIGHT:
                        if (MOUSECOORDS.y > box.middle().y)
                            ++insertIndex; // insert after
                        break;

                    case ORIENTATION_TOP:
                    case ORIENTATION_BOTTOM:
                        if (MOUSECOORDS.x > box.middle().x)
                            ++insertIndex; // insert after
                        break;

                    case ORIENTATION_CENTER: break;

                    default: UNREACHABLE();
                }

                if (insertIndex > srcIndex)
                    --insertIndex;

                if (insertIndex == srcIndex)
                    break;

                auto node = std::move(v[srcIndex]);
                v.erase(v.begin() + static_cast<std::ptrdiff_t>(srcIndex));
                v.insert(v.begin() + static_cast<std::ptrdiff_t>(insertIndex), std::move(node));

                break;
            }
        } else if (WINDOWSONWORKSPACE == 2) {
            // when dropping as the second tiled window in the workspace,
            // make it the master only if the cursor is on the master side of the screen
            for (auto const& nd : m_masterNodesData) {
                if (nd->isMaster) {
                    const auto MIDDLE = nd->pTarget->position().middle();
                    switch (orientation) {
                        case ORIENTATION_LEFT:
                        case ORIENTATION_CENTER:
                            if (MOUSECOORDS.x < MIDDLE.x)
                                forceDropAsMaster = true;
                            break;
                        case ORIENTATION_RIGHT:
                            if (MOUSECOORDS.x > MIDDLE.x)
                                forceDropAsMaster = true;
                            break;
                        case ORIENTATION_TOP:
                            if (MOUSECOORDS.y < MIDDLE.y)
                                forceDropAsMaster = true;
                            break;
                        case ORIENTATION_BOTTOM:
                            if (MOUSECOORDS.y > MIDDLE.y)
                                forceDropAsMaster = true;
                            break;
                        default: UNREACHABLE();
                    }
                    break;
                }
            }
        }
    }

    if ((BNEWISMASTER && g_layoutManager->dragController()->mode() != MBIND_MOVE)    //
        || WINDOWSONWORKSPACE == 1                                                   //
        || (WINDOWSONWORKSPACE > 2 && !firstMap && OPENINGON && OPENINGON->isMaster) //
        || forceDropAsMaster                                                         //
        || (*PNEWSTATUS == "inherit" && OPENINGON && OPENINGON->isMaster && g_layoutManager->dragController()->mode() != MBIND_MOVE)) {

        if (BNEWBEFOREACTIVE) {
            for (auto& nd : m_masterNodesData | std::views::reverse) {
                if (nd->isMaster) {
                    nd->isMaster     = false;
                    lastSplitPercent = nd->percMaster;
                    break;
                }
            }
        } else {
            for (auto& nd : m_masterNodesData) {
                if (nd->isMaster) {
                    nd->isMaster     = false;
                    lastSplitPercent = nd->percMaster;
                    break;
                }
            }
        }

        PNODE->isMaster   = true;
        PNODE->percMaster = lastSplitPercent;

        // first, check if it isn't too big.
        if (const auto MAXSIZE = target->maxSize().value_or(Math::VECTOR2D_MAX); MAXSIZE.x < PMONITOR->m_size.x * lastSplitPercent || MAXSIZE.y < PMONITOR->m_size.y) {
            // we can't continue. make it floating.
            std::erase(m_masterNodesData, PNODE);
            m_parent->setFloating(target, true);
            return;
        }
    } else {
        PNODE->isMaster   = false;
        PNODE->percMaster = lastSplitPercent;

        // first, check if it isn't too big.
        if (const auto MAXSIZE = target->maxSize().value_or(Math::VECTOR2D_MAX);
            MAXSIZE.x < PMONITOR->m_size.x * (1 - lastSplitPercent) || MAXSIZE.y < PMONITOR->m_size.y * (1.f / (WINDOWSONWORKSPACE - 1))) {
            // we can't continue. make it floating.
            std::erase(m_masterNodesData, PNODE);
            m_parent->setFloating(target, true);
            return;
        }
    }

    // recalc
    calculateWorkspace();
}

void CMasterAlgorithm::removeTarget(SP<ITarget> target) {
    const auto  MASTERSLEFT = getMastersNo();
    static auto SMALLSPLIT  = CConfigValue<Hyprlang::INT>("master:allow_small_split");

    const auto  PNODE = getNodeFromTarget(target);

    if (target->fullscreenMode() != FSMODE_NONE)
        g_pCompositor->setWindowFullscreenInternal(target->window(), FSMODE_NONE);

    if (PNODE->isMaster && (MASTERSLEFT <= 1 || *SMALLSPLIT == 1)) {
        // find a new master from top of the list
        for (auto& nd : m_masterNodesData) {
            if (!nd->isMaster) {
                nd->isMaster   = true;
                nd->percMaster = PNODE->percMaster;
                break;
            }
        }
    }

    std::erase(m_masterNodesData, PNODE);

    if (getMastersNo() == getNodesNo() && MASTERSLEFT > 1) {
        for (auto& nd : m_masterNodesData | std::views::reverse) {
            nd->isMaster = false;
            break;
        }
    }
    // BUGFIX: correct bug where closing one master in a stack of 2 would leave
    // the screen half bare, and make it difficult to select remaining window
    if (getNodesNo() == 1) {
        for (auto& nd : m_masterNodesData) {
            if (!nd->isMaster) {
                nd->isMaster = true;
                break;
            }
        }
    }

    calculateWorkspace();
}

void CMasterAlgorithm::resizeTarget(const Vector2D& Δ, SP<ITarget> target, eRectCorner corner) {
    ;
}

void CMasterAlgorithm::swapTargets(SP<ITarget> a, SP<ITarget> b) {
    auto nodeA = getNodeFromTarget(a);
    auto nodeB = getNodeFromTarget(b);

    if (nodeA)
        nodeA->pTarget = b;
    if (nodeB)
        nodeB->pTarget = a;
}

void CMasterAlgorithm::moveTargetInDirection(SP<ITarget> t, Layout::eDirection dir, bool silent) {
    const auto PWINDOW2 = g_pCompositor->getWindowInDirection(t->window(), directionToChar(dir));

    if (!PWINDOW2 || !t->window())
        return;

    t->window()->setAnimationsToMove();

    if (t->window()->m_workspace != PWINDOW2->m_workspace) {
        // can't
        return;
    } else {
        // if same monitor, switch windows
        g_layoutManager->switchTargets(t, PWINDOW2->layoutTarget());
        if (silent)
            Desktop::focusState()->fullWindowFocus(PWINDOW2);
    }
}

void CMasterAlgorithm::recalculate() {
    calculateWorkspace();
}

std::expected<void, std::string> CMasterAlgorithm::layoutMsg(const std::string_view& sv) {
    auto switchToWindow = [&](SP<ITarget> target) {
        if (!validMapped(target->window()))
            return;

        Desktop::focusState()->fullWindowFocus(target->window());
        g_pCompositor->warpCursorTo(target->position().middle());

        g_pInputManager->m_forcedFocus = target->window();
        g_pInputManager->simulateMouseMovement();
        g_pInputManager->m_forcedFocus.reset();
    };

    CVarList2 vars(std::string{sv}, 0, 's');

    if (vars.size() < 1 || vars[0].empty()) {
        Log::logger->log(Log::ERR, "layoutmsg called without params");
        return std::unexpected("layoutmsg without params");
    }

    auto command = vars[0];

    // swapwithmaster <master | child | auto> <ignoremaster>
    // first message argument can have the following values:
    // * master - keep the focus at the new master
    // * child - keep the focus at the new child
    // * auto (default) - swap the focus (keep the focus of the previously selected window)
    // * ignoremaster - ignore if master is focused

    const auto PWINDOW = Desktop::focusState()->window();

    if (command == "swapwithmaster") {
        if (!PWINDOW)
            return std::unexpected("No focused window");

        if (!isWindowTiled(PWINDOW))
            return std::unexpected("focused window isn't tiled");

        const auto PMASTER = getMasterNode();

        if (!PMASTER)
            return std::unexpected("no master node");

        const auto NEWCHILD = PMASTER->pTarget.lock();

        const bool IGNORE_IF_MASTER = vars.size() >= 2 && std::ranges::any_of(vars, [](const auto& e) { return e == "ignoremaster"; });

        if (PMASTER->pTarget.lock() != PWINDOW->layoutTarget()) {
            const auto& NEWMASTER       = PWINDOW->layoutTarget();
            const bool  newFocusToChild = vars.size() >= 2 && vars[1] == "child";
            g_layoutManager->switchTargets(NEWMASTER, NEWCHILD);
            const auto NEWFOCUS = newFocusToChild ? NEWCHILD : NEWMASTER;
            switchToWindow(NEWFOCUS);
        } else if (!IGNORE_IF_MASTER) {
            for (auto const& n : m_masterNodesData) {
                if (!n->isMaster) {
                    const auto NEWMASTER = n->pTarget.lock();
                    g_layoutManager->switchTargets(NEWMASTER, NEWCHILD);
                    const bool newFocusToMaster = vars.size() >= 2 && vars[1] == "master";
                    const auto NEWFOCUS         = newFocusToMaster ? NEWMASTER : NEWCHILD;
                    switchToWindow(NEWFOCUS);
                    break;
                }
            }
        }

        return {};
    }
    // focusmaster <master | previous | auto>
    // first message argument can have the following values:
    // * master - keep the focus at the new master, even if it was focused before
    // * previous - focus window which was previously switched from using `focusmaster previous` command, otherwise fallback to `auto`
    // * auto (default) - swap the focus with the first child, if the current focus was master, otherwise focus master
    else if (command == "focusmaster") {
        if (!PWINDOW)
            return std::unexpected("no focused window");

        const auto PMASTER = getMasterNode();

        if (!PMASTER)
            return std::unexpected("no master");

        const auto& ARG = vars[1]; // returns empty string if out of bounds

        if (PMASTER->pTarget.lock() != PWINDOW->layoutTarget()) {
            switchToWindow(PMASTER->pTarget.lock());
            // save previously focused window (only for `previous` mode)
            if (ARG == "previous")
                m_workspaceData.focusMasterPrev = PWINDOW->layoutTarget();
            return {};
        }

        const auto focusAuto = [&]() {
            // focus first non-master window
            for (auto const& n : m_masterNodesData) {
                if (!n->isMaster) {
                    switchToWindow(n->pTarget.lock());
                    break;
                }
            }
        };

        if (ARG == "master")
            return {};
        // switch to previously saved window
        else if (ARG == "previous") {
            const auto PREVWINDOW = m_workspaceData.focusMasterPrev.lock();
            const bool VALID      = validMapped(PREVWINDOW->window()) && PWINDOW != PREVWINDOW;
            VALID ? switchToWindow(PREVWINDOW) : focusAuto();
        } else
            focusAuto();
    } else if (command == "cyclenext") {
        if (!PWINDOW)
            return std::unexpected("no window");

        const bool NOLOOP      = vars.size() >= 2 && vars[1] == "noloop";
        const auto PNEXTWINDOW = getNextTarget(PWINDOW->layoutTarget(), true, !NOLOOP);
        switchToWindow(PNEXTWINDOW);
    } else if (command == "cycleprev") {
        if (!PWINDOW)
            return std::unexpected("no window");

        const bool NOLOOP      = vars.size() >= 2 && vars[1] == "noloop";
        const auto PPREVWINDOW = getNextTarget(PWINDOW->layoutTarget(), false, !NOLOOP);
        switchToWindow(PPREVWINDOW);
    } else if (command == "swapnext") {
        if (!validMapped(PWINDOW))
            return std::unexpected("no window");

        if (PWINDOW->layoutTarget()->floating()) {
            g_pKeybindManager->m_dispatchers["swapnext"]("");
            return {};
        }

        const bool NOLOOP            = vars.size() >= 2 && vars[1] == "noloop";
        const auto PWINDOWTOSWAPWITH = getNextTarget(PWINDOW->layoutTarget(), true, !NOLOOP);

        if (PWINDOWTOSWAPWITH) {
            g_pCompositor->setWindowFullscreenInternal(PWINDOW, FSMODE_NONE);
            g_layoutManager->switchTargets(PWINDOW->layoutTarget(), PWINDOWTOSWAPWITH);
            switchToWindow(PWINDOW->layoutTarget());
        }
    } else if (command == "swapprev") {
        if (!validMapped(PWINDOW))
            return std::unexpected("no window");

        if (PWINDOW->layoutTarget()->floating()) {
            g_pKeybindManager->m_dispatchers["swapnext"]("prev");
            return {};
        }

        const bool NOLOOP            = vars.size() >= 2 && vars[1] == "noloop";
        const auto PWINDOWTOSWAPWITH = getNextTarget(PWINDOW->layoutTarget(), false, !NOLOOP);

        if (PWINDOWTOSWAPWITH) {
            g_pCompositor->setWindowFullscreenInternal(PWINDOW, FSMODE_NONE);
            g_layoutManager->switchTargets(PWINDOW->layoutTarget(), PWINDOWTOSWAPWITH);
            switchToWindow(PWINDOW->layoutTarget());
        }
    } else if (command == "addmaster") {
        if (!validMapped(PWINDOW))
            return std::unexpected("no window");

        if (PWINDOW->layoutTarget()->floating())
            return std::unexpected("window is floating");

        const auto  PNODE = getNodeFromTarget(PWINDOW->layoutTarget());

        const auto  WINDOWS    = getNodesNo();
        const auto  MASTERS    = getMastersNo();
        static auto SMALLSPLIT = CConfigValue<Hyprlang::INT>("master:allow_small_split");

        if (MASTERS + 2 > WINDOWS && *SMALLSPLIT == 0)
            return std::unexpected("nothing to do");

        g_pCompositor->setWindowFullscreenInternal(PWINDOW, FSMODE_NONE);

        if (!PNODE || PNODE->isMaster) {
            // first non-master node
            for (auto& n : m_masterNodesData) {
                if (!n->isMaster) {
                    n->isMaster = true;
                    break;
                }
            }
        } else {
            PNODE->isMaster = true;
        }

        calculateWorkspace();

    } else if (command == "removemaster") {

        if (!validMapped(PWINDOW))
            return std::unexpected("no window");

        if (PWINDOW->layoutTarget()->floating())
            return std::unexpected("window isnt tiled");

        const auto PNODE = getNodeFromTarget(PWINDOW->layoutTarget());

        const auto WINDOWS = getNodesNo();
        const auto MASTERS = getMastersNo();

        if (WINDOWS < 2 || MASTERS < 2)
            return std::unexpected("nothing to do");

        g_pCompositor->setWindowFullscreenInternal(PWINDOW, FSMODE_NONE);

        if (!PNODE || !PNODE->isMaster) {
            // first non-master node
            for (auto& nd : m_masterNodesData | std::views::reverse) {
                if (nd->isMaster) {
                    nd->isMaster = false;
                    break;
                }
            }
        } else {
            PNODE->isMaster = false;
        }

        calculateWorkspace();
    } else if (command == "orientationleft" || command == "orientationright" || command == "orientationtop" || command == "orientationbottom" || command == "orientationcenter") {
        if (!PWINDOW)
            return std::unexpected("no window");

        g_pCompositor->setWindowFullscreenInternal(PWINDOW, FSMODE_NONE);

        if (command == "orientationleft")
            m_workspaceData.orientation = ORIENTATION_LEFT;
        else if (command == "orientationright")
            m_workspaceData.orientation = ORIENTATION_RIGHT;
        else if (command == "orientationtop")
            m_workspaceData.orientation = ORIENTATION_TOP;
        else if (command == "orientationbottom")
            m_workspaceData.orientation = ORIENTATION_BOTTOM;
        else if (command == "orientationcenter")
            m_workspaceData.orientation = ORIENTATION_CENTER;

        calculateWorkspace();
    } else if (command == "orientationnext") {
        runOrientationCycle(nullptr, 1);
    } else if (command == "orientationprev") {
        runOrientationCycle(nullptr, -1);
    } else if (command == "orientationcycle") {
        runOrientationCycle(&vars, 1);
    } else if (command == "mfact") {
        g_pKeybindManager->m_dispatchers["splitratio"](std::format("{} {}", vars[1], vars[2]));
    } else if (command == "rollnext") {
        const auto PNODE = getNodeFromWindow(PWINDOW);

        if (!PNODE)
            return std::unexpected("window couldnt be found");

        const auto OLDMASTER = PNODE->isMaster ? PNODE : getMasterNode();
        if (!OLDMASTER)
            return std::unexpected("no old master");

        auto oldMasterIt = std::ranges::find(m_masterNodesData, OLDMASTER);

        for (auto& nd : m_masterNodesData) {
            if (!nd->isMaster) {
                const auto newMaster = nd;
                newMaster->isMaster  = true;

                auto newMasterIt = std::ranges::find(m_masterNodesData, newMaster);

                if (newMasterIt < oldMasterIt)
                    std::ranges::rotate(newMasterIt, std::next(newMasterIt), oldMasterIt);
                else if (newMasterIt > oldMasterIt)
                    std::ranges::rotate(oldMasterIt, newMasterIt, std::next(newMasterIt));

                switchToWindow(newMaster->pTarget.lock());
                OLDMASTER->isMaster = false;

                oldMasterIt = std::ranges::find(m_masterNodesData, OLDMASTER);
                if (oldMasterIt != m_masterNodesData.end())
                    std::ranges::rotate(oldMasterIt, std::next(oldMasterIt), m_masterNodesData.end());

                break;
            }
        }

        calculateWorkspace();
    } else if (command == "rollprev") {
        const auto PNODE = getNodeFromWindow(PWINDOW);

        if (!PNODE)
            return std::unexpected("window couldnt be found");

        const auto OLDMASTER = PNODE->isMaster ? PNODE : getMasterNode();
        if (!OLDMASTER)
            return std::unexpected("no old master");

        auto oldMasterIt = std::ranges::find(m_masterNodesData, OLDMASTER);

        for (auto& nd : m_masterNodesData | std::views::reverse) {
            if (!nd->isMaster) {
                const auto newMaster = nd;
                newMaster->isMaster  = true;

                auto newMasterIt = std::ranges::find(m_masterNodesData, newMaster);

                if (newMasterIt < oldMasterIt)
                    std::ranges::rotate(newMasterIt, std::next(newMasterIt), oldMasterIt);
                else if (newMasterIt > oldMasterIt)
                    std::ranges::rotate(oldMasterIt, newMasterIt, std::next(newMasterIt));

                switchToWindow(newMaster->pTarget.lock());
                OLDMASTER->isMaster = false;

                oldMasterIt = std::ranges::find(m_masterNodesData, OLDMASTER);
                if (oldMasterIt != m_masterNodesData.begin())
                    std::ranges::rotate(m_masterNodesData.begin(), oldMasterIt, std::next(oldMasterIt));

                break;
            }
        }

        calculateWorkspace();
    }

    return {};
}

std::optional<Vector2D> CMasterAlgorithm::predictSizeForNewTarget() {
    static auto PNEWSTATUS = CConfigValue<std::string>("master:new_status");

    const auto  MONITOR = m_parent->space()->workspace()->m_monitor;

    if (!MONITOR)
        return std::nullopt;

    const int NODES = getNodesNo();

    if (NODES <= 0)
        return Desktop::focusState()->monitor()->m_size;

    const auto MASTER = getMasterNode();
    if (!MASTER) // wtf
        return std::nullopt;

    if (*PNEWSTATUS == "master") {
        return MASTER->size;
    } else {
        const auto SLAVES = NODES - getMastersNo();

        // TODO: make this better
        return Vector2D{MONITOR->m_size.x - MASTER->size.x, MONITOR->m_size.y / (SLAVES + 1)};
    }

    return std::nullopt;
}

void CMasterAlgorithm::buildOrientationCycleVectorFromVars(std::vector<eOrientation>& cycle, Hyprutils::String::CVarList2* vars) {
    for (size_t i = 1; i < vars->size(); ++i) {
        if ((*vars)[i] == "top") {
            cycle.emplace_back(ORIENTATION_TOP);
        } else if ((*vars)[i] == "right") {
            cycle.emplace_back(ORIENTATION_RIGHT);
        } else if ((*vars)[i] == "bottom") {
            cycle.emplace_back(ORIENTATION_BOTTOM);
        } else if ((*vars)[i] == "left") {
            cycle.emplace_back(ORIENTATION_LEFT);
        } else if ((*vars)[i] == "center") {
            cycle.emplace_back(ORIENTATION_CENTER);
        }
    }
}

void CMasterAlgorithm::buildOrientationCycleVectorFromEOperation(std::vector<eOrientation>& cycle) {
    for (int i = 0; i <= ORIENTATION_CENTER; ++i) {
        cycle.push_back(sc<eOrientation>(i));
    }
}

void CMasterAlgorithm::runOrientationCycle(Hyprutils::String::CVarList2* vars, int next) {
    std::vector<eOrientation> cycle;
    if (vars != nullptr)
        buildOrientationCycleVectorFromVars(cycle, vars);

    if (cycle.empty())
        buildOrientationCycleVectorFromEOperation(cycle);

    const auto PWINDOW = Desktop::focusState()->window();

    if (!PWINDOW)
        return;

    g_pCompositor->setWindowFullscreenInternal(PWINDOW, FSMODE_NONE);

    int nextOrPrev = 0;
    for (size_t i = 0; i < cycle.size(); ++i) {
        if (m_workspaceData.orientation == cycle[i]) {
            nextOrPrev = i + next;
            break;
        }
    }

    if (nextOrPrev >= sc<int>(cycle.size()))
        nextOrPrev = nextOrPrev % sc<int>(cycle.size());
    else if (nextOrPrev < 0)
        nextOrPrev = cycle.size() + (nextOrPrev % sc<int>(cycle.size()));

    m_workspaceData.orientation = cycle.at(nextOrPrev);
    calculateWorkspace();
}

eOrientation CMasterAlgorithm::getDynamicOrientation() {
    const auto  WORKSPACERULE = g_pConfigManager->getWorkspaceRuleFor(m_parent->space()->workspace());
    std::string orientationString;
    if (WORKSPACERULE.layoutopts.contains("orientation"))
        orientationString = WORKSPACERULE.layoutopts.at("orientation");

    eOrientation orientation = m_workspaceData.orientation;
    // override if workspace rule is set
    if (!orientationString.empty()) {
        if (orientationString == "top")
            orientation = ORIENTATION_TOP;
        else if (orientationString == "right")
            orientation = ORIENTATION_RIGHT;
        else if (orientationString == "bottom")
            orientation = ORIENTATION_BOTTOM;
        else if (orientationString == "center")
            orientation = ORIENTATION_CENTER;
        else
            orientation = ORIENTATION_LEFT;
    }

    return orientation;
}

int CMasterAlgorithm::getNodesNo() {
    return m_masterNodesData.size();
}

SP<SMasterNodeData> CMasterAlgorithm::getNodeFromWindow(PHLWINDOW x) {
    return getNodeFromTarget(x->layoutTarget());
}

SP<SMasterNodeData> CMasterAlgorithm::getNodeFromTarget(SP<ITarget> x) {
    for (const auto& n : m_masterNodesData) {
        if (n->pTarget == x)
            return n;
    }

    return nullptr;
}

SP<SMasterNodeData> CMasterAlgorithm::getMasterNode() {
    for (const auto& n : m_masterNodesData) {
        if (n->isMaster)
            return n;
    }

    return nullptr;
}

void CMasterAlgorithm::calculateWorkspace() {
    const auto PMASTERNODE = getMasterNode();

    if (!PMASTERNODE)
        return;

    eOrientation orientation         = getDynamicOrientation();
    bool         centerMasterWindow  = false;
    static auto  SLAVECOUNTFORCENTER = CConfigValue<Hyprlang::INT>("master:slave_count_for_center_master");
    static auto  CMFALLBACK          = CConfigValue<std::string>("master:center_master_fallback");
    static auto  PIGNORERESERVED     = CConfigValue<Hyprlang::INT>("master:center_ignores_reserved");
    static auto  PSMARTRESIZING      = CConfigValue<Hyprlang::INT>("master:smart_resizing");

    const auto   MASTERS          = getMastersNo();
    const auto   WINDOWS          = getNodesNo();
    const auto   STACKWINDOWS     = WINDOWS - MASTERS;
    const auto   WORKAREA         = m_parent->space()->workArea();
    const auto   PMONITOR         = m_parent->space()->workspace()->m_monitor;
    const auto   UNRESERVED_WIDTH = WORKAREA.width + PMONITOR->m_reservedArea.left() + PMONITOR->m_reservedArea.right();

    if (orientation == ORIENTATION_CENTER) {
        if (STACKWINDOWS >= *SLAVECOUNTFORCENTER)
            centerMasterWindow = true;
        else {
            if (*CMFALLBACK == "left")
                orientation = ORIENTATION_LEFT;
            else if (*CMFALLBACK == "right")
                orientation = ORIENTATION_RIGHT;
            else if (*CMFALLBACK == "top")
                orientation = ORIENTATION_TOP;
            else if (*CMFALLBACK == "bottom")
                orientation = ORIENTATION_BOTTOM;
            else
                orientation = ORIENTATION_LEFT;
        }
    }

    const float totalSize             = (orientation == ORIENTATION_TOP || orientation == ORIENTATION_BOTTOM) ? WORKAREA.w : WORKAREA.h;
    const float masterAverageSize     = totalSize / MASTERS;
    const float slaveAverageSize      = totalSize / STACKWINDOWS;
    float       masterAccumulatedSize = 0;
    float       slaveAccumulatedSize  = 0;

    if (*PSMARTRESIZING) {
        // check the total width and height so that later
        // if larger/smaller than screen size them down/up
        for (auto const& nd : m_masterNodesData) {
            if (nd->isMaster)
                masterAccumulatedSize += totalSize / MASTERS * nd->percSize;
            else
                slaveAccumulatedSize += totalSize / STACKWINDOWS * nd->percSize;
        }
    }

    // compute placement of master window(s)
    if (WINDOWS == 1 && !centerMasterWindow) {
        static auto PALWAYSKEEPPOSITION = CConfigValue<Hyprlang::INT>("master:always_keep_position");
        if (*PALWAYSKEEPPOSITION) {
            const float WIDTH = WORKAREA.w * PMASTERNODE->percMaster;
            float       nextX = 0;

            if (orientation == ORIENTATION_RIGHT)
                nextX = WORKAREA.w - WIDTH;
            else if (orientation == ORIENTATION_CENTER)
                nextX = (WORKAREA.w - WIDTH) / 2;

            PMASTERNODE->size     = Vector2D(WIDTH, WORKAREA.h);
            PMASTERNODE->position = WORKAREA.pos() + Vector2D(nextX, 0.0);
        } else {
            PMASTERNODE->size     = WORKAREA.size();
            PMASTERNODE->position = WORKAREA.pos();
        }

        PMASTERNODE->pTarget->setPositionGlobal({PMASTERNODE->position, PMASTERNODE->size});
        return;
    } else if (orientation == ORIENTATION_TOP || orientation == ORIENTATION_BOTTOM) {
        const float HEIGHT      = STACKWINDOWS != 0 ? WORKAREA.h * PMASTERNODE->percMaster : WORKAREA.h;
        float       widthLeft   = WORKAREA.w;
        int         mastersLeft = MASTERS;
        float       nextX       = 0;
        float       nextY       = 0;

        if (orientation == ORIENTATION_BOTTOM)
            nextY = WORKAREA.h - HEIGHT;

        for (auto& nd : m_masterNodesData) {
            if (!nd->isMaster)
                continue;

            float WIDTH = mastersLeft > 1 ? widthLeft / mastersLeft * nd->percSize : widthLeft;
            if (WIDTH > widthLeft * 0.9f && mastersLeft > 1)
                WIDTH = widthLeft * 0.9f;

            if (*PSMARTRESIZING) {
                nd->percSize *= WORKAREA.w / masterAccumulatedSize;
                WIDTH = masterAverageSize * nd->percSize;
            }

            nd->size     = Vector2D(WIDTH, HEIGHT);
            nd->position = WORKAREA.pos() + Vector2D(nextX, nextY);
            nd->pTarget->setPositionGlobal({nd->position, nd->size});

            mastersLeft--;
            widthLeft -= WIDTH;
            nextX += WIDTH;
        }
    } else { // orientation left, right or center
        const float TOTAL_WIDTH = *PIGNORERESERVED && centerMasterWindow ? UNRESERVED_WIDTH : WORKAREA.w;
        float       WIDTH       = TOTAL_WIDTH;
        float       heightLeft  = WORKAREA.h;
        int         mastersLeft = MASTERS;
        float       nextX       = 0;
        float       nextY       = 0;

        if (STACKWINDOWS > 0 || centerMasterWindow)
            WIDTH *= PMASTERNODE->percMaster;

        if (orientation == ORIENTATION_RIGHT)
            nextX = WORKAREA.w - WIDTH;
        else if (centerMasterWindow)
            nextX += (TOTAL_WIDTH - WIDTH) / 2;

        for (auto& nd : m_masterNodesData) {
            if (!nd->isMaster)
                continue;

            float HEIGHT = mastersLeft > 1 ? heightLeft / mastersLeft * nd->percSize : heightLeft;
            if (HEIGHT > heightLeft * 0.9f && mastersLeft > 1)
                HEIGHT = heightLeft * 0.9f;

            if (*PSMARTRESIZING) {
                nd->percSize *= WORKAREA.h / masterAccumulatedSize;
                HEIGHT = masterAverageSize * nd->percSize;
            }

            nd->size     = Vector2D(WIDTH, HEIGHT);
            nd->position = (*PIGNORERESERVED && centerMasterWindow ? WORKAREA.pos() - Vector2D(PMONITOR->m_reservedArea.left(), 0.0) : WORKAREA.pos()) + Vector2D(nextX, nextY);
            nd->pTarget->setPositionGlobal({nd->position, nd->size});

            mastersLeft--;
            heightLeft -= HEIGHT;
            nextY += HEIGHT;
        }
    }

    if (STACKWINDOWS == 0)
        return;

    // compute placement of slave window(s)
    int slavesLeft = STACKWINDOWS;
    if (orientation == ORIENTATION_TOP || orientation == ORIENTATION_BOTTOM) {
        const float HEIGHT    = WORKAREA.h - PMASTERNODE->size.y;
        float       widthLeft = WORKAREA.w;
        float       nextX     = 0;
        float       nextY     = 0;

        if (orientation == ORIENTATION_TOP)
            nextY = PMASTERNODE->size.y;

        for (auto& nd : m_masterNodesData) {
            if (nd->isMaster)
                continue;

            float WIDTH = slavesLeft > 1 ? widthLeft / slavesLeft * nd->percSize : widthLeft;
            if (WIDTH > widthLeft * 0.9f && slavesLeft > 1)
                WIDTH = widthLeft * 0.9f;

            if (*PSMARTRESIZING) {
                nd->percSize *= WORKAREA.w / slaveAccumulatedSize;
                WIDTH = slaveAverageSize * nd->percSize;
            }

            nd->size     = Vector2D(WIDTH, HEIGHT);
            nd->position = WORKAREA.pos() + Vector2D(nextX, nextY);
            nd->pTarget->setPositionGlobal({nd->position, nd->size});

            slavesLeft--;
            widthLeft -= WIDTH;
            nextX += WIDTH;
        }
    } else if (orientation == ORIENTATION_LEFT || orientation == ORIENTATION_RIGHT) {
        const float WIDTH      = WORKAREA.w - PMASTERNODE->size.x;
        float       heightLeft = WORKAREA.h;
        float       nextY      = 0;
        float       nextX      = 0;

        if (orientation == ORIENTATION_LEFT)
            nextX = PMASTERNODE->size.x;

        for (auto& nd : m_masterNodesData) {
            if (nd->isMaster)
                continue;

            float HEIGHT = slavesLeft > 1 ? heightLeft / slavesLeft * nd->percSize : heightLeft;
            if (HEIGHT > heightLeft * 0.9f && slavesLeft > 1)
                HEIGHT = heightLeft * 0.9f;

            if (*PSMARTRESIZING) {
                nd->percSize *= WORKAREA.h / slaveAccumulatedSize;
                HEIGHT = slaveAverageSize * nd->percSize;
            }

            nd->size     = Vector2D(WIDTH, HEIGHT);
            nd->position = WORKAREA.pos() + Vector2D(nextX, nextY);
            nd->pTarget->setPositionGlobal({nd->position, nd->size});

            slavesLeft--;
            heightLeft -= HEIGHT;
            nextY += HEIGHT;
        }
    } else { // slaves for centered master window(s)
        const float WIDTH       = ((*PIGNORERESERVED ? UNRESERVED_WIDTH : WORKAREA.w) - PMASTERNODE->size.x) / 2.0;
        float       heightLeft  = 0;
        float       heightLeftL = WORKAREA.h;
        float       heightLeftR = WORKAREA.h;
        float       nextX       = 0;
        float       nextY       = 0;
        float       nextYL      = 0;
        float       nextYR      = 0;
        bool        onRight     = *CMFALLBACK == "right";
        int         slavesLeftL = 1 + (slavesLeft - 1) / 2;
        int         slavesLeftR = slavesLeft - slavesLeftL;

        if (onRight) {
            slavesLeftR = 1 + (slavesLeft - 1) / 2;
            slavesLeftL = slavesLeft - slavesLeftR;
        }

        const float slaveAverageHeightL     = WORKAREA.h / slavesLeftL;
        const float slaveAverageHeightR     = WORKAREA.h / slavesLeftR;
        float       slaveAccumulatedHeightL = 0;
        float       slaveAccumulatedHeightR = 0;

        if (*PSMARTRESIZING) {
            for (auto const& nd : m_masterNodesData) {
                if (nd->isMaster)
                    continue;

                if (onRight)
                    slaveAccumulatedHeightR += slaveAverageHeightR * nd->percSize;
                else
                    slaveAccumulatedHeightL += slaveAverageHeightL * nd->percSize;

                onRight = !onRight;
            }

            onRight = *CMFALLBACK == "right";
        }

        for (auto& nd : m_masterNodesData) {
            if (nd->isMaster)
                continue;

            if (onRight) {
                nextX      = WIDTH + PMASTERNODE->size.x - (*PIGNORERESERVED ? PMONITOR->m_reservedArea.left() : 0);
                nextY      = nextYR;
                heightLeft = heightLeftR;
                slavesLeft = slavesLeftR;
            } else {
                nextX      = 0;
                nextY      = nextYL;
                heightLeft = heightLeftL;
                slavesLeft = slavesLeftL;
            }

            float HEIGHT = slavesLeft > 1 ? heightLeft / slavesLeft * nd->percSize : heightLeft;
            if (HEIGHT > heightLeft * 0.9f && slavesLeft > 1)
                HEIGHT = heightLeft * 0.9f;

            if (*PSMARTRESIZING) {
                if (onRight) {
                    nd->percSize *= WORKAREA.h / slaveAccumulatedHeightR;
                    HEIGHT = slaveAverageHeightR * nd->percSize;
                } else {
                    nd->percSize *= WORKAREA.h / slaveAccumulatedHeightL;
                    HEIGHT = slaveAverageHeightL * nd->percSize;
                }
            }

            nd->size     = Vector2D(*PIGNORERESERVED ? (WIDTH - (onRight ? PMONITOR->m_reservedArea.right() : PMONITOR->m_reservedArea.left())) : WIDTH, HEIGHT);
            nd->position = WORKAREA.pos() + Vector2D(nextX, nextY);
            nd->pTarget->setPositionGlobal({nd->position, nd->size});

            if (onRight) {
                heightLeftR -= HEIGHT;
                nextYR += HEIGHT;
                slavesLeftR--;
            } else {
                heightLeftL -= HEIGHT;
                nextYL += HEIGHT;
                slavesLeftL--;
            }

            onRight = !onRight;
        }
    }
}

SP<ITarget> CMasterAlgorithm::getNextCandidate(SP<ITarget> old) {
    const auto MIDDLE = old->position().middle();

    if (const auto NODE = getClosestNode(MIDDLE); NODE)
        return NODE->pTarget.lock();

    if (const auto NODE = getMasterNode(); NODE)
        return NODE->pTarget.lock();

    return nullptr;
}

SP<ITarget> CMasterAlgorithm::getNextTarget(SP<ITarget> t, bool next, bool loop) {
    if (t->floating())
        return nullptr;

    const auto PNODE = getNodeFromTarget(t);

    auto       nodes = m_masterNodesData;
    if (!next)
        std::ranges::reverse(nodes);

    const auto NODEIT = std::ranges::find(nodes, PNODE);

    const bool ISMASTER = PNODE->isMaster;

    auto       CANDIDATE = std::find_if(NODEIT, nodes.end(), [&](const auto& other) { return other != PNODE && ISMASTER == other->isMaster; });
    if (CANDIDATE == nodes.end())
        CANDIDATE = std::ranges::find_if(nodes, [&](const auto& other) { return other != PNODE && ISMASTER != other->isMaster; });

    if (CANDIDATE != nodes.end() && !loop) {
        if ((*CANDIDATE)->isMaster && next)
            return nullptr;
        if (!(*CANDIDATE)->isMaster && ISMASTER && !next)
            return nullptr;
    }

    return CANDIDATE == nodes.end() ? nullptr : (*CANDIDATE)->pTarget.lock();
}

int CMasterAlgorithm::getMastersNo() {
    return std::ranges::count_if(m_masterNodesData, [](const auto& n) { return n->isMaster; });
}

bool CMasterAlgorithm::isWindowTiled(PHLWINDOW x) {
    return x && !x->layoutTarget()->floating();
}

SP<SMasterNodeData> CMasterAlgorithm::getClosestNode(const Vector2D& point) {
    SP<SMasterNodeData> res         = nullptr;
    double              distClosest = -1;
    for (auto& n : m_masterNodesData) {
        if (n->pTarget && Desktop::View::validMapped(n->pTarget->window())) {
            auto distAnother = vecToRectDistanceSquared(point, n->position, n->position + n->size);
            if (!res || distAnother < distClosest) {
                res         = n;
                distClosest = distAnother;
            }
        }
    }
    return res;
}
