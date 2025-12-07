#include "ExtWorkspace.hpp"
#include "../Compositor.hpp"
#include "../managers/HookSystemManager.hpp"
#include "../managers/eventLoop/EventLoopManager.hpp"
#include <algorithm>
#include <any>
#include <utility>
#include "core/Output.hpp"

CExtWorkspaceGroupResource::CExtWorkspaceGroupResource(WP<CExtWorkspaceManagerResource> manager, UP<CExtWorkspaceGroupHandleV1> resource, PHLMONITORREF monitor) :
    m_monitor(std::move(monitor)), m_manager(std::move(manager)), m_resource(std::move(resource)) {
    if (!good())
        return;

    m_resource->setData(this);
    m_manager->m_resource->sendWorkspaceGroup(m_resource.get());

    m_listeners.destroyed = m_monitor->m_events.destroy.listen([this] { m_resource->sendRemoved(); });

    m_resource->setOnDestroy([this](auto) { PROTO::extWorkspace->destroyGroup(m_self); });
    m_resource->setDestroy([this](auto) { PROTO::extWorkspace->destroyGroup(m_self); });

    m_resource->sendCapabilities(sc<extWorkspaceGroupHandleV1GroupCapabilities>(0));

    if (!PROTO::outputs.contains(m_monitor->m_name))
        return;

    const auto& output = PROTO::outputs.at(m_monitor->m_name);

    if (auto resource = output->outputResourceFrom(m_resource->client()))
        m_resource->sendOutputEnter(resource->getResource()->resource());

    m_listeners.outputBound = output->m_events.outputBound.listen([this](const SP<CWLOutputResource>& output) {
        if (output->client() == m_resource->client())
            m_resource->sendOutputEnter(output->getResource()->resource());
    });
}

bool CExtWorkspaceGroupResource::good() const {
    return m_resource;
}

WP<CExtWorkspaceGroupResource> CExtWorkspaceGroupResource::fromResource(wl_resource* resource) {
    auto handle = sc<CExtWorkspaceGroupHandleV1*>(wl_resource_get_user_data(resource))->data();
    auto data   = sc<CExtWorkspaceGroupResource*>(handle);
    return data ? data->m_self : WP<CExtWorkspaceGroupResource>();
}

void CExtWorkspaceGroupResource::sendToWorkspaces() {
    m_manager->sendGroupToWorkspaces(m_self);
    m_manager->scheduleDone();
}

void CExtWorkspaceGroupResource::workspaceEnter(const WP<CExtWorkspaceHandleV1>& handle) {
    m_resource->sendWorkspaceEnter(handle.get());
}
void CExtWorkspaceGroupResource::workspaceLeave(const WP<CExtWorkspaceHandleV1>& handle) {
    m_resource->sendWorkspaceLeave(handle.get());
}

CExtWorkspaceResource::CExtWorkspaceResource(WP<CExtWorkspaceManagerResource> manager, UP<CExtWorkspaceHandleV1> resource, PHLWORKSPACEREF workspace) :
    m_manager(std::move(manager)), m_resource(std::move(resource)), m_workspace(std::move(workspace)) {
    if (!good())
        return;

    m_resource->setData(this);
    m_manager->m_resource->sendWorkspace(m_resource.get());

    m_listeners.destroyed = m_workspace->m_events.destroy.listen([this] {
        m_resource->sendRemoved();

        if (m_manager)
            m_manager->scheduleDone();
    });

    m_listeners.activeChanged = m_workspace->m_events.activeChanged.listen([this] {
        sendState();
        sendCapabilities();
    });

    m_listeners.monitorChanged = m_workspace->m_events.monitorChanged.listen([this] { this->sendGroup(); });

    m_listeners.renamed = m_workspace->m_events.renamed.listen([this] {
        m_resource->sendName(m_workspace->m_name.c_str());

        if (m_manager)
            m_manager->scheduleDone();
    });

    m_resource->setOnDestroy([this](auto) { PROTO::extWorkspace->destroyWorkspace(m_self); });
    m_resource->setDestroy([this](auto) { PROTO::extWorkspace->destroyWorkspace(m_self); });

    m_resource->setActivate([this](void*) { m_pendingState.activate = true; });
    m_resource->setDeactivate([this](void*) { m_pendingState.deactivate = true; });

    m_resource->setAssign([this](void*, wl_resource* groupResource) {
        auto group = CExtWorkspaceGroupResource::fromResource(groupResource);

        if (group)
            m_pendingState.targetMonitor = group->m_monitor;
    });

    m_resource->sendName(m_workspace->m_name.c_str());

    wl_array coordinates;
    wl_array_init(&coordinates);

    auto id = m_workspace->m_id;
    if (id < 0 && !m_workspace->m_name.empty())
        id += UINT32_MAX - 1337;

    if (id > 0)
        *sc<uint32_t*>(wl_array_add(&coordinates, sizeof(uint32_t))) = id;

    m_resource->sendCoordinates(&coordinates);
    wl_array_release(&coordinates);

    sendState();
    sendCapabilities();
    sendGroup();

    m_manager->scheduleDone();
}

