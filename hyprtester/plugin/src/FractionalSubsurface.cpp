#include "FractionalSubsurface.hpp"
#include "globals.hpp"

#include <src/desktop/state/WindowState.hpp>
#include <src/desktop/view/window/Window.hpp>
#include <src/desktop/view/window/WindowMetadata.hpp>
#include <src/event/EventBus.hpp>
#include <src/output/Monitor.hpp>
#include <src/render/Context.hpp>
#include <src/render/OpenGL.hpp>
#include <src/render/Renderer.hpp>
#include <src/render/gl/GLFramebuffer.hpp>

#include <array>

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

struct SEdgeProbe {
    CHyprSignalListener            stage;
    SP<Render::GL::CGLFramebuffer> framebuffer;
    CBox                           content;
    bool                           complete = false;
    std::string                    error;
};

static SP<SEdgeProbe> g_probe;

static std::string    sampleEdges(const SEdgeProbe& probe, bool red) {
    // The output retains undamaged pixels; the scene work buffer is cleared each frame.
    const Render::GL::CFramebufferBindingGuard bindings{Render::GL::g_pHyprOpenGL};
    Render::GL::g_pHyprOpenGL->bindFramebuffer(GL_READ_FRAMEBUFFER, probe.framebuffer->getFBID());
    const auto&                   content = probe.content;
    const std::array<Vector2D, 4> POINTS  = {{
        {content.x + content.w - 1, content.y + content.h / 2},
        {content.x + content.w / 2, content.y + content.h - 1},
        {content.x + content.w, content.y + content.h / 2},
        {content.x + content.w / 2, content.y + content.h},
    }};
    for (size_t i = 0; i < POINTS.size(); ++i) {
        std::array<uint8_t, 4> pixel = {};
        // With a normal output transform, the final blit preserves scene pixel coordinates.
        glReadPixels(sc<int>(POINTS[i].x), sc<int>(POINTS[i].y), 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel.data());
        // Content is green/red; the immediately adjoining one-pixel border is blue.
        const std::array<uint8_t, 3> expected = i >= 2 ? std::array<uint8_t, 3>{0, 0, 255} : red ? std::array<uint8_t, 3>{255, 0, 0} : std::array<uint8_t, 3>{0, 255, 0};
        if (pixel[0] != expected[0] || pixel[1] != expected[1] || pixel[2] != expected[2])
            return std::format("Edge pixel {} at {}: expected {},{},{}; got {},{},{}", i, POINTS[i], expected[0], expected[1], expected[2], pixel[0], pixel[1], pixel[2]);
    }
    return {};
}

static int arm(lua_State* L) {
    const bool RED = lua_toboolean(L, 1);
    PHLWINDOW  window;
    for (const auto& candidate : Desktop::windowState()->windows()) {
        if (candidate->mapped() && candidate->metadata().appID() == "fractional-subsurface") {
            window = candidate;
            break;
        }
    }
    if (!window || window->sizeAnimation()->isBeingAnimated() || window->backend().reportedSize() != window->size(Desktop::View::IGeometric::GEOMETRIC_CURRENT).floor())
        return luaL_error(L, "Fractional subsurface fixture not settled");
    const auto MONITOR = window->m_monitor.lock();
    if (!MONITOR || MONITOR->m_transform != WL_OUTPUT_TRANSFORM_NORMAL)
        return luaL_error(L, "Fractional subsurface probe requires an untransformed monitor");
    g_probe                   = makeShared<SEdgeProbe>();
    const WP<SEdgeProbe> WEAK = g_probe;
    g_probe->stage            = Event::bus()->m_events.render.stage.listen([WEAK, window, MONITOR, RED](Event::SRenderStageEvent event) {
        const auto PROBE = WEAK.lock();
        if (!PROBE || PROBE->complete || event.monitor != MONITOR)
            return;
        if (event.stage == RENDER_POST && PROBE->framebuffer) {
            PROBE->error    = sampleEdges(*PROBE, RED);
            PROBE->complete = true;
            PROBE->framebuffer.reset();
            return;
        }
        if (event.stage != RENDER_LAST_MOMENT || !event.context)
            return;
        if (window->sizeAnimation()->isBeingAnimated() || window->backend().reportedSize() != window->size(Desktop::View::IGeometric::GEOMETRIC_CURRENT).floor())
            return;
        PROBE->content = window->geometricBox(Desktop::View::IGeometric::GEOMETRIC_CURRENT);
        PROBE->content.translate(-MONITOR->m_position).scale(MONITOR->m_scale).round();
        // endRender() resets the context before RENDER_POST, so retain this frame's output.
        PROBE->framebuffer = dynamicPointerCast<Render::GL::CGLFramebuffer>(event.context->get().m_data.outFB);
        if (!PROBE->framebuffer) {
            PROBE->error    = "Fractional subsurface probe has no GL output framebuffer";
            PROBE->complete = true;
        }
    });
    // Trigger a frame without repainting the sampled edges: partial-update tests must
    // read what the client's own damage actually refreshed in the retained framebuffer.
    g_pHyprRenderer->damageBox(CBox{MONITOR->m_position, {1, 1}});
    return 0;
}

static int check(lua_State* L) {
    if (!g_probe || !g_probe->complete)
        return luaL_error(L, "Edge probe pending");
    const auto ERROR = g_probe->error;
    FractionalSubsurface::reset();
    if (!ERROR.empty())
        return luaL_error(L, "%s", ERROR.c_str());
    return 0;
}

void FractionalSubsurface::registerFunctions() {
    HyprlandAPI::addLuaFunction(PHANDLE, "test", "arm_subsurface_edges", arm);
    HyprlandAPI::addLuaFunction(PHANDLE, "test", "check_subsurface_edges", check);
}

void FractionalSubsurface::reset() {
    g_probe.reset();
}
