#include "MonocleAlgorithm.hpp"

#include "../../Algorithm.hpp"
#include "../../../space/Space.hpp"
#include "../../../target/WindowTarget.hpp"
#include "../../../LayoutManager.hpp"

#include "../../../../config/ConfigValue.hpp"
#include "../../../../desktop/state/FocusState.hpp"
#include "../../../../desktop/history/WindowHistoryTracker.hpp"
#include "../../../../helpers/Monitor.hpp"
#include "../../../../Compositor.hpp"

#include <hyprutils/string/VarList2.hpp>
#include <hyprutils/string/ConstVarList.hpp>
#include <hyprutils/utils/ScopeGuard.hpp>

using namespace Hyprutils::String;
using namespace Hyprutils::Utils;
using namespace Layout;
using namespace Layout::Tiled;

CMonocleAlgorithm::CMonocleAlgorithm() {
    // hook into focus changes to bring focused window to front
    m_focusCallback = g_pHookSystem->hookDynamic("activeWindow", [this](void* hk, SCallbackInfo& info, std::any param) {
        const auto PWINDOW = std::any_cast<Desktop::View::SWindowActiveEvent>(param).window;

        if (!PWINDOW)
            return;

        if (!PWINDOW->m_workspace->isVisible())
            return;

        const auto TARGET = PWINDOW->layoutTarget();
        if (!TARGET)
            return;

        focusTargetUpdate(TARGET);
    });
}

CMonocleAlgorithm::~CMonocleAlgorithm() {
    // unhide all windows before destruction
    for (const auto& data : m_targetDatas) {
        const auto TARGET = data->target.lock();
        if (!TARGET)
            continue;

        const auto WINDOW = TARGET->window();
        if (WINDOW)
            WINDOW->setHidden(false);
    }

    m_focusCallback.reset();
}

SP<SMonocleTargetData> CMonocleAlgorithm::dataFor(SP<ITarget> t) {
    for (auto& data : m_targetDatas) {
        if (data->target.lock() == t)
            return data;
    }
    return nullptr;
}

void CMonocleAlgorithm::newTarget(SP<ITarget> target) {
    const auto DATA = m_targetDatas.emplace_back(makeShared<SMonocleTargetData>(target));

    m_currentVisibleIndex = m_targetDatas.size() - 1;

    recalculate();
}

void CMonocleAlgorithm::movedTarget(SP<ITarget> target, std::optional<Vector2D> focalPoint) {
    newTarget(target);
}

void CMonocleAlgorithm::removeTarget(SP<ITarget> target) {
    auto it = std::ranges::find_if(m_targetDatas, [target](const auto& data) { return data->target.lock() == target; });

    if (it == m_targetDatas.end())
        return;

    // unhide window when removing from monocle layout
    const auto WINDOW = target->window();
    if (WINDOW)
        WINDOW->setHidden(false);

    const auto INDEX = std::distance(m_targetDatas.begin(), it);
    m_targetDatas.erase(it);

    if (m_targetDatas.empty()) {
        m_currentVisibleIndex = 0;
        return;
    }

    // try to use the last window in history if we can
    for (const auto& historyWindow : Desktop::History::windowTracker()->historyForWorkspace(m_parent->space()->workspace()) | std::views::reverse) {
        auto it = std::ranges::find_if(m_targetDatas, [&historyWindow](const auto& d) { return d->target == historyWindow->layoutTarget(); });

        if (it == m_targetDatas.end())
            continue;

        // we found a historical target, use that first
        m_currentVisibleIndex = std::distance(m_targetDatas.begin(), it);

        recalculate();

        return;
    }

    // if we didn't find history, fall back to last

    if (m_currentVisibleIndex >= (int)m_targetDatas.size())
        m_currentVisibleIndex = m_targetDatas.size() - 1;
    else if (INDEX <= m_currentVisibleIndex && m_currentVisibleIndex > 0)
        m_currentVisibleIndex--;

    recalculate();
}

void CMonocleAlgorithm::resizeTarget(const Vector2D& Δ, SP<ITarget> target, eRectCorner corner) {
    // monocle layout doesn't support manual resizing, all windows are fullscreen
}

void CMonocleAlgorithm::recalculate() {
    if (m_targetDatas.empty())
        return;

    const auto WORK_AREA = m_parent->space()->workArea();

    for (size_t i = 0; i < m_targetDatas.size(); ++i) {
        const auto& DATA   = m_targetDatas[i];
        const auto  TARGET = DATA->target.lock();

        if (!TARGET)
            continue;

        const auto WINDOW = TARGET->window();
        if (!WINDOW)
            continue;

        DATA->layoutBox = WORK_AREA;
        TARGET->setPositionGlobal(WORK_AREA);

        const bool SHOULD_BE_VISIBLE = ((int)i == m_currentVisibleIndex);
        WINDOW->setHidden(!SHOULD_BE_VISIBLE);
    }
}

