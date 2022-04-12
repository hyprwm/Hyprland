#include "../Compositor.hpp"
#include "../helpers/WLClasses.hpp"
#include "../managers/InputManager.hpp"
#include "../render/Renderer.hpp"
#include "Events.hpp"

// --------------------------------------------------------- //
//   __  __  ____  _   _ _____ _______ ____  _____   _____   //
//  |  \/  |/ __ \| \ | |_   _|__   __/ __ \|  __ \ / ____|  //
//  | \  / | |  | |  \| | | |    | | | |  | | |__) | (___    //
//  | |\/| | |  | | . ` | | |    | | | |  | |  _  / \___ \   //
//  | |  | | |__| | |\  |_| |_   | | | |__| | | \ \ ____) |  //
//  |_|  |_|\____/|_| \_|_____|  |_|  \____/|_|  \_\_____/   //
//                                                           //
// --------------------------------------------------------- //

void Events::listener_change(wl_listener* listener, void* data) {
    // layout got changed, let's update monitors.
    const auto CONFIG = wlr_output_configuration_v1_create();

    for (auto& m : g_pCompositor->m_lMonitors) {
        const auto CONFIGHEAD = wlr_output_configuration_head_v1_create(CONFIG, m.output);

        // TODO: clients off of disabled
        wlr_box BOX;
        wlr_output_layout_get_box(g_pCompositor->m_sWLROutputLayout, m.output, &BOX);

        m.vecSize.x = BOX.width;
        m.vecSize.y = BOX.height;
        m.vecPosition.x = BOX.x;
        m.vecPosition.y = BOX.y;

        CONFIGHEAD->state.enabled = m.output->enabled;
        CONFIGHEAD->state.mode = m.output->current_mode;
        CONFIGHEAD->state.x = m.vecPosition.x;
        CONFIGHEAD->state.y = m.vecPosition.y;

        wlr_output_set_custom_mode(m.output, m.vecSize.x, m.vecSize.y, (int)(round(m.refreshRate * 1000)));
    }

    wlr_output_manager_v1_set_configuration(g_pCompositor->m_sWLROutputMgr, CONFIG);
}

