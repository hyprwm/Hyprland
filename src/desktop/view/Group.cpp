#include "Group.hpp"
#include "Window.hpp"

#include "../../render/decorations/CHyprGroupBarDecoration.hpp"
#include "../../layout/target/WindowGroupTarget.hpp"
#include "../../layout/target/WindowTarget.hpp"
#include "../../layout/target/Target.hpp"
#include "../../layout/space/Space.hpp"
#include "../../layout/LayoutManager.hpp"
#include "../../desktop/state/FocusState.hpp"

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
        RASSERT(!w->m_group, "CGroup: windows cannot contain grouped in init, this will explode");
        w->m_group = m_self.lock();
    }

    g_layoutManager->switchTargets(m_windows.at(0)->m_target, m_target);

    for (const auto& w : m_windows) {
        w->m_target->setSpaceGhost(m_target->space());
    }

    for (const auto& w : m_windows) {
        applyWindowDecosAndUpdates(w.lock());
    }

    updateWindowVisibility();
}

void CGroup::destroy() {
    while (true) {
        if (m_windows.size() == 1) {
            remove(m_windows.at(0).lock());
            break;
        }

        remove(m_windows.at(0).lock());
    }
}

CGroup::~CGroup() {
    if (m_target->space())
        m_target->assignToSpace(nullptr);
    std::erase_if(groups(), [this](const auto& e) { return !e || e == m_self; });
}

bool CGroup::has(PHLWINDOW w) const {
    return std::ranges::contains(m_windows, w);
}

void CGroup::add(PHLWINDOW w) {
    if (w->m_group) {
        if (w->m_group == m_self)
            return;

        const auto WINDOWS = w->m_group->windows();
        for (const auto& w : WINDOWS) {
            w->m_group->remove(w.lock());
            add(w.lock());
        }

        return;
    }

    m_windows.insert(m_windows.begin() + m_current + 1, w);
    m_current++;
    w->m_group = m_self.lock();
    w->m_target->setSpaceGhost(m_target->space());
    applyWindowDecosAndUpdates(w);
    updateWindowVisibility();
    m_target->recalc();
}

void CGroup::remove(PHLWINDOW w) {
    std::optional<size_t> idx;
    for (size_t i = 0; i < m_windows.size(); ++i) {
        if (m_windows.at(i) == w) {
            idx = i;
            break;
        }
    }

    if (!idx)
        return;

    if (m_current >= *idx)
        m_current--;

    auto g = m_self.lock(); // keep ref to avoid uaf after w->m_group.reset()

    w->m_group.reset();
    removeWindowDecos(w);

    w->setHidden(false);

    if (m_windows.size() <= 1) {
        w->m_target->assignToSpace(nullptr);
        g_layoutManager->switchTargets(m_target, w->m_target);
    } else
        w->m_target->assignToSpace(m_target->space());

    // we do it after the above because switchTargets expects this to be a valid group
    m_windows.erase(m_windows.begin() + *idx);

    updateWindowVisibility();
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
    const auto WASFOCUS = Desktop::focusState()->window() == current();
    m_current           = std::clamp(idx, sc<size_t>(0), m_windows.size() - 1);
    updateWindowVisibility();
    if (WASFOCUS)
        Desktop::focusState()->fullWindowFocus(current());
}

void CGroup::setCurrent(PHLWINDOW w) {
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

PHLWINDOW CGroup::fromIndex(size_t idx) const {
    if (idx >= m_windows.size())
        return nullptr;

    return m_windows.at(idx).lock();
}

const std::vector<PHLWINDOWREF>& CGroup::windows() const {
    return m_windows;
}

void CGroup::applyWindowDecosAndUpdates(PHLWINDOW x) {
    x->addWindowDeco(makeUnique<CHyprGroupBarDecoration>(x));

    x->m_ruleApplicator->propertiesChanged(Desktop::Rule::RULE_PROP_GROUP | Desktop::Rule::RULE_PROP_ON_WORKSPACE);
    x->updateWindowDecos();
    x->updateDecorationValues();
}

void CGroup::removeWindowDecos(PHLWINDOW x) {
    x->removeWindowDeco(x->getDecorationByType(DECORATION_GROUPBAR));

    x->m_ruleApplicator->propertiesChanged(Desktop::Rule::RULE_PROP_GROUP | Desktop::Rule::RULE_PROP_ON_WORKSPACE);
    x->updateWindowDecos();
    x->updateDecorationValues();
}

void CGroup::updateWindowVisibility() {
    for (size_t i = 0; i < m_windows.size(); ++i) {
        if (i == m_current) {
            auto& x = m_windows.at(i);
            x->setHidden(false);
            x->m_ruleApplicator->propertiesChanged(Desktop::Rule::RULE_PROP_GROUP | Desktop::Rule::RULE_PROP_ON_WORKSPACE);
            x->updateWindowDecos();
            x->updateDecorationValues();
        } else
            m_windows.at(i)->setHidden(true);
    }

    m_target->recalc();

    m_target->damageEntire();
}

size_t CGroup::size() const {
    return m_windows.size();
}

bool CGroup::locked() const {
    return m_locked;
}

void CGroup::setLocked(bool x) {
    m_locked = x;
}

bool CGroup::denied() const {
    return m_deny;
}

void CGroup::setDenied(bool x) {
    m_deny = x;
}
