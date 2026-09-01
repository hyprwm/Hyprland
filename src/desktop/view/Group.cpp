#include "Group.hpp"
#include "window/Window.hpp"
#include "window/WindowGroupMembership.hpp"
#include "window/WindowPresentation.hpp"

#include "../../render/decorations/CHyprGroupBarDecoration.hpp"
#include "../../layout/target/WindowGroupTarget.hpp"
#include "../../layout/target/WindowTarget.hpp"
#include "../../layout/target/Target.hpp"
#include "../../layout/space/Space.hpp"
#include "../../layout/LayoutManager.hpp"
#include "../../desktop/state/FocusState.hpp"
#include "../../Compositor.hpp"
#include "../../ipc/s2/S2.hpp"
#include "../../managers/fullscreen/FullscreenController.hpp"

#include <algorithm>

using namespace Desktop;
using namespace Desktop::View;

std::vector<WP<CGroup>>& View::groups() {
    static std::vector<WP<CGroup>> g;
    return g;
}

SP<CGroup> CGroup::create(std::vector<PHLWINDOWREF>&& windows) {
    auto x      = SP<CGroup>(new CGroup(std::move(windows)));
    x->m_self   = x;
    x->m_target = Layout::CWindowGroupTarget::create(x);
    groups().emplace_back(x);

    x->init();

    return x;
}

CGroup::CGroup(std::vector<PHLWINDOWREF>&& windows) : m_windows(std::move(windows)) {
    ;
}

void CGroup::init() {
    // for proper group logic:
    //  - add all windows to us
    //  - replace the first window with our target
    //  - remove all window targets from layout
    //  - apply updates

    // FIXME: what if some windows are grouped? For now we only do 1-window but YNK
    for (const auto& w : m_windows) {
        RASSERT(!w->grouping().group(), "CGroup: windows cannot contain grouped in init, this will explode");
        w->grouping().attach(m_self.lock());
        m_groupPolicyFlags |= w->grouping().rules();
    }

    g_layoutManager->switchTargets(m_windows.at(0)->windowTarget(), m_target);

    for (const auto& w : m_windows) {
        w->windowTarget()->setSpaceGhost(m_target->space());
    }

    for (const auto& w : m_windows) {
        applyWindowDecosAndUpdates(w.lock());
    }

    updateWindowVisibility();

    IPC::Socket2::sock()->postEvent({.event = "togglegroup", .data = std::format("1,{:x}", rc<uintptr_t>(m_windows.at(0).get()))});
}

void CGroup::destroy() {
    const auto POWNER = head();

    while (!m_windows.empty()) {
        if (m_windows.size() == 1) {
            remove(m_windows.at(0).lock());
            break;
        }

        remove(m_windows.at(0).lock());
    }

    if (POWNER)
        IPC::Socket2::sock()->postEvent({.event = "togglegroup", .data = std::format("0,{:x}", rc<uintptr_t>(POWNER.get()))});
}

CGroup::~CGroup() {
    if (m_target->space())
        m_target->assignToSpace(nullptr);
    std::erase_if(groups(), [this](const auto& e) { return !e || e == m_self; });
}

bool CGroup::has(PHLWINDOW w) const {
    return std::ranges::contains(m_windows, w);
}

