#include "AnimationTree.hpp"

using namespace Config;

UP<CAnimationTreeController>& Config::animationTree() {
    static UP<CAnimationTreeController> p = makeUnique<CAnimationTreeController>();
    return p;
}

CAnimationTreeController::CAnimationTreeController() {
    reset();
}

void CAnimationTreeController::reset() {
    m_animationTree.createNode("__internal_fadeCTM");
    m_animationTree.createNode("global");

    // global
    m_animationTree.createNode("windows", "global");
    m_animationTree.createNode("layers", "global");
    m_animationTree.createNode("fade", "global");
    m_animationTree.createNode("border", "global");
    m_animationTree.createNode("borderangle", "global");
    m_animationTree.createNode("workspaces", "global");
    m_animationTree.createNode("zoomFactor", "global");
    m_animationTree.createNode("monitorAdded", "global");

    // layer
    m_animationTree.createNode("layersIn", "layers");
    m_animationTree.createNode("layersOut", "layers");

    // windows
    m_animationTree.createNode("windowsIn", "windows");
    m_animationTree.createNode("windowsOut", "windows");
    m_animationTree.createNode("windowsMove", "windows");

    // fade
    m_animationTree.createNode("fadeIn", "fade");
    m_animationTree.createNode("fadeOut", "fade");
    m_animationTree.createNode("fadeSwitch", "fade");
    m_animationTree.createNode("fadeShadow", "fade");
    m_animationTree.createNode("fadeDim", "fade");
    m_animationTree.createNode("fadeLayers", "fade");
    m_animationTree.createNode("fadeLayersIn", "fadeLayers");
    m_animationTree.createNode("fadeLayersOut", "fadeLayers");
    m_animationTree.createNode("fadePopups", "fade");
    m_animationTree.createNode("fadePopupsIn", "fadePopups");
    m_animationTree.createNode("fadePopupsOut", "fadePopups");
    m_animationTree.createNode("fadeDpms", "fade");

    // workspaces
    m_animationTree.createNode("workspacesIn", "workspaces");
    m_animationTree.createNode("workspacesOut", "workspaces");
    m_animationTree.createNode("specialWorkspace", "workspaces");
    m_animationTree.createNode("specialWorkspaceIn", "specialWorkspace");
    m_animationTree.createNode("specialWorkspaceOut", "specialWorkspace");

    // init the root nodes
    m_animationTree.setConfigForNode("global", 1, 8.f, "default");
    m_animationTree.setConfigForNode("__internal_fadeCTM", 1, 5.f, "linear");
    m_animationTree.setConfigForNode("borderangle", 0, 1, "default");
}

const std::unordered_map<std::string, SP<Hyprutils::Animation::SAnimationPropertyConfig>>& CAnimationTreeController::getAnimationConfig() {
    return m_animationTree.getFullConfig();
}

SP<Hyprutils::Animation::SAnimationPropertyConfig> CAnimationTreeController::getAnimationPropertyConfig(const std::string& name) {
    return m_animationTree.getConfig(name);
}

void CAnimationTreeController::setConfigForNode(const std::string& name, bool enabled, float speed, const std::string& bezier, const std::string& style) {
    m_animationTree.setConfigForNode(name, enabled, speed, bezier, style);
}

bool CAnimationTreeController::nodeExists(const std::string& name) {
    return m_animationTree.nodeExists(name);
}
