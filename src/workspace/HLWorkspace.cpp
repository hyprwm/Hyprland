#include "HLWorkspace.hpp"
#include "../desktop/view/window/WindowPresentation.hpp"
#include "../desktop/view/Group.hpp"
#include "../desktop/view/LayerSurface.hpp"
#include "../desktop/state/FocusState.hpp"
#include "../desktop/history/WorkspaceHistoryTracker.hpp"
#include "../Compositor.hpp"
#include "../config/shared/parserUtils/ParserUtils.hpp"
#include "../config/shared/animation/AnimationTree.hpp"
#include "../config/shared/workspace/WorkspaceRuleManager.hpp"
#include "../config/supplementary/executor/Executor.hpp"
#include "../config/supplementary/propRefresher/PropRefresher.hpp"
#include "../animation/AnimationManager.hpp"
#include "../ipc/s2/S2.hpp"
#include "../managers/fullscreen/FullscreenController.hpp"
#include "../output/Monitor.hpp"
#include "../state/MonitorState.hpp"
#include "../state/WorkspacePlacementController.hpp"
#include "../state/WorkspaceState.hpp"
#include "../state/workspace/LifecyclePolicyAdapter.hpp"
#include "../layout/algorithm/Algorithm.hpp"
#include "../layout/space/Space.hpp"
#include "../layout/target/Target.hpp"
#include "../layout/supplementary/WorkspaceAlgoMatcher.hpp"
#include "../event/EventBus.hpp"
#include "../workspace/filter/WorkspaceFilter.hpp"
#include "../workspace/query/Query.hpp"
#include "../workspace/SpecialWorkspace.hpp"

#include <hyprutils/animation/AnimatedVariable.hpp>
#include <hyprutils/string/String.hpp>
using namespace Hyprutils::String;
using namespace Desktop::View;

Workspace::CHLWorkspace::CHLWorkspace(WorkspaceID id, PHLMONITOR monitor, std::string displayName, std::string addressableName, eWorkspaceType type, bool isEmpty) :
    Workspace::IAbstractWorkspace(type), m_monitor(monitor), m_wasCreatedEmpty(isEmpty), m_focusTracker(this), m_name(std::move(displayName)), m_id(std::move(id)),
    m_addressableName(std::move(addressableName)) {
    ;
}

Workspace::WorkspaceID Workspace::CHLWorkspace::id() const {
    return m_id;
}

const std::string& Workspace::CHLWorkspace::displayName() const {
    return m_name;
}

const std::string& Workspace::CHLWorkspace::addressableName() const {
    return m_addressableName;
}

SP<Monitor::IMonitorAddressable> Workspace::CHLWorkspace::monitor() const {
    return dynamicPointerCast<Monitor::IMonitorAddressable>(m_monitor.lock());
}

SP<Layout::CSpace> Workspace::CHLWorkspace::space() const {
    return m_space;
}

std::optional<Workspace::WorkspaceIDContainer> Workspace::CHLWorkspace::numberedID() const {
    const auto NUMBERED = std::get_if<Workspace::SWorkspaceNumberedID>(&m_id);
    return NUMBERED ? std::optional{NUMBERED->value} : std::nullopt;
}

bool Workspace::CHLWorkspace::visible() const {
    return m_visible;
}

void Workspace::CHLWorkspace::setVisible(bool visible) {
    m_visible = visible;
}

void Workspace::CHLWorkspace::init(PHLWORKSPACE self) {
    m_self = self;

    Animation::mgr()->createAnimation(Vector2D(0, 0), m_renderOffset,
                                      Config::animationTree()->getAnimationPropertyConfig(type() == eWorkspaceType::SPECIAL ? "specialWorkspaceIn" : "workspacesIn"), self,
                                      AVARDAMAGE_ENTIRE);
    Animation::mgr()->createAnimation(1.f, m_alpha, Config::animationTree()->getAnimationPropertyConfig(type() == eWorkspaceType::SPECIAL ? "specialWorkspaceIn" : "workspacesIn"),
                                      self, AVARDAMAGE_ENTIRE);

    const auto RULEFORTHIS = Config::workspaceRuleMgr()->getWorkspaceRuleFor(self).value_or(Config::CWorkspaceRule{});
    if (RULEFORTHIS.m_defaultName.has_value())
        m_name = RULEFORTHIS.m_defaultName.value();
    if (RULEFORTHIS.m_animationStyle.has_value())
        m_animationStyle = RULEFORTHIS.m_animationStyle.value();

    m_space = Layout::CSpace::create(m_self.lock());
    m_space->setAlgorithmProvider(Layout::Supplementary::algoMatcher()->createAlgorithmForWorkspace(m_self.lock()));

    applyTypeSpecificRules(RULEFORTHIS);

    if (self->m_wasCreatedEmpty)
        if (auto cmd = RULEFORTHIS.m_onCreatedEmptyRunCmd)
            Config::Supplementary::executor()->spawnWithRules(*cmd, self);

    IPC::Socket2::sock()->postEvent({.event = "createworkspace", .data = m_name});
    IPC::Socket2::sock()->postEvent({.event = "createworkspacev2", .data = std::format("{},{}", Workspace::selector(*this), displayName())});
    Event::bus()->m_events.workspace.created.emit(self);
}