void CGroup::add(PHLWINDOW w, std::optional<size_t> index) {
    static auto INSERT_AFTER_CURRENT          = CConfigValue<Config::INTEGER>("group:insert_after_current");
    static auto PDISABLE                      = CConfigValue<Config::BOOL>("group:groupbar:disable_when_only");
    const auto  GROUPBAR_DISABLED_ONLY_MEMBER = (*PDISABLE && m_windows.size() == 1) ? m_windows.at(0).lock() : nullptr;

    if (w->grouping().group()) {
        if (w->grouping().group() == m_self)
            return;

        const auto WINDOWS = w->grouping().group()->windows();
        for (size_t i = 0; i < WINDOWS.size(); ++i) {
            const auto WINDOW = WINDOWS.at(i).lock();
            if (!WINDOW)
                continue;

            WINDOW->grouping().group()->remove(WINDOW);
            add(WINDOW, index ? std::optional(*index + i) : std::nullopt);
        }

        return;
    }

    const auto FS_INTERNAL_MODE            = m_target->window() ? Fullscreen::controller()->getFullscreenModes(m_target->window()).internal : Fullscreen::FSMODE_NONE;
    const auto OLD_FULLSCREEN_WINDOW       = FS_INTERNAL_MODE != Fullscreen::FSMODE_NONE ? current() : nullptr;
    const bool FS_WINDOW_IS_LAYOUT_HANDLED = FS_INTERNAL_MODE != Fullscreen::FSMODE_NONE ? Fullscreen::controller()->layoutManagedFS(m_target->window()) : false;

    if (Fullscreen::controller()->isFullscreen(w))
        Fullscreen::controller()->setFullscreenMode(w, Fullscreen::FSMODE_NONE);

    if (OLD_FULLSCREEN_WINDOW)
        Fullscreen::controller()->setFullscreenMode(OLD_FULLSCREEN_WINDOW, Fullscreen::FSMODE_NONE);

    if (w->layoutTarget()->space()) {
        // remove the target from a space if it is in one
        g_layoutManager->removeTarget(w->layoutTarget());
    }

    w->grouping().attach(m_self.lock());
    m_groupPolicyFlags |= w->grouping().rules();
    w->windowTarget()->setSpaceGhost(m_target->space());
    w->windowTarget()->setFloating(m_target->floating());

    // a window in a group lives on the group's monitor/workspace
    if (const auto WS = m_target->workspace(); WS && w->m_workspace != WS) {
        w->m_monitor = WS->m_monitor;
        w->moveToWorkspace(WS);
    }

    if (index) {
        m_current = std::min(*index, m_windows.size());
        m_windows.insert(m_windows.begin() + m_current, w);
    } else if (*INSERT_AFTER_CURRENT) {
        m_windows.insert(m_windows.begin() + m_current + 1, w);
        m_current++;
    } else {
        m_windows.emplace_back(w);
        m_current = m_windows.size() - 1;
    }

    applyWindowDecosAndUpdates(w);

    // when groupbar:disable_when_only = true, give the only member of the group its groupbar after adding the second member.
    if (GROUPBAR_DISABLED_ONLY_MEMBER)
        g_pDecorationPositioner->forceRecalcFor(GROUPBAR_DISABLED_ONLY_MEMBER);

    updateWindowVisibility();

    if (FS_INTERNAL_MODE != Fullscreen::FSMODE_NONE) {
        Fullscreen::controller()->setFullscreenMode(w, FS_INTERNAL_MODE, std::nullopt, FS_WINDOW_IS_LAYOUT_HANDLED);
        w->windowTarget()->warpPositionSize();

        if (OLD_FULLSCREEN_WINDOW)
            OLD_FULLSCREEN_WINDOW->windowTarget()->setPositionGlobal(w->windowTarget()->position());
    }

    m_target->recalc();
}