bool CExtWorkspaceResource::good() const {
    return m_resource;
}

bool CExtWorkspaceResource::isActive() const {
    if (!m_workspace)
        return false;

    auto const& monitor      = m_workspace->m_monitor;
    auto const& cmpWorkspace = m_workspace->m_isSpecialWorkspace ? monitor->m_activeSpecialWorkspace : monitor->m_activeWorkspace;
    return m_workspace == cmpWorkspace;
}

void CExtWorkspaceResource::sendState() {
    uint32_t state = 0;

    if (isActive())
        state |= EXT_WORKSPACE_HANDLE_V1_STATE_ACTIVE;

    if (m_workspace->hasUrgentWindow())
        state |= EXT_WORKSPACE_HANDLE_V1_STATE_URGENT;

    if (m_workspace->m_isSpecialWorkspace)
        state |= EXT_WORKSPACE_HANDLE_V1_STATE_HIDDEN;

    m_resource->sendState(sc<extWorkspaceHandleV1State>(state));

    if (m_manager)
        m_manager->scheduleDone();
}

void CExtWorkspaceResource::sendCapabilities() {
    uint32_t capabilities = EXT_WORKSPACE_HANDLE_V1_WORKSPACE_CAPABILITIES_ASSIGN;
    auto     active       = isActive();

    if (!active)
        capabilities |= EXT_WORKSPACE_HANDLE_V1_WORKSPACE_CAPABILITIES_ACTIVATE;

    if (active && m_workspace->m_isSpecialWorkspace)
        capabilities |= EXT_WORKSPACE_HANDLE_V1_WORKSPACE_CAPABILITIES_DEACTIVATE;

    m_resource->sendCapabilities(sc<extWorkspaceHandleV1WorkspaceCapabilities>(capabilities));

    if (m_manager)
        m_manager->scheduleDone();
}

void CExtWorkspaceResource::sendGroup() {
    if (m_group)
        m_group->workspaceLeave(m_resource);

    if (m_manager) {
        m_group = m_manager->findGroup(m_workspace->m_monitor);

        if (m_group)
            m_group->workspaceEnter(m_resource);

        m_manager->scheduleDone();
    }
}

void CExtWorkspaceResource::commit() {
    // order is important

    if (m_pendingState.deactivate && isActive() && m_workspace->m_isSpecialWorkspace)
        m_workspace->m_monitor->setSpecialWorkspace(nullptr);

    if (m_pendingState.targetMonitor && m_workspace && m_workspace->m_monitor != m_pendingState.targetMonitor)
        g_pCompositor->moveWorkspaceToMonitor(m_workspace.lock(), m_pendingState.targetMonitor.lock(), true);

    if (m_pendingState.activate && !isActive() && m_workspace)
        m_workspace->m_monitor->changeWorkspace(m_workspace.lock());

    m_pendingState.activate   = false;
    m_pendingState.deactivate = false;
    m_pendingState.targetMonitor.reset();
}

CExtWorkspaceManagerResource::CExtWorkspaceManagerResource(UP<CExtWorkspaceManagerV1> resource) : m_resource(std::move(resource)) {
    if (!good())
        return;

    m_resource->setOnDestroy([this](auto) { PROTO::extWorkspace->destroyManager(m_self); });

    m_resource->setStop([this](auto) {
        m_resource->sendFinished();
        PROTO::extWorkspace->destroyManager(m_self);
    });

    m_resource->setCommit([this](auto) {
        for (auto& workspace : PROTO::extWorkspace->m_workspaces) {
            if (workspace->m_manager == m_self)
                workspace->commit();
        }
    });
}

void CExtWorkspaceManagerResource::init(WP<CExtWorkspaceManagerResource> self) {
    if (!good())
        return;

    m_self = self;

    for (auto const& m : g_pCompositor->m_monitors) {
        onMonitorCreated(m);
    }

    for (auto const& w : g_pCompositor->getWorkspaces()) {
        onWorkspaceCreated(w.lock());
    }
}