void Workspace::CHLWorkspace::applyTypeSpecificRules(const Config::CWorkspaceRule&) {
    ;
}

Workspace::CHLWorkspace::~CHLWorkspace() {
    LOG(Log::DEBUG, "Destroying workspace {}", addressableName());

    State::Workspace::workspaceDestroyed({id(), addressableName(), type()});

    if (IPC::Socket2::sock()) {
        IPC::Socket2::sock()->postEvent({.event = "destroyworkspace", .data = m_name});
        IPC::Socket2::sock()->postEvent({.event = "destroyworkspacev2", .data = std::format("{},{}", Workspace::selector(*this), displayName())});
    }

    Event::bus()->m_events.workspace.removed.emit(m_self);

    m_events.destroy.emit();
}

PHLWINDOW Workspace::CHLWorkspace::getLastFocusedWindow() const {
    const auto LAST = m_focusTracker.last();
    if (!validMapped(LAST) || LAST->m_workspace.get() != this)
        return nullptr;

    return LAST;
}

void Workspace::CHLWorkspace::rememberFocusedWindow(PHLWINDOW window) {
    m_focusTracker.remember(std::move(window));
}

PHLWINDOW Workspace::CHLWorkspace::getFocusCandidate() const {
    auto pWindow = getLastFocusedWindow();

    if (!pWindow)
        pWindow = getTopLeftWindow();

    if (!pWindow)
        pWindow = getFirstWindow();

    return pWindow;
}

bool Workspace::CHLWorkspace::matchesStaticSelector(const std::string& selector_) const {
    const Workspace::Filter::CWorkspaceFilter filter{selector_, &Workspace::Filter::hlDataSource()};
    if (!filter.error().empty()) {
        LOG(Log::ERR, "Invalid workspace filter '{}': {}", selector_, filter.error());
        return false;
    }

    return filter.matches(*this);
}

MONITORID Workspace::CHLWorkspace::monitorID() const {
    return m_monitor ? m_monitor->m_id : MONITOR_INVALID;
}

bool Workspace::CHLWorkspace::isVisibleNotCovered() const {
    const auto PMONITOR = m_monitor.lock();
    if (!PMONITOR)
        return false;

    if (PMONITOR->m_activeSpecialWorkspace)
        return PMONITOR->m_activeSpecialWorkspace.get() == this;

    return PMONITOR->m_activeWorkspace.get() == this;
}

int Workspace::CHLWorkspace::getWindowCount(std::optional<bool> onlyTiled, std::optional<bool> onlyPinned, std::optional<bool> onlyVisible) const {
    int no = 0;

    if (!m_space)
        return 0;

    for (auto const& t : m_space->targets()) {
        if (!t)
            continue;

        const auto visibilityFulfilled = t->window() && !t->window()->isHidden() &&
            !t->window()->isInputBlockedReasonAnyOf(FOCUS_BLOCK_GROUP_INACTIVE | FOCUS_BLOCK_MONOCLE_INACTIVE | FOCUS_BLOCK_BELOW_FULLSCREEN);

        if (onlyTiled.has_value() && t->floating() == onlyTiled.value())
            continue;
        if (onlyPinned.has_value() && (!t->window() || sc<bool>(t->window()->m_state & WINDOW_STATE_PINNED) != onlyPinned.value()))
            continue;
        if (onlyVisible.has_value() && (!t->window() || visibilityFulfilled != onlyVisible.value()))
            continue;
        no++;
    }

    return no;
}

int Workspace::CHLWorkspace::getGroups(std::optional<bool> onlyTiled, std::optional<bool> onlyPinned, std::optional<bool> onlyVisible) const {
    int no = 0;
    for (auto const& g : Desktop::View::groups()) {
        const auto HEAD = g->head();

        const auto visibilityFulfilled = g->current() && !g->current()->isHidden() &&
            !g->current()->isInputBlockedReasonAnyOf(FOCUS_BLOCK_GROUP_INACTIVE | FOCUS_BLOCK_MONOCLE_INACTIVE | FOCUS_BLOCK_BELOW_FULLSCREEN);

        if (HEAD->m_workspace.get() != this || !HEAD->mapped())
            continue;
        if (onlyTiled.has_value() && HEAD->isFloating() == onlyTiled.value())
            continue;
        if (onlyPinned.has_value() && sc<bool>(HEAD->m_state & WINDOW_STATE_PINNED) != onlyPinned.value())
            continue;
        if (onlyVisible.has_value() && visibilityFulfilled != onlyVisible.value())
            continue;
        no++;
    }
    return no;
}