void CGroup::replaceMember(PHLWINDOW oldWindow, PHLWINDOW newWindow, std::optional<Fullscreen::eFullscreenMode> internalMode, bool layoutManaged) {
    if (!oldWindow || !newWindow || oldWindow == newWindow)
        return;

    const auto ITR = std::ranges::find(m_windows, oldWindow);
    if (ITR == m_windows.end() || newWindow->grouping().group() == m_self)
        return;

    const auto SELF = m_self.lock();
    const auto IDX  = sc<size_t>(std::distance(m_windows.begin(), ITR));

    if (const auto SOURCE_GROUP = newWindow->grouping().group())
        SOURCE_GROUP->remove(newWindow);

    const auto FS_INTERNAL_MODE = internalMode.value_or(Fullscreen::controller()->getFullscreenModes(oldWindow).internal);
    const bool HAD_FULLSCREEN   = FS_INTERNAL_MODE != Fullscreen::FSMODE_NONE;
    const bool LAYOUT_MANAGED   = HAD_FULLSCREEN && (internalMode.has_value() ? layoutManaged : Fullscreen::controller()->layoutManagedFS(oldWindow));
    const bool GROUP_FLOATING   = m_target->floating();
    const auto GROUP_SPACE      = m_target->space();
    const auto GROUP_WORKSPACE  = GROUP_SPACE ? GROUP_SPACE->workspace() : nullptr;

    if (HAD_FULLSCREEN)
        Fullscreen::controller()->setFullscreenMode(oldWindow, Fullscreen::FSMODE_NONE, std::nullopt, LAYOUT_MANAGED, Fullscreen::FULLSCREEN_MUTATION_TRANSFER);

    const auto NEW_FS_INTERNAL_MODE = Fullscreen::controller()->getFullscreenModes(newWindow).internal;
    if (NEW_FS_INTERNAL_MODE != Fullscreen::FSMODE_NONE)
        Fullscreen::controller()->setFullscreenMode(newWindow, Fullscreen::FSMODE_NONE, std::nullopt, Fullscreen::controller()->layoutManagedFS(newWindow),
                                                    Fullscreen::FULLSCREEN_MUTATION_TRANSFER);

    if (newWindow->layoutTarget()->space())
        g_layoutManager->removeTarget(newWindow->layoutTarget());

    oldWindow->setInputBlocked(FOCUS_BLOCK_GROUP_INACTIVE, false);
    *oldWindow->presentation().alpha(WINDOW_ALPHA_LAYOUT) = 1.F;
    oldWindow->windowTarget()->setSpaceGhost(nullptr);
    oldWindow->grouping().detach();
    removeWindowDecos(oldWindow);

    newWindow->grouping().attach(SELF);
    m_groupPolicyFlags |= newWindow->grouping().rules();
    newWindow->windowTarget()->setFloating(GROUP_FLOATING);
    newWindow->windowTarget()->setSpaceGhost(GROUP_SPACE);

    if (GROUP_WORKSPACE && newWindow->m_workspace != GROUP_WORKSPACE) {
        newWindow->m_monitor = GROUP_WORKSPACE->m_monitor;
        newWindow->moveToWorkspace(GROUP_WORKSPACE);
    }

    m_windows.at(IDX) = newWindow;
    applyWindowDecosAndUpdates(newWindow);
    updateWindowVisibility();

    if (HAD_FULLSCREEN) {
        Fullscreen::controller()->setFullscreenMode(newWindow, FS_INTERNAL_MODE, std::nullopt, LAYOUT_MANAGED, Fullscreen::FULLSCREEN_MUTATION_TRANSFER);
        newWindow->windowTarget()->warpPositionSize();
    }
}

void CGroup::remove(PHLWINDOW w, Math::eDirection dir, eRemoveFromGroupReason reason) {
    std::optional<size_t> idx;
    for (size_t i = 0; i < m_windows.size(); ++i) {
        if (m_windows.at(i) == w) {
            idx = i;
            break;
        }
    }

    if (!idx)
        return;

    if ((m_current >= *idx && idx != 0) || (m_current >= m_windows.size() - 1 && m_current > 0))
        m_current--;

    auto g = m_self.lock(); // keep ref to avoid uaf after membership is detached

    w->grouping().detach();
    removeWindowDecos(w);

    w->setInputBlocked(FOCUS_BLOCK_GROUP_INACTIVE, false);
    *w->presentation().alpha(WINDOW_ALPHA_LAYOUT) = 1.F;

    const bool REMOVING_GROUP = m_windows.size() <= 1;

    if (REMOVING_GROUP) {
        w->windowTarget()->assignToSpace(nullptr);
        g_layoutManager->switchTargets(m_target, w->windowTarget());
    }

    // we do it after the above because switchTargets expects this to be a valid group
    m_windows.erase(m_windows.begin() + *idx);

    // if groupbar:disable_when_only is enabled and there is only one group member left, we need to fix its size because it will lose its groupbar.
    static auto PDISABLE = CConfigValue<Config::BOOL>("group:groupbar:disable_when_only");
    if (*PDISABLE && m_windows.size() == 1) {
        if (const auto REMAINING_MEMBER = m_windows.at(0).lock()) {
            const auto GROUPBAR = REMAINING_MEMBER->presentation().decoration(DECORATION_GROUPBAR);
            GROUPBAR->updateWindow(REMAINING_MEMBER);
            g_pDecorationPositioner->forceRecalcFor(REMAINING_MEMBER);
        }
    }

    if (!m_windows.empty())
        updateWindowVisibility();

    // do this here: otherwise the new current is hidden and workspace rules get wrong data
    if (!REMOVING_GROUP) {
        std::optional<Vector2D> focalPoint;
        if (dir != Math::DIRECTION_DEFAULT) {
            const auto box = m_target->position();
            switch (dir) {
                case Math::DIRECTION_RIGHT: focalPoint = Vector2D(box.x + box.w, box.y + box.h / 2.0); break;
                case Math::DIRECTION_LEFT: focalPoint = Vector2D(box.x, box.y + box.h / 2.0); break;
                case Math::DIRECTION_DOWN: focalPoint = Vector2D(box.x + box.w / 2.0, box.y + box.h); break;
                case Math::DIRECTION_UP: focalPoint = Vector2D(box.x + box.w / 2.0, box.y); break;
                default: break;
            }
        }

        // We don't need to assign a window to a new space if we intend to unmap it
        if (reason == REMOVE_FROM_GROUP_REASON_UNMAP_WINDOW)
            return;
        w->windowTarget()->assignToSpace(m_target->space(), focalPoint);
    }
}

