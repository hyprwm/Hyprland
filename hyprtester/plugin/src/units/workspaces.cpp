#include "../private.hpp"
#include "../globals.hpp"

static SDispatchResult expectWorkspaceRenameEvent(lua_State* L) {
    const auto SELECTOR = std::string{luaL_checkstring(L, 1)};
    const auto NAME     = std::string{luaL_checkstring(L, 2)};

    const auto WORKSPACE = State::Workspace::state()->find(State::Workspace::resolver()->getWorkspaceTargetFromString(SELECTOR));
    if (!WORKSPACE)
        return {.success = false, .error = std::format("No workspace matching '{}'", SELECTOR)};

    size_t          eventCount = 0;
    PHLWORKSPACEREF eventWorkspace;
    std::string     eventName;
    const auto      LISTENER = Event::bus()->m_events.workspace.renamed.listen([&](PHLWORKSPACEREF workspace) {
        ++eventCount;
        eventWorkspace = workspace;
        if (const auto WORKSPACE = workspace.lock())
            eventName = WORKSPACE->m_name;
    });

    WORKSPACE->rename(NAME);

    if (eventCount != 1)
        return {.success = false, .error = std::format("Expected one workspace rename event, got {}", eventCount)};
    if (eventWorkspace.lock() != WORKSPACE)
        return {.success = false, .error = "Workspace rename event carried the wrong workspace"};
    if (eventName != NAME)
        return {.success = false, .error = std::format("Workspace rename event observed name '{}', expected '{}'", eventName, NAME)};

    return {};
}

static SDispatchResult expectWorkspaceLifecycleState(lua_State* L) {
    const auto MONITORNAME = std::string{luaL_checkstring(L, 1)};

    const auto MONITOR = std::ranges::find(State::monitorState()->monitors(), MONITORNAME, &Monitor::CMonitor::m_name);
    if (MONITOR == State::monitorState()->monitors().end() || !(*MONITOR)->m_activeWorkspace)
        return {.success = false, .error = std::format("Monitor '{}' has no active workspace", MONITORNAME)};

    const auto WORKSPACE = (*MONITOR)->m_activeWorkspace;
    if (!WORKSPACE->visible())
        return {.success = false, .error = "Active lifecycle workspace is not visible"};
    if (WORKSPACE->m_alpha->value() != 1.F || WORKSPACE->m_alpha->goal() != 1.F)
        return {.success = false, .error = "Active lifecycle workspace alpha is not instantly IN"};
    if (WORKSPACE->m_renderOffset->value() != Vector2D{} || WORKSPACE->m_renderOffset->goal() != Vector2D{})
        return {.success = false, .error = "Active lifecycle workspace offset is not instantly IN"};

    return {};
}

REGISTER_UNIT(workspaces) {
    registerLuaFn<expectWorkspaceRenameEvent>("expect_workspace_rename_event");
    registerLuaFn<expectWorkspaceLifecycleState>("expect_workspace_lifecycle_state");
}