SP<ITarget> CMonocleAlgorithm::getNextCandidate(SP<ITarget> old) {
    if (m_targetDatas.empty())
        return nullptr;

    auto it = std::ranges::find_if(m_targetDatas, [old](const auto& data) { return data->target.lock() == old; });

    if (it == m_targetDatas.end()) {
        if (m_currentVisibleIndex >= 0 && m_currentVisibleIndex < (int)m_targetDatas.size())
            return m_targetDatas[m_currentVisibleIndex]->target.lock();
        return nullptr;
    }

    auto next = std::next(it);
    if (next == m_targetDatas.end())
        next = m_targetDatas.begin();

    return next->get()->target.lock();
}

std::expected<void, std::string> CMonocleAlgorithm::layoutMsg(const std::string_view& sv) {
    CVarList2 vars(std::string{sv}, 0, 's');

    if (vars.size() < 1)
        return std::unexpected("layoutmsg requires at least 1 argument");

    const auto COMMAND = vars[0];

    if (COMMAND == "cyclenext") {
        cycleNext();
        return {};
    } else if (COMMAND == "cycleprev") {
        cyclePrev();
        return {};
    }

    return std::unexpected(std::format("Unknown monocle layoutmsg: {}", COMMAND));
}

std::optional<Vector2D> CMonocleAlgorithm::predictSizeForNewTarget() {
    const auto WORK_AREA = m_parent->space()->workArea();
    return WORK_AREA.size();
}

void CMonocleAlgorithm::swapTargets(SP<ITarget> a, SP<ITarget> b) {
    auto nodeA = dataFor(a);
    auto nodeB = dataFor(b);

    if (nodeA)
        nodeA->target = b;
    if (nodeB)
        nodeB->target = a;

    recalculate();
}

void CMonocleAlgorithm::moveTargetInDirection(SP<ITarget> t, Math::eDirection dir, bool silent) {
    // try to find a monitor in the specified direction, thats the logical thing
    if (!t || !t->space() || !t->space()->workspace())
        return;

    const auto PMONINDIR = g_pCompositor->getMonitorInDirection(t->space()->workspace()->m_monitor.lock(), dir);

    // if we found a monitor, move the window there
    if (PMONINDIR && PMONINDIR != t->space()->workspace()->m_monitor.lock()) {
        const auto TARGETWS = PMONINDIR->m_activeWorkspace;

        if (t->window())
            t->window()->setAnimationsToMove();

        t->assignToSpace(TARGETWS->m_space);
    }
}

void CMonocleAlgorithm::cycleNext() {
    if (m_targetDatas.empty())
        return;

    m_currentVisibleIndex = (m_currentVisibleIndex + 1) % m_targetDatas.size();
    updateVisible();
}

void CMonocleAlgorithm::cyclePrev() {
    if (m_targetDatas.empty())
        return;

    m_currentVisibleIndex--;
    if (m_currentVisibleIndex < 0)
        m_currentVisibleIndex = m_targetDatas.size() - 1;
    updateVisible();
}

void CMonocleAlgorithm::focusTargetUpdate(SP<ITarget> target) {
    auto it = std::ranges::find_if(m_targetDatas, [target](const auto& data) { return data->target.lock() == target; });

    if (it == m_targetDatas.end())
        return;

    const auto NEW_INDEX = std::distance(m_targetDatas.begin(), it);

    if (m_currentVisibleIndex != NEW_INDEX) {
        m_currentVisibleIndex = NEW_INDEX;
        updateVisible();
    }
}

void CMonocleAlgorithm::updateVisible() {
    recalculate();

    const auto VISIBLE_TARGET = getVisibleTarget();
    if (!VISIBLE_TARGET)
        return;

    const auto WINDOW = VISIBLE_TARGET->window();
    if (!WINDOW)
        return;

    Desktop::focusState()->fullWindowFocus(WINDOW, Desktop::FOCUS_REASON_DESKTOP_STATE_CHANGE);
}

SP<ITarget> CMonocleAlgorithm::getVisibleTarget() {
    if (m_currentVisibleIndex < 0 || m_currentVisibleIndex >= (int)m_targetDatas.size())
        return nullptr;

    return m_targetDatas[m_currentVisibleIndex]->target.lock();
}