void CGroup::moveCurrent(bool next) {
    size_t idx = m_current;

    if (next) {
        idx++;
        if (idx >= m_windows.size())
            idx = 0;
    } else {
        if (idx == 0)
            idx = m_windows.size() - 1;
        else
            idx--;
    }

    setCurrent(idx);
}

void CGroup::setCurrent(size_t idx) {
    if (idx == m_current || !m_target->window())
        return;

    const bool IS_FULLSCREEN     = Fullscreen::controller()->isFullscreen(m_target->window());
    const auto FS_MODE_INTERNAL  = Fullscreen::controller()->getFullscreenModes(m_target->window()).internal;
    const bool IS_LAYOUT_HANDLED = Fullscreen::controller()->layoutManagedFS(m_target->window());
    const auto WASFOCUS          = Desktop::focusState()->window() == current();
    auto       oldWindow         = m_windows.at(m_current).lock();

    if (IS_FULLSCREEN)
        Fullscreen::controller()->setFullscreenMode(oldWindow, Fullscreen::FSMODE_NONE);

    m_current = std::clamp(idx, sc<size_t>(0), m_windows.size() - 1);
    updateWindowVisibility();

    auto newWindow = m_windows.at(m_current).lock();

    if (IS_FULLSCREEN) {
        Fullscreen::controller()->setFullscreenMode(newWindow, FS_MODE_INTERNAL, std::nullopt, IS_LAYOUT_HANDLED);
        newWindow->windowTarget()->warpPositionSize();
    }

    if (WASFOCUS)
        Desktop::focusState()->rawWindowFocus(current(), FOCUS_REASON_GROUP_CURRENT_WINDOW_CHANGE);
}

void CGroup::setCurrent(PHLWINDOW w) {
    if (w == current())
        return;

    for (size_t i = 0; i < m_windows.size(); ++i) {
        if (m_windows.at(i) == w) {
            setCurrent(i);
            return;
        }
    }
}

size_t CGroup::getCurrentIdx() const {
    return m_current;
}

PHLWINDOW CGroup::head() const {
    return m_windows.front().lock();
}

PHLWINDOW CGroup::tail() const {
    return m_windows.back().lock();
}

PHLWINDOW CGroup::current() const {
    return m_windows.at(m_current).lock();
}

PHLWINDOW CGroup::next() const {
    return (m_current >= m_windows.size() - 1 ? m_windows.front() : m_windows.at(m_current + 1)).lock();
}

PHLWINDOW CGroup::fromIndex(size_t idx) const {
    if (idx >= m_windows.size())
        return nullptr;

    return m_windows.at(idx).lock();
}

const std::vector<PHLWINDOWREF>& CGroup::windows() const {
    return m_windows;
}

SP<Layout::CWindowGroupTarget> CGroup::target() const {
    return m_target;
}