bool CExtWorkspaceManagerResource::good() const {
    return m_resource;
}

void CExtWorkspaceManagerResource::scheduleDone() {
    if (m_doneScheduled)
        return;

    m_doneScheduled = true;
    g_pEventLoopManager->doLater([self = m_self] {
        if (!self || !self->m_resource)
            return;

        self->m_doneScheduled = false;
        self->m_resource->sendDone();
    });
}

WP<CExtWorkspaceGroupResource> CExtWorkspaceManagerResource::findGroup(const PHLMONITORREF& monitor) const {
    auto iter = std::ranges::find_if(PROTO::extWorkspace->m_groups,
                                     [&](const UP<CExtWorkspaceGroupResource>& resource) { return resource->m_manager.get() == this && resource->m_monitor == monitor; });

    return iter != PROTO::extWorkspace->m_groups.end() ? *iter : WP<CExtWorkspaceGroupResource>();
}

void CExtWorkspaceManagerResource::sendGroupToWorkspaces(const WP<CExtWorkspaceGroupResource>& group) {
    for (auto& workspace : PROTO::extWorkspace->m_workspaces) {
        if (workspace->m_manager == m_self && workspace->m_workspace && workspace->m_workspace->m_monitor == group->m_monitor)
            workspace->sendGroup();
    }
}

void CExtWorkspaceManagerResource::onMonitorCreated(const PHLMONITOR& monitor) {
    auto& group = PROTO::extWorkspace->m_groups.emplace_back(
        makeUnique<CExtWorkspaceGroupResource>(m_self, makeUnique<CExtWorkspaceGroupHandleV1>(m_resource->client(), m_resource->version(), 0), monitor));
    group->m_self = group;
    group->sendToWorkspaces();

    if UNLIKELY (!group->good()) {
        LOGM(ERR, "Couldn't create a workspace group object");
        wl_client_post_no_memory(m_resource->client());
        return;
    }

    scheduleDone();
}

void CExtWorkspaceManagerResource::onWorkspaceCreated(const PHLWORKSPACE& workspace) {
    auto& ws = PROTO::extWorkspace->m_workspaces.emplace_back(
        makeUnique<CExtWorkspaceResource>(m_self, makeUnique<CExtWorkspaceHandleV1>(m_resource->client(), m_resource->version(), 0), workspace));
    ws->m_self = ws;

    if UNLIKELY (!ws->good()) {
        LOGM(ERR, "Couldn't create a workspace object");
        wl_client_post_no_memory(m_resource->client());
        return;
    }
}

CExtWorkspaceProtocol::CExtWorkspaceProtocol(const wl_interface* iface, const int& ver, const std::string& name) : IWaylandProtocol(iface, ver, name) {
    static auto P1 = g_pHookSystem->hookDynamic("createWorkspace", [this](void* self, SCallbackInfo& info, std::any data) {
        auto workspace = std::any_cast<CWorkspace*>(data)->m_self.lock();

        for (auto const& m : m_managers) {
            m->onWorkspaceCreated(workspace);
        }
    });

    static auto P2 = g_pHookSystem->hookDynamic("monitorAdded", [this](void* self, SCallbackInfo& info, std::any data) {
        auto monitor = std::any_cast<PHLMONITOR>(data);

        for (auto const& m : m_managers) {
            m->onMonitorCreated(monitor);
        }
    });
}

void CExtWorkspaceProtocol::bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id) {
    auto& manager = m_managers.emplace_back(makeUnique<CExtWorkspaceManagerResource>(makeUnique<CExtWorkspaceManagerV1>(client, ver, id)));
    manager->init(manager);

    if UNLIKELY (!manager->good()) {
        LOGM(ERR, "Couldn't create a workspace manager");
        wl_client_post_no_memory(client);
        return;
    }
}

void CExtWorkspaceProtocol::destroyGroup(const WP<CExtWorkspaceGroupResource>& group) {
    std::erase_if(m_groups, [&](const UP<CExtWorkspaceGroupResource>& resource) { return resource == group; });
}

void CExtWorkspaceProtocol::destroyWorkspace(const WP<CExtWorkspaceResource>& workspace) {
    std::erase_if(m_workspaces, [&](const UP<CExtWorkspaceResource>& resource) { return resource == workspace; });
}

void CExtWorkspaceProtocol::destroyManager(const WP<CExtWorkspaceManagerResource>& manager) {
    std::erase_if(PROTO::extWorkspace->m_managers, [&](const UP<CExtWorkspaceManagerResource>& resource) { return resource == manager; });
}