PHLWINDOW Workspace::CHLWorkspace::getFirstWindow() const {
    for (auto const& w : Desktop::windowState()->windows()) {
        if (w->m_workspace == m_self && w->mapped() && w->acceptsInput())
            return w;
    }

    return nullptr;
}

PHLWINDOW Workspace::CHLWorkspace::getTopLeftWindow() const {
    const auto PMONITOR = m_monitor.lock();

    for (auto const& w : Desktop::windowState()->windows()) {
        if (w->m_workspace != m_self || !w->mapped() || !w->acceptsInput())
            continue;

        const auto WINDOWIDEALBB = w->getWindowIdealBoundingBoxIgnoreReserved();

        if (WINDOWIDEALBB.x <= PMONITOR->m_position.x + 1 && WINDOWIDEALBB.y <= PMONITOR->m_position.y + 1)
            return w;
    }
    return nullptr;
}

bool Workspace::CHLWorkspace::hasUrgentWindow() const {
    return std::ranges::any_of(Desktop::windowState()->windows(), [this](const auto& w) { return w->m_workspace == m_self && w->mapped() && (w->m_hints & WINDOW_HINT_URGENT); });
}

void Workspace::CHLWorkspace::updateWindowDecos() {
    for (auto const& w : Desktop::windowState()->windows()) {
        if (w->m_workspace != m_self)
            continue;

        w->presentation().updateDecorations();
    }
}

void Workspace::CHLWorkspace::updateWindowData() {
    const auto WORKSPACERULE = Config::workspaceRuleMgr()->getWorkspaceRuleFor(m_self.lock());

    for (auto const& w : Desktop::windowState()->windows()) {
        if (w->m_workspace != m_self)
            continue;

        w->updateWindowData(WORKSPACERULE.value_or(Config::CWorkspaceRule{}));
    }
}

void Workspace::CHLWorkspace::forceReportSizesToWindows() {
    for (auto const& w : Desktop::windowState()->windows()) {
        if (w->m_workspace != m_self || !w->mapped() || w->isHidden())
            continue;

        w->sendWindowSize(true);
    }
}

void Workspace::CHLWorkspace::rename(const std::string& name) {
    if (type() == Workspace::eWorkspaceType::SPECIAL)
        return;

    LOG(Log::DEBUG, "CHLWorkspace::rename: Renaming workspace {} to '{}'", addressableName(), name);
    m_name = name;

    Config::Supplementary::refresher()->scheduleRefresh(Config::Supplementary::REFRESH_ALL);

    m_wasRenamed = true;

    IPC::Socket2::sock()->postEvent({.event = "renameworkspace", .data = std::format("{},{}", addressableName(), displayName())});
    m_events.renamed.emit();
    Event::bus()->m_events.workspace.renamed.emit(m_self);
}

void Workspace::CHLWorkspace::changeID(Workspace::SWorkspaceNumberedID id) {
    if (!numberedID() || id.value == 0)
        return; // invalid

    const auto OLD_IDENTITY = State::Workspace::SWorkspaceIdentity{m_id, addressableName(), type()};
    const auto OLD_ID       = *numberedID();
    LOG(Log::DEBUG, "CHLWorkspace::changeID: Changing workspace id {} to {}", OLD_ID, id.value);
    m_id              = id;
    m_addressableName = std::to_string(id.value);

    if (!m_wasRenamed)
        m_name = std::format("{}", id.value);

    Config::Supplementary::refresher()->scheduleRefresh(Config::Supplementary::REFRESH_ALL);

    State::Workspace::workspaceIdentityChanged(OLD_IDENTITY, {m_id, addressableName(), type()});
    Desktop::History::workspaceTracker()->workspaceIdentityChanged(m_self.lock());

    IPC::Socket2::sock()->postEvent({.event = "changeworkspaceid", .data = std::format("{},{}", OLD_ID, id.value)});
    m_events.idChanged.emit();
}

void Workspace::CHLWorkspace::updateWindows() {
    for (auto const& t : m_space->targets()) {
        if (t->window())
            t->window()->m_ruleApplicator->propertiesChanged(Desktop::Rule::RULE_PROP_ON_WORKSPACE);
    }
}
