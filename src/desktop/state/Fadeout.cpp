#include "Fadeout.hpp"
#include "../../render/scene/SceneSelection.hpp"

using namespace Desktop;
using namespace Desktop::View;

SP<Render::IFramebuffer> IFadeout::framebuffer() const {
    return m_framebuffer;
}

SP<Render::IFramebuffer> IFadeout::framebuffer(Render::eSceneMode mode) const {
    if (mode == Render::eSceneMode::MONITOR)
        return m_framebuffer;

    const auto SOURCE = source();
    if (SOURCE.type == eFadeoutSource::WINDOW)
        return m_workspaceFramebuffer;

    if (SOURCE.type == eFadeoutSource::LAYER && Render::sceneIncludesShell(mode))
        return m_framebuffer;

    return nullptr;
}

PHLWORKSPACEREF IFadeout::workspace() const {
    return m_workspace;
}

SFadeoutRenderEffects IFadeout::effects() const {
    return m_effects;
}

SFadeoutSource IFadeout::source() const {
    if (m_source.type != eFadeoutSource::UNKNOWN)
        return m_source;

    switch (plane()) {
        case FADEOUT_PLANE_WINDOW_TILED:
        case FADEOUT_PLANE_WINDOW_FLOATING:
        case FADEOUT_PLANE_WINDOW_OVER_FULLSCREEN: return {.type = eFadeoutSource::WINDOW, .workspace = m_workspace};
        case FADEOUT_PLANE_LAYER_BACKGROUND:
        case FADEOUT_PLANE_LAYER_BOTTOM:
        case FADEOUT_PLANE_LAYER_TOP:
        case FADEOUT_PLANE_LAYER_OVERLAY: return {.type = eFadeoutSource::LAYER};
        default: return {};
    }
}