void CGroup::applyWindowDecosAndUpdates(PHLWINDOW x) {
    static auto PDISABLE = CConfigValue<Config::BOOL>("group:groupbar:disable_when_only");
    const auto  GROUPBAR = makeShared<CHyprGroupBarDecoration>(x);
    if (*PDISABLE)
        GROUPBAR->updateWindow(x);
    x->presentation().addDecoration(GROUPBAR);

    x->m_ruleApplicator->propertiesChanged(Desktop::Rule::RULE_PROP_GROUP | Desktop::Rule::RULE_PROP_ON_WORKSPACE);
    x->presentation().updateDecorations();
    x->presentation().refreshValues();
}

void CGroup::removeWindowDecos(PHLWINDOW x) {
    const auto GROUPBAR = x->presentation().decoration(DECORATION_GROUPBAR);
    if (GROUPBAR)
        x->presentation().removeDecoration(GROUPBAR.get());

    x->m_ruleApplicator->propertiesChanged(Desktop::Rule::RULE_PROP_GROUP | Desktop::Rule::RULE_PROP_ON_WORKSPACE);
    x->presentation().updateDecorations();
    x->presentation().refreshValues();
}

void CGroup::updateWindowVisibility() {
    for (size_t i = 0; i < m_windows.size(); ++i) {
        if (i == m_current) {
            auto& x = m_windows.at(i);
            x->setInputBlocked(FOCUS_BLOCK_GROUP_INACTIVE, false);
            *x->presentation().alpha(WINDOW_ALPHA_LAYOUT) = 1.F;
            x->m_ruleApplicator->propertiesChanged(Desktop::Rule::RULE_PROP_GROUP | Desktop::Rule::RULE_PROP_ON_WORKSPACE);
            x->presentation().updateDecorations();
            x->presentation().refreshValues();
        } else {
            auto& x = m_windows.at(i);
            x->setInputBlocked(FOCUS_BLOCK_GROUP_INACTIVE, true);
            *x->presentation().alpha(WINDOW_ALPHA_LAYOUT) = 0.F;
        }
    }

    m_target->recalc();

    m_target->damageEntire();
}

size_t CGroup::size() const {
    return m_windows.size();
}

bool CGroup::locked() const {
    return m_groupPolicyFlags & GROUP_LOCK;
}

void CGroup::setLocked(bool x) {
    if (x)
        m_groupPolicyFlags |= GROUP_LOCK;
    else
        m_groupPolicyFlags &= ~GROUP_LOCK;
}

bool CGroup::denied() const {
    return m_groupPolicyFlags & GROUP_DENY;
}

void CGroup::setDenied(bool x) {
    if (x)
        m_groupPolicyFlags |= GROUP_DENY;
    else
        m_groupPolicyFlags &= ~GROUP_DENY;
}

void CGroup::updateWorkspace(PHLWORKSPACE ws) {
    if (!ws)
        return;

    for (const auto& w : windows()) {
        w->m_monitor = ws->m_monitor;
        w->moveToWorkspace(ws);
        w->updateToplevel();
        w->presentation().updateDecorations();
        w->windowTarget()->setSpaceGhost(ws->m_space);
    }
}

void CGroup::swapWithNext() {
    const bool HAD_FOCUS = Desktop::focusState()->window() == m_windows.at(m_current);

    size_t     idx = m_current + 1 >= m_windows.size() ? 0 : m_current + 1;
    std::iter_swap(m_windows.begin() + m_current, m_windows.begin() + idx);
    m_current = idx;

    updateWindowVisibility();

    if (HAD_FOCUS)
        Desktop::focusState()->fullWindowFocus(m_windows.at(m_current).lock(), FOCUS_REASON_DESKTOP_STATE_CHANGE);
}

void CGroup::swapWithLast() {
    const bool HAD_FOCUS = Desktop::focusState()->window() == m_windows.at(m_current);

    size_t     idx = m_current == 0 ? m_windows.size() - 1 : m_current - 1;
    std::iter_swap(m_windows.begin() + m_current, m_windows.begin() + idx);
    m_current = idx;

    updateWindowVisibility();

    if (HAD_FOCUS)
        Desktop::focusState()->fullWindowFocus(m_windows.at(m_current).lock(), FOCUS_REASON_DESKTOP_STATE_CHANGE);
}