void Events::listener_newOutput(wl_listener* listener, void* data) {
    // new monitor added, let's accomodate for that.
    const auto OUTPUT = (wlr_output*)data;

    SMonitor newMonitor;
    newMonitor.output = OUTPUT;
    newMonitor.ID = g_pCompositor->m_lMonitors.size();
    newMonitor.szName = OUTPUT->name;

    wlr_output_init_render(OUTPUT, g_pCompositor->m_sWLRAllocator, g_pCompositor->m_sWLRRenderer);

    // get monitor rule that matches
    SMonitorRule monitorRule = g_pConfigManager->getMonitorRuleFor(OUTPUT->name);

    wlr_output_set_scale(OUTPUT, monitorRule.scale);
    wlr_xcursor_manager_load(g_pCompositor->m_sWLRXCursorMgr, monitorRule.scale);
    wlr_output_set_transform(OUTPUT, WL_OUTPUT_TRANSFORM_NORMAL);  // TODO: support other transforms

    wlr_output_set_mode(OUTPUT, wlr_output_preferred_mode(OUTPUT));
    wlr_output_enable_adaptive_sync(OUTPUT, 1);

    // create it in the arr
    newMonitor.vecPosition = monitorRule.offset;
    newMonitor.vecSize = monitorRule.resolution;
    newMonitor.refreshRate = monitorRule.refreshRate;

    g_pCompositor->m_lMonitors.push_back(newMonitor);
    const auto PNEWMONITOR = &g_pCompositor->m_lMonitors.back();

    PNEWMONITOR->hyprListener_monitorFrame.initCallback(&OUTPUT->events.frame, &Events::listener_monitorFrame, PNEWMONITOR);
    PNEWMONITOR->hyprListener_monitorDestroy.initCallback(&OUTPUT->events.destroy, &Events::listener_monitorDestroy, PNEWMONITOR);

    wlr_output_enable(OUTPUT, 1);
    if (!wlr_output_commit(OUTPUT)) {
        Debug::log(ERR, "Couldn't commit output named %s", OUTPUT->name);
        return;
    }

    // TODO: this doesn't seem to set the X and Y correctly,
    // wlr_output_layout_output_coords returns invalid values, I think...
    wlr_output_layout_add(g_pCompositor->m_sWLROutputLayout, OUTPUT, monitorRule.offset.x, monitorRule.offset.y);

    wlr_output_set_custom_mode(OUTPUT, OUTPUT->width, OUTPUT->height, (int)(round(monitorRule.refreshRate * 1000)));

    Debug::log(LOG, "Added new monitor with name %s at %i,%i with size %ix%i@%i, pointer %x", OUTPUT->name, (int)monitorRule.offset.x, (int)monitorRule.offset.y, (int)monitorRule.resolution.x, (int)monitorRule.resolution.y, (int)monitorRule.refreshRate, OUTPUT);

    // add a WLR workspace group
    PNEWMONITOR->pWLRWorkspaceGroupHandle = wlr_ext_workspace_group_handle_v1_create(g_pCompositor->m_sWLREXTWorkspaceMgr);

    // Workspace
    const auto WORKSPACEID = monitorRule.defaultWorkspaceID == -1 ? g_pCompositor->m_lWorkspaces.size() : monitorRule.defaultWorkspaceID;
    g_pCompositor->m_lWorkspaces.emplace_back(newMonitor.ID);
    const auto PNEWWORKSPACE = &g_pCompositor->m_lWorkspaces.back();

    // We are required to set the name here immediately
    wlr_ext_workspace_handle_v1_set_name(PNEWWORKSPACE->m_pWlrHandle, std::to_string(WORKSPACEID).c_str());

    PNEWWORKSPACE->m_iID = WORKSPACEID;
    PNEWWORKSPACE->m_iMonitorID = newMonitor.ID;

    PNEWMONITOR->activeWorkspace = PNEWWORKSPACE->m_iID;

    g_pCompositor->deactivateAllWLRWorkspaces();
    wlr_ext_workspace_handle_v1_set_active(PNEWWORKSPACE->m_pWlrHandle, true);
    //
}

void Events::listener_monitorFrame(void* owner, void* data) {
    SMonitor* const PMONITOR = (SMonitor*)owner;

    // Hack: only check when monitor number 1 refreshes, saves a bit of resources.
    // This is for stuff that should be run every frame
    // TODO: do this on the most Hz monitor
    if (PMONITOR->ID == 0) {
        g_pCompositor->sanityCheckWorkspaces();
        g_pAnimationManager->tick();
        g_pCompositor->cleanupWindows();

        g_pConfigManager->dispatchExecOnce(); // We exec-once when at least one monitor starts refreshing, meaning stuff has init'd
    }

    timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);

    if (!wlr_output_attach_render(PMONITOR->output, nullptr))
        return;

    g_pHyprOpenGL->begin(PMONITOR);
    g_pHyprOpenGL->clear(CColor(11, 11, 11, 255));
    g_pHyprOpenGL->clearWithTex(); // will apply the hypr "wallpaper"

    g_pHyprRenderer->renderAllClientsForMonitor(PMONITOR->ID, &now);

    wlr_renderer_begin(g_pCompositor->m_sWLRRenderer, PMONITOR->vecSize.x, PMONITOR->vecSize.y);

    wlr_output_render_software_cursors(PMONITOR->output, NULL);

    wlr_renderer_end(g_pCompositor->m_sWLRRenderer);

    g_pHyprOpenGL->end();

    wlr_output_commit(PMONITOR->output);
}

void Events::listener_monitorDestroy(void* owner, void* data) {
    const auto OUTPUT = (wlr_output*)data;

    SMonitor* pMonitor = nullptr;

    for (auto& m : g_pCompositor->m_lMonitors) {
        if (m.szName == OUTPUT->name) {
            pMonitor = &m;
            break;
        }
    }

    if (!pMonitor)
        return;

    g_pCompositor->m_lMonitors.remove(*pMonitor);

    // TODO: cleanup windows
}
