#include "SceneSelection.hpp"
#include "../../desktop/state/Fadeout.hpp"

bool Render::sceneIncludesShell(eSceneMode mode) {
    return mode != eSceneMode::WORKSPACE_WINDOWS;
}

bool Render::sceneSelectsWindow(eSceneMode mode, const SSceneWindowState& window) {
    if (mode == eSceneMode::MONITOR)
        return window.monitorVisible;

    const bool OUTPUT_GLOBAL = mode == eSceneMode::WORKSPACE_WITH_SHELL && window.floating && window.pinned && window.onOwnerMonitor;
    return window.mapped && !window.hidden && (window.belongsToWorkspace || OUTPUT_GLOBAL);
}

bool Render::sceneSelectsFadeout(eSceneMode mode, Desktop::eFadeoutSource source, bool belongsToWorkspace, bool pinnedOnMonitor) {
    if (mode == eSceneMode::MONITOR)
        return true;

    switch (source) {
        case Desktop::eFadeoutSource::WINDOW: return belongsToWorkspace || (mode == eSceneMode::WORKSPACE_WITH_SHELL && pinnedOnMonitor);
        case Desktop::eFadeoutSource::LAYER: return sceneIncludesShell(mode);
        default: return false;
    }
}

Desktop::eFadeoutPlane Render::sceneFadeoutPlane(eSceneMode mode, Desktop::eFadeoutPlane plane, bool pinned) {
    if (mode != eSceneMode::MONITOR && pinned && (plane == Desktop::FADEOUT_PLANE_WINDOW_FLOATING || plane == Desktop::FADEOUT_PLANE_WINDOW_OVER_FULLSCREEN))
        return Desktop::FADEOUT_PLANE_WINDOW_PINNED;

    return plane;
}
