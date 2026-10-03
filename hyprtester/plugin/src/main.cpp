#include "state/workspace/Resolver.hpp"
#include <unistd.h>
#include <src/includes.hpp>
#include <sstream>
#include <any>
#include <cmath>
#include <cstring>
#include <vector>
#include <array>
#include <tuple>

#define private   public
#define protected public
#include <src/render/Renderer.hpp>
#include <src/render/SceneResources.hpp>
#include <src/output/MonitorResources.hpp>
#include <src/managers/input/InputManager.hpp>
#include <src/pointer/PointerManager.hpp>
#include <src/pointer/cursor/CursorManager.hpp>
#include <src/render/pass/TexPassElement.hpp>
#include <src/render/Texture.hpp>
#include <src/pointer/PointerController.hpp>
#include <src/managers/SeatManager.hpp>
#include <src/managers/input/trackpad/TrackpadGestures.hpp>
#include <src/output/Monitor.hpp>
#include <src/desktop/rule/windowRule/WindowRuleEffectContainer.hpp>
#include <src/desktop/rule/layerRule/LayerRuleEffectContainer.hpp>
#include <src/desktop/rule/windowRule/WindowRuleApplicator.hpp>
#include <src/desktop/view/LayerSurface.hpp>
#include <src/desktop/view/window/WindowFullscreenPolicy.hpp>
#include <src/desktop/view/window/WindowPresentation.hpp>
#include <src/desktop/state/ViewState.hpp>
#include <src/desktop/state/WindowState.hpp>
#include <src/workspace/HLWorkspace.hpp>
#include <src/layout/target/Target.hpp>
#include <src/layout/target/WindowTarget.hpp>
#include <src/keybinds/Key.hpp>
#include <src/Compositor.hpp>
#include <src/desktop/state/FocusState.hpp>
#include <src/state/MonitorState.hpp>
#include <src/state/workspace/State.hpp>
#include <src/layout/LayoutManager.hpp>
#include <src/event/EventBus.hpp>
#undef private
#undef protected

#include <src/config/ConfigValue.hpp>
#include <src/config/shared/animation/AnimationTree.hpp>
#include <src/animation/AnimationManager.hpp>
#include <src/render/WindowRenderPresentation.hpp>
#include <src/render/scene/SceneSelection.hpp>
#include <hyprutils/utils/ScopeGuard.hpp>
#include <hyprutils/string/Numeric.hpp>
#include <hyprutils/string/VarList.hpp>
using namespace Hyprutils::Utils;
using namespace Hyprutils::String;

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

#include "globals.hpp"
#include "WorkspaceGestures.hpp"
#include "SpecialWorkspaceGestures.hpp"
#include "XWaylandSelection.hpp"

// Do NOT change this function.
APICALL EXPORT std::string PLUGIN_API_VERSION() {
    return HYPRLAND_API_VERSION;
}

static SDispatchResult test(std::string in) {
    return {.success = true};
}

// Trigger a snap move event for the active window
static SDispatchResult snapMove(std::string in) {
    const auto PLASTWINDOW = Desktop::focusState()->window();
    if (!PLASTWINDOW->isFloating())
        return {.success = false, .error = "Window must be floating"};

    Vector2D pos  = PLASTWINDOW->position(Desktop::View::IGeometric::GEOMETRIC_GOAL);
    Vector2D size = PLASTWINDOW->size(Desktop::View::IGeometric::GEOMETRIC_GOAL);

    g_layoutManager->performSnap(pos, size, PLASTWINDOW->layoutTarget(), MBIND_MOVE, -1, size);

    PLASTWINDOW->layoutTarget()->setPositionGlobal(CBox{pos, size});

    return {};
}

static PHLWINDOW windowByClass(const std::string& cls) {
    for (const auto& window : Desktop::windowState()->windows()) {
        if (window->metadata().appID() == cls)
            return window;
    }

    return nullptr;
}

static SDispatchResult expectWindowAtWorkspace(const std::string& workspaceSelector, const Vector2D& pos, const std::string& expectedClass, const std::string& ignoreClass) {
    const auto WORKSPACE = State::Workspace::state()->find(State::Workspace::resolver()->getWorkspaceTargetFromString(workspaceSelector));
    if (!WORKSPACE)
        return {.success = false, .error = std::format("No workspace matching '{}'", workspaceSelector)};

    const auto IGNORE = ignoreClass.empty() ? nullptr : windowByClass(ignoreClass);
    if (!ignoreClass.empty() && !IGNORE)
        return {.success = false, .error = std::format("No window with class '{}' to ignore", ignoreClass)};

    const auto WINDOW =
        Desktop::viewState()->hitTest().windowAtWorkspace(pos, WORKSPACE, Desktop::View::RESERVED_EXTENTS | Desktop::View::INPUT_EXTENTS | Desktop::View::ALLOW_FLOATING, IGNORE);

    if (!WINDOW)
        return {.success = false, .error = std::format("Expected window '{}', got no window", expectedClass)};
    if (WINDOW->metadata().appID() != expectedClass)
        return {.success = false, .error = std::format("Expected window '{}', got '{}'", expectedClass, WINDOW->metadata().appID())};

    return {};
}

static SDispatchResult expectWorkspaceRenameEvent(const std::string& workspaceSelector, const std::string& name) {
    const auto WORKSPACE = State::Workspace::state()->find(State::Workspace::resolver()->getWorkspaceTargetFromString(workspaceSelector));
    if (!WORKSPACE)
        return {.success = false, .error = std::format("No workspace matching '{}'", workspaceSelector)};

    size_t          eventCount = 0;
    PHLWORKSPACEREF eventWorkspace;
    std::string     eventName;
    const auto      LISTENER = Event::bus()->m_events.workspace.renamed.listen([&](PHLWORKSPACEREF workspace) {
        ++eventCount;
        eventWorkspace = workspace;
        if (const auto WORKSPACE = workspace.lock())
            eventName = WORKSPACE->m_name;
    });

    WORKSPACE->rename(name);

    if (eventCount != 1)
        return {.success = false, .error = std::format("Expected one workspace rename event, got {}", eventCount)};
    if (eventWorkspace.lock() != WORKSPACE)
        return {.success = false, .error = "Workspace rename event carried the wrong workspace"};
    if (eventName != name)
        return {.success = false, .error = std::format("Workspace rename event observed name '{}', expected '{}'", eventName, name)};

    return {};
}

struct SMonitorRenderRecording {
    PHLMONITORREF             monitor;
    std::vector<eRenderStage> stages;
    bool                      complete = false;
    std::optional<bool>       workspaceProbe;
    SDispatchResult           result;
    CHyprSignalListener       stage;
};

static SP<SMonitorRenderRecording> g_monitorRenderRecording;

struct SCursorFixture {
    std::string theme;
    std::string name;
    int         size = 24;
    Vector2D    position;
};

struct SCursorRenderRecording {
    bool                complete = false;
    SDispatchResult     result;
    CHyprSignalListener stage;
};

static std::optional<SCursorFixture> g_cursorFixture;
static SP<SCursorRenderRecording>    g_cursorRenderRecording;

static void                          restoreCursorFixture() {
    g_cursorRenderRecording.reset();
    if (!g_cursorFixture)
        return;

    const auto OLD = *g_cursorFixture;
    g_cursorFixture.reset();
    Pointer::Cursor::mgr()->changeTheme(OLD.theme, OLD.size);
    Pointer::Cursor::mgr()->setCursorFromName(OLD.name);
    Pointer::mgr()->warpTo(OLD.position);
}

static SDispatchResult startCursorFixture(const std::string& theme) {
    if (g_cursorFixture)
        return {.success = false, .error = "Cursor fixture already active"};

    auto& cursor    = *Pointer::Cursor::mgr();
    g_cursorFixture = SCursorFixture{
        cursor.m_theme,
        cursor.m_lastCursorName,
        cursor.m_size,
        Pointer::mgr()->untransformedPosition(),
    };
    cursor.changeTheme(theme, 24);
    if (!cursor.m_hyprcursor || !cursor.m_hyprcursor->valid())
        return {.success = false, .error = "Failed to load the hyprcursor fixture"};

    // Straddle both outputs without client input changing the named image.
    Pointer::mgr()->warpTo({5798, 100});
    cursor.setCursorFromName("default");
    return {};
}

static SDispatchResult probeCursorRender(Render::CRenderContext& ctx, PHLMONITOR monitor, int pixels) {
    auto&       pointer = *Pointer::mgr();
    auto&       entries = g_pHyprRenderer->currentPass(ctx).m_passElements;
    const auto  BEGIN   = entries.size();
    CScopeGuard restore([&] { entries.resize(BEGIN); });
    CRegion     damage{CBox{{}, monitor->m_transformedSize}};

    // The forced screencopy path queues the same texture/box, regardless of HW
    // support, and leaves the normal software-rendered bookkeeping untouched.
    pointer.renderSoftwareCursorsFor(ctx, monitor, Time::steadyNow(), damage, std::nullopt, true, true);
    if (pixels == 0) {
        if (entries.size() != BEGIN || pointer.hasCursor() || !pointer.m_cursorImages.empty() || pointer.m_currentCursorImage.bufferTex)
            return {.success = false, .error = "Cleared cursor retained an image or queued a texture"};
        return {};
    }

    if (entries.size() != BEGIN + 1 || entries[BEGIN].element->type() != EK_TEXTURE)
        return {.success = false, .error = "Expected exactly one cursor texture pass element"};

    const auto& DATA  = sc<CTexPassElement*>(entries[BEGIN].element.get())->m_data;
    auto&       image = pointer.cursorImageForMonitor(monitor);
    const auto  SIZE  = Vector2D{pixels, pixels};
    if (!DATA.tex || DATA.tex->m_size != SIZE || image.size != SIZE || image.scale != monitor->m_scale)
        return {.success = false,
                .error   = std::format("{}: expected {}px at scale {}, got image {}x{} at scale {}, texture {}x{}", monitor->m_name, pixels, monitor->m_scale, image.size.x,
                                       image.size.y, image.scale, DATA.tex ? DATA.tex->m_size.x : 0, DATA.tex ? DATA.tex->m_size.y : 0)};

    if (DATA.tex != pointer.cursorTextureForImage(image))
        return {.success = false, .error = "Cursor render did not use the output's resolved texture"};

    // Cairo ARGB32, copied by createTexture(..., true). Check every pixel so
    // matching dimensions alone cannot disguise the wrong theme or a resample.
    const auto& BYTES = DATA.tex->dataCopy();
    const auto  COLOR = pixels == 24 ? 0xFFFF0000U : 0xFF00FF00U;
    if (BYTES.size() != sc<size_t>(pixels * pixels * 4))
        return {.success = false, .error = "Cursor texture has no complete CPU data copy"};
    for (size_t i = 0; i < BYTES.size(); i += 4) {
        uint32_t pixel = 0;
        std::memcpy(&pixel, BYTES.data() + i, sizeof(pixel));
        if (pixel != COLOR)
            return {.success = false, .error = std::format("{}: unexpected cursor pixel {:#x}, expected {:#x}", monitor->m_name, pixel, COLOR)};
    }

    // Derive the oracle from fixture metadata, not the image geometry helpers.
    // Even at 1.25, resize_algorithm=none must draw 32 native pixels, not 30.
    const auto POS      = (pointer.untransformedPosition() - monitor->m_position) * monitor->m_scale;
    const auto EXPECTED = CBox{std::round(POS.x - pixels / 4.0), std::round(POS.y - pixels / 2.0), sc<double>(pixels), sc<double>(pixels)};
    if (DATA.box != EXPECTED)
        return {.success = false,
                .error   = std::format("{}: expected cursor box {},{}/{}x{}, got {},{}/{}x{}", monitor->m_name, EXPECTED.x, EXPECTED.y, EXPECTED.w, EXPECTED.h, DATA.box.x,
                                       DATA.box.y, DATA.box.w, DATA.box.h)};
    return {};
}

static SDispatchResult armCursorRender(const std::string& name, int pixels) {
    g_cursorRenderRecording.reset();
    const auto MONITOR = State::monitorState()->query().name(name).run();
    if (!g_cursorFixture || !MONITOR || !MONITOR->m_enabled || (pixels != 0 && pixels != 24 && pixels != 32))
        return {.success = false, .error = "Invalid cursor probe fixture, monitor or size"};

    g_cursorRenderRecording                 = makeShared<SCursorRenderRecording>();
    const WP<SCursorRenderRecording> WEAK   = g_cursorRenderRecording;
    const PHLMONITORREF              TARGET = MONITOR;
    g_cursorRenderRecording->stage          = Event::bus()->m_events.render.stage.listen([WEAK, TARGET, pixels](Event::SRenderStageEvent event) {
        const auto RECORDING = WEAK.lock();
        const auto MONITOR   = TARGET.lock();
        if (!RECORDING || RECORDING->complete || !MONITOR || event.stage != RENDER_BEGIN || event.monitor != MONITOR || !event.context)
            return;

        RECORDING->complete = true;
        RECORDING->result   = probeCursorRender(event.context->get(), MONITOR, pixels);
    });
    g_pHyprRenderer->damageMonitor(MONITOR);
    MONITOR->scheduleFrame(Aquamarine::IOutput::AQ_SCHEDULE_DAMAGE);
    return {};
}

static SDispatchResult probeWorkspaceBackground(Render::CRenderContext& ctx, PHLMONITOR monitor, bool withWorkspace);

static SDispatchResult armMonitorRenderRecording(const std::string& name, std::optional<bool> workspaceProbe = std::nullopt) {
    g_monitorRenderRecording.reset();

    // Mirrors are absent from the active monitor list.
    const auto MONITOR = State::monitorState()->query().name(name).includeDisabled(true).run();
    if (!MONITOR || !MONITOR->m_enabled)
        return {.success = false, .error = std::format("No enabled monitor named '{}'", name)};

    g_monitorRenderRecording                         = makeShared<SMonitorRenderRecording>();
    g_monitorRenderRecording->monitor                = MONITOR;
    g_monitorRenderRecording->workspaceProbe         = workspaceProbe;
    const WP<SMonitorRenderRecording> WEAK_RECORDING = g_monitorRenderRecording;

    g_monitorRenderRecording->stage = Event::bus()->m_events.render.stage.listen([WEAK_RECORDING](Event::SRenderStageEvent event) {
        const auto stage     = event.stage;
        const auto RECORDING = WEAK_RECORDING.lock();
        if (!RECORDING || RECORDING->complete || RECORDING->monitor.expired() || event.monitor != RECORDING->monitor)
            return;

        if (RECORDING->workspaceProbe) {
            if (stage == RENDER_BEGIN && event.context) {
                RECORDING->complete = true; // Ignore the probe's nested stage emissions.
                RECORDING->result   = probeWorkspaceBackground(event.context->get(), RECORDING->monitor.lock(), *RECORDING->workspaceProbe);
            }
            return;
        }

        RECORDING->stages.emplace_back(stage);
        // Freeze the first completed frame, so later frames cannot hide a routing failure.
        RECORDING->complete = stage == RENDER_POST;
    });

    // The renderer's damageMonitor ignores mirrors. Damage the queried output
    // directly so even an already-scheduled frame must take the damaged branch.
    MONITOR->addDamage(CBox{0, 0, MONITOR->m_transformedSize.x, MONITOR->m_transformedSize.y});
    MONITOR->scheduleFrame(Aquamarine::IOutput::AQ_SCHEDULE_DAMAGE);
    return {};
}

static std::string describeRenderStages(const std::vector<eRenderStage>& stages) {
    std::string result;
    for (const auto stage : stages) {
        if (!result.empty())
            result += " -> ";

        switch (stage) {
            case RENDER_PRE: result += "PRE"; break;
            case RENDER_BEGIN: result += "BEGIN"; break;
            case RENDER_POST_WALLPAPER: result += "POST_WALLPAPER"; break;
            case RENDER_PRE_WINDOWS: result += "PRE_WINDOWS"; break;
            case RENDER_POST_WINDOWS: result += "POST_WINDOWS"; break;
            case RENDER_LAST_MOMENT: result += "LAST_MOMENT"; break;
            case RENDER_POST: result += "POST"; break;
            case RENDER_POST_MIRROR: result += "POST_MIRROR"; break;
            case RENDER_PRE_WINDOW: result += "PRE_WINDOW"; break;
            case RENDER_POST_WINDOW: result += "POST_WINDOW"; break;
            default: result += std::format("unknown({})", sc<int>(stage)); break;
        }
    }
    return result;
}

static SDispatchResult probeSceneResources(Render::CRenderContext& ctx, PHLMONITOR monitor) {
    using namespace Render;
    auto&                        renderer  = *g_pHyprRenderer;
    const auto                   resources = monitor->resources();
    GL::CFramebufferBindingGuard bindings{renderer.glBackend()}; // Outlive every allocation and release below.
    if (!monitor->m_mirrors.empty() && !resources->hasMirrorFB())
        return {.success = false, .error = "Scene resources: mirrored output requires a warmed monitor mirror cache"};
    const auto TEXTURE_STATE = [](const SP<ITexture>& tex) {
        return std::tuple{tex, tex ? tex->m_texID : 0U, tex ? tex->m_size : Vector2D{}, tex ? tex->m_drmFormat : 0U, tex ? glIsTexture(tex->m_texID) : GL_FALSE};
    };
    const auto FB_STATE = [&](const SP<IFramebuffer>& fb) {
        return std::make_tuple(fb, fb && fb->isAllocated(), fb ? fb->m_size : Vector2D{}, fb ? fb->m_drmFormat : DRM_FORMAT_INVALID, TEXTURE_STATE(fb ? fb->getTexture() : nullptr),
                               TEXTURE_STATE(fb ? fb->getMirrorTexture() : nullptr));
    };
    const auto STATE = [&] {
        return std::make_tuple(FB_STATE(resources->m_blurFB), FB_STATE(resources->m_monitorMirrorFB), TEXTURE_STATE(resources->m_mirrorTex), resources->sceneResources(),
                               monitor->m_blurFBDirty, monitor->m_blurFBShouldRender, resources->m_mirrorFBValid, resources->m_mirrorFBNeedsFullRefresh, ctx.sceneResources(),
                               ctx.active(), ctx.m_frameTime, ctx.m_currentPass, ctx.m_pass.m_passElements.size(), renderer.currentPass(ctx).m_passElements.size(), ctx.m_mode,
                               ctx.m_currentBuffer, ctx.m_currentRenderbuffer, ctx.m_swapchainAcquired, ctx.m_data.pMonitor, ctx.m_data.currentFB, ctx.m_data.mainFB,
                               ctx.m_data.outFB, ctx.m_data.fbSize, ctx.m_data.targetProjection, ctx.m_usedAsyncBuffers.size(), ctx.m_backdropCaptures.size());
    };
    const auto OLD_STATE = STATE();
    const auto OLD_STALE = resources->m_mirrorFBStaleDamage.copy();
    const auto OLD_DATA  = ctx.m_data;
    {
        const auto     owner = makeShared<CSceneResources>(renderer.createFB("Scene resources probe"));
        CRenderContext isolated;
        if (!isolated.begin(owner) || !isolated.readOnlyEffects() || isolated.effectTime() != isolated.m_frameTime || owner->canPrecomputeBlur() ||
            renderer.getBlurTexture(isolated))
            return {.success = false, .error = "Scene resources: unprepared private cache was readable or ready"};
        isolated.m_data.pMonitor = monitor;
        isolated.m_data.fbSize   = ctx.m_data.fbSize;
        if (!resources->prepareSceneResources(*owner) || !owner->canPrecomputeBlur() || !owner->blurDirty() || owner->blurQueued() || renderer.getBlurTexture(isolated) ||
            owner->blurFramebuffer() == resources->m_blurFB || owner->blurFramebuffer()->m_size != resources->m_size ||
            owner->blurFramebuffer()->m_drmFormat != resources->m_drmFormat || owner->blurFramebuffer()->imageDescription() != resources->m_imageDescription)
            return {.success = false, .error = "Scene resources: private allocation/readiness did not use monitor geometry and format"};

        for (const bool SIZED : {false, true}) {
            const auto SIZE    = SIZED ? std::optional<Vector2D>{{37, 23}} : std::nullopt;
            auto       scratch = SIZED ? resources->getUnusedWorkBuffer(*SIZE) : resources->getUnusedWorkBuffer();
            if (!scratch || !scratch->isAllocated() || scratch == ctx.m_data.currentFB || scratch == resources->m_monitorMirrorFB || scratch == resources->m_blurFB)
                return {.success = false, .error = "Scene resources: no independent scratch framebuffer"};
            const WP<IFramebuffer> BORROWED = scratch;
            const auto             mirror   = renderer.createTexture();
            mirror->allocate(scratch->m_size, scratch->m_drmFormat);
            const auto  MIRROR_ID = mirror->m_texID;
            CScopeGuard detach([&] {
                if (const auto FB = BORROWED.lock(); FB && FB->getMirrorTexture() == mirror)
                    FB->disableMirror();
            });
            scratch->enableMirror(mirror);
            scratch->bind();
            GLint attachment = 0;
            glGetFramebufferAttachmentParameteriv(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, &attachment);
            if (!scratch->isAllocated() || scratch->getMirrorTexture() != mirror || sc<GLuint>(attachment) != MIRROR_ID ||
                glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
                return {.success = false, .error = "Scene resources: disposable mirror was not attached"};
            scratch.reset(); // The weak identity must not prevent pool reuse.
            scratch = renderer.getWorkBuffer(isolated, SIZE);
            if (!scratch || scratch != BORROWED.lock() || !scratch->isAllocated() || scratch->getMirrorTexture() || scratch->m_size != SIZE.value_or(resources->m_size) ||
                scratch->m_drmFormat != resources->m_drmFormat || scratch->imageDescription() != resources->m_imageDescription)
                return {.success = false, .error = "Scene resources: isolated borrowing did not detach/reuse the scratch framebuffer"};
            scratch->bind();
            glGetFramebufferAttachmentParameteriv(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, &attachment);
            if (attachment != GL_NONE || glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE || mirror->m_texID != MIRROR_ID || !glIsTexture(MIRROR_ID) ||
                mirror->m_size != scratch->m_size || mirror->m_drmFormat != scratch->m_drmFormat)
                return {.success = false, .error = "Scene resources: GL mirror attachment survived or disposable texture was damaged"};
        }
        owner->setBlurQueued(true);
        owner->completePreBlur(); // Bookkeeping only: these uninitialized pixels must never be sampled.
        if (owner->blurDirty() || owner->blurQueued() || !isolated.readOnlyEffects() || isolated.effectTime() != isolated.m_frameTime ||
            renderer.getBlurTexture(isolated) != owner->blurFramebuffer()->getTexture() || !renderer.getBlurTexture(isolated) ||
            renderer.getBlurTexture(isolated) == resources->m_blurFB->getTexture())
            return {.success = false, .error = "Scene resources: completed private cache did not route its own texture"};
    }
    if (STATE() != OLD_STATE || !pixman_region32_equal(OLD_STALE.pixman(), resources->m_mirrorFBStaleDamage.pixman()) ||
        !pixman_region32_equal(OLD_DATA.damage.pixman(), ctx.m_data.damage.pixman()) || !pixman_region32_equal(OLD_DATA.finalDamage.pixman(), ctx.m_data.finalDamage.pixman()))
        return {.success = false, .error = "Scene resources: monitor cache/mirror state or caller pass/session/damage changed"};
    return {};
}

static SDispatchResult probeWorkspaceBackground(Render::CRenderContext& ctx, PHLMONITOR monitor, bool withWorkspace) {
    if (!monitor->m_activeWorkspace || std::ranges::any_of(Desktop::windowState()->windows(), [](const auto& window) { return window->mapped(); }) ||
        !Desktop::layerState()->layers().empty())
        return {.success = false, .error = "Background probe requires an active workspace and no mapped clients or layers"};

    if (const auto RESULT = probeSceneResources(ctx, monitor); !RESULT.success)
        return RESULT;

    auto&                     renderer  = *g_pHyprRenderer;
    auto&                     entries   = ctx.m_pass.m_passElements;
    const auto                BEGIN     = entries.size();
    const auto                OLD_DATA  = ctx.m_data;
    const auto                ROOT_PASS = renderer.redirectPass(ctx, nullptr);
    CScopeGuard               restore([&] {
        entries.resize(BEGIN);
        ctx.m_data = OLD_DATA;
    });

    std::vector<eRenderStage> stages;
    bool                      backgroundBeforeEvent = true;
    const auto                LISTENER              = Event::bus()->m_events.render.stage.listen([&](Event::SRenderStageEvent event) {
        const auto stage = event.stage;
        stages.emplace_back(stage);
        if (stage == RENDER_POST_WALLPAPER)
            backgroundBeforeEvent &= entries.size() == BEGIN + 1 && entries[BEGIN].element->type() == EK_CLEAR;
    });
    renderer.renderAllClientsForWorkspace(ctx, monitor, withWorkspace ? monitor->m_activeWorkspace : nullptr, Time::steadyNow());

    const bool                WALLPAPER = !withWorkspace || !*CConfigValue<Config::INTEGER>("render:xp_mode");
    std::vector<eRenderStage> expected;
    if (WALLPAPER)
        expected.emplace_back(RENDER_POST_WALLPAPER);
    if (withWorkspace) {
        expected.emplace_back(RENDER_PRE_WINDOWS);
        expected.emplace_back(RENDER_POST_WINDOWS);
    }
    const auto CLEARS = std::count_if(entries.begin() + BEGIN, entries.end(), [](const auto& entry) { return entry.element->type() == EK_CLEAR; });
    if (stages != expected || !backgroundBeforeEvent || CLEARS != (WALLPAPER ? 1 : 0))
        return {.success = false,
                .error   = std::format("Background probe: expected [{}], got [{}], clears {}, background before event {}", describeRenderStages(expected),
                                       describeRenderStages(stages), CLEARS, backgroundBeforeEvent)};
    return {};
}

static SDispatchResult checkMonitorRenderRecording(bool mirror) {
    if (!g_monitorRenderRecording)
        return {.success = false, .error = "Monitor render recorder is not armed"};
    if (g_monitorRenderRecording->monitor.expired())
        return {.success = false, .error = "Recorded monitor disappeared"};
    if (!g_monitorRenderRecording->complete)
        return {.success = false, .error = std::format("Monitor render pending: [{}]", describeRenderStages(g_monitorRenderRecording->stages))};
    if (g_monitorRenderRecording->workspaceProbe)
        return g_monitorRenderRecording->result;

    // These test outputs have empty workspaces: require the entire frame, with no
    // duplicate stages or workspace stages leaking into the mirror path.
    const std::vector<eRenderStage> EXPECTED = mirror ?
        std::vector<eRenderStage>{
            RENDER_PRE, RENDER_BEGIN, RENDER_POST_MIRROR, RENDER_LAST_MOMENT, RENDER_POST,
        } :
        std::vector<eRenderStage>{
            RENDER_PRE, RENDER_BEGIN, RENDER_POST_WALLPAPER, RENDER_PRE_WINDOWS, RENDER_POST_WINDOWS, RENDER_LAST_MOMENT, RENDER_POST,
        };

    if (g_monitorRenderRecording->stages != EXPECTED)
        return {.success = false,
                .error   = std::format("Expected {} frame [{}], recorded [{}]", mirror ? "mirror" : "normal", describeRenderStages(EXPECTED),
                                       describeRenderStages(g_monitorRenderRecording->stages))};

    return {};
}

struct SPopupRenderRecording {
    bool                complete = false;
    SDispatchResult     result   = {.success = false, .error = "Waiting for a render frame"};
    CHyprSignalListener stage;
};

static SP<SPopupRenderRecording> g_popupRenderRecording;

static SDispatchResult           probePopupOpacity(Render::CRenderContext& ctx, PHLWINDOW window, float parentFade, Render::eRenderPassMode mode, bool redirected,
                                                   const std::string& presentationMode, bool& ready) {
    using namespace Desktop::View;

    // Only fixture readiness may retry; all assertion failures are terminal.
    ready = true;

    const auto NOT_READY = [&ready](const std::string& reason) {
        ready = false;
        return SDispatchResult{.success = false, .error = reason};
    };
    if (!window->mapped() || !window->m_monitor || !window->m_workspace || !window->wlSurface()->resource() || !window->wlSurface()->resource()->m_current.texture)
        return NOT_READY("Parent surface is not ready");

    // The client has two direct siblings, identified by their distinct buffer widths.
    auto popups = window->popupHead()->m_children;
    if (popups.size() != 2)
        return {.success = false, .error = std::format("Expected two popup siblings, got {}", popups.size())};

    for (const auto& popup : popups) {
        if (popup->m_parent != window->popupHead())
            return {.success = false, .error = "Unexpected popup parent"};
        if (!popup->mapped() || !popup->acceptsInput() || !popup->wlSurface()->resource() || !popup->wlSurface()->resource()->m_current.texture)
            return NOT_READY("Popup sibling is not ready");
    }
    std::ranges::sort(popups, {}, [](const auto& popup) { return popup->wlSurface()->resource()->m_current.size.x; });
    if (popups[0]->wlSurface()->resource()->m_current.size.x != 64 || popups[1]->wlSurface()->resource()->m_current.size.x != 96)
        return {.success = false, .error = "Unexpected popup surface identities"};

    if (window->presentation().alpha().isBeingAnimated() || window->presentation().alphaTotal() != 1.F || window->m_workspace->m_alpha->isBeingAnimated() ||
        window->m_workspace->m_alpha->value() != 1.F)
        return NOT_READY("Parent alpha must be settled at one before probing");

    const std::array<PHLANIMVARREF<float>, 9> ALPHAS = {
        window->presentation().alpha(WINDOW_ALPHA_FADE),
        window->presentation().alpha(WINDOW_ALPHA_ACTIVE),
        window->m_workspace->m_alpha,
        popups[0]->alpha()[POPUP_ALPHA_FADE],
        popups[1]->alpha()[POPUP_ALPHA_FADE],
        window->presentation().alpha(WINDOW_ALPHA_FULLSCREEN),
        window->presentation().alpha(WINDOW_ALPHA_LAYOUT),
        window->presentation().alpha(WINDOW_ALPHA_MOVE_TO_WORKSPACE),
        window->presentation().alpha(WINDOW_ALPHA_MOVE_FROM_WORKSPACE),
    };
    for (const auto& alpha : ALPHAS) {
        if (alpha->isBeingAnimated() || alpha->value() != alpha->goal())
            return NOT_READY("Fixture alpha is still animating");
    }

    std::array<float, 9> oldAlpha = {};
    for (size_t i = 0; i < ALPHAS.size(); ++i)
        oldAlpha[i] = ALPHAS[i]->value();

    // An explicit mode also probes geometry; omitted modes retain the original opacity cases.
    const bool CHECK_PRESENTATION = !presentationMode.empty();
    const bool ISOLATED           = presentationMode.starts_with("isolated-");
    const bool WORKSPACE_OFFSET   = presentationMode.ends_with("workspace-offset");
    auto       presentation       = dynamicPointerCast<Workspace::CWorkspacePresentable>(window->m_workspace);
    if (presentationMode == "none")
        presentation.reset();
    else if (presentationMode == "custom") {
        presentation      = makeShared<Workspace::CWorkspacePresentable>();
        const auto CONFIG = Config::animationTree()->getAnimationPropertyConfig("workspacesIn");
        Animation::mgr()->createAnimation(Vector2D{37, 23}, presentation->m_renderOffset, CONFIG, AVARDAMAGE_NONE);
        Animation::mgr()->createAnimation(0.6F, presentation->m_alpha, CONFIG, AVARDAMAGE_NONE);
    }
    const auto          OLD_FLOATING_OFFSET = window->presentation().m_floatingOffset;
    const auto          OLD_MOVED_FROM      = window->presentation().m_monitorMovedFrom;
    const bool          OLD_FLOATING        = window->isFloating();
    auto&               workspaceOffset     = *window->m_workspace->m_renderOffset;
    const auto          OLD_OFFSET          = workspaceOffset.value();
    const bool          OLD_OFFSET_ANIMATED = workspaceOffset.isBeingAnimated();

    auto&               renderer    = *g_pHyprRenderer;
    auto&               rootEntries = ctx.m_pass.m_passElements;
    const auto          BEGIN       = rootEntries.size();
    const auto          OLD_DATA    = ctx.m_data;
    Render::CRenderPass previousPass, redirectedPass;
    // Root opacity remains independent of routing. Routing also tests nested guard restoration.
    const auto  OUTER_PASS = renderer.redirectPass(ctx, redirected ? &previousPass : nullptr);
    CScopeGuard restore([&] {
        // Drop every test item, including non-surface items, before the real frame draws.
        rootEntries.resize(BEGIN);
        ctx.m_data = OLD_DATA;
        for (size_t i = 0; i < ALPHAS.size(); ++i)
            ALPHAS[i]->value() = oldAlpha[i];
        if (CHECK_PRESENTATION)
            window->presentation().m_floatingOffset = OLD_FLOATING_OFFSET;
        if (ISOLATED) {
            workspaceOffset.value()                   = OLD_OFFSET;
            workspaceOffset.m_bIsBeingAnimated        = OLD_OFFSET_ANIMATED;
            window->presentation().m_monitorMovedFrom = OLD_MOVED_FROM;
            window->m_target->setFloatingInitial(OLD_FLOATING);
        }
    });

    // A zero window alpha returns early. Use the workspace contribution for zero
    // so we still require actual popup pass elements with zero inherited fade.
    // Only current values change: preserve goals/animation state and avoid callbacks.
    const float WINDOW_FADE = parentFade == 0.F ? 1.F : parentFade;
    ALPHAS[0]->value()      = WINDOW_FADE;
    ALPHAS[1]->value()      = 0.75F;
    ALPHAS[2]->value()      = parentFade == 0.F ? 0.F : 1.F;
    ALPHAS[3]->value()      = 0.4F;
    ALPHAS[4]->value()      = 0.8F;
    if (CHECK_PRESENTATION) {
        // Distinct live/custom alpha catches accidentally reading window->m_workspace.
        ALPHAS[2]->value() = ISOLATED && parentFade == 0.F ? 0.F : 0.25F;
        window->presentation().setFloatingOffset({7, 11});
    }
    if (ISOLATED) {
        ALPHAS[5]->value() = 0.6F;
        ALPHAS[6]->value() = 0.8F;
        ALPHAS[7]->value() = 0.F;
        ALPHAS[8]->value() = 0.F;
        // Synthetic slide state: no manager registration, callbacks or goal changes.
        workspaceOffset.value()            = {37, 23};
        workspaceOffset.m_bIsBeingAnimated = true;
        window->presentation().setMonitorMovedFrom(0);
        // Exercise the floating slide-clip path without changing layout membership or rules.
        window->m_target->setFloatingInitial(true);
        if (WORKSPACE_OFFSET)
            window->presentation().m_floatingOffset.set({7, 11}, eFloatingOffsetSource::WORKSPACE);
    }
    const float PRESENTATION_ALPHA    = presentationMode == "none" ? 1.F : presentationMode == "custom" ? 0.6F : 0.25F;
    const float EXPECTED_PARENT_FADE  = ISOLATED ? WINDOW_FADE * 0.6F * 0.8F : CHECK_PRESENTATION ? WINDOW_FADE * PRESENTATION_ALPHA : parentFade;
    const auto  EXPECTED_OFFSET       = ISOLATED ? (WORKSPACE_OFFSET ? Vector2D{} : Vector2D{7, 11}) :
        presentation                             ? (presentationMode == "custom" ? Vector2D{37, 23} : OLD_OFFSET) + Vector2D{7, 11} :
                                                   Vector2D{};
    const auto  EXPECTED_POSITION     = window->position(IGeometric::GEOMETRIC_CURRENT) + EXPECTED_OFFSET;
    const auto  SCENE_MODE            = presentationMode.starts_with("isolated-shell-") ? Render::eSceneMode::WORKSPACE_WITH_SHELL : Render::eSceneMode::WORKSPACE_WINDOWS;
    const auto  RESOLVED_PRESENTATION = ISOLATED ? window->presentation().renderPresentation(SCENE_MODE) : window->presentation().renderPresentation(presentation);
    if (ISOLATED &&
        (RESOLVED_PRESENTATION.workspaceOffset != Vector2D{} || RESOLVED_PRESENTATION.workspaceAlpha != 1.F || RESOLVED_PRESENTATION.workspaceOffsetAnimating ||
         !RESOLVED_PRESENTATION.alphaVisible || RESOLVED_PRESENTATION.floatingOffset != EXPECTED_OFFSET))
        return {.success = false, .error = "Isolated presentation retained workspace transition state or lost the window offset/visibility"};
    const auto PREVIOUS_PASS = ctx.m_currentPass;
    {
        const auto REDIRECT = renderer.redirectPass(ctx, redirected ? &redirectedPass : nullptr);
        renderer.renderWindow(ctx, window, window->m_monitor.lock(), RESOLVED_PRESENTATION, Time::steadyNow(), false, mode, !CHECK_PRESENTATION);
    }
    if (ctx.m_currentPass != PREVIOUS_PASS || &renderer.currentPass(ctx) != (redirected ? &previousPass : &ctx.m_pass))
        return {.success = false, .error = "Popup render did not restore the previous pass"};

    const auto&                                 entries = (redirected ? redirectedPass : ctx.m_pass).m_passElements;

    const std::array<SP<CWLSurfaceResource>, 3> SURFACES = {
        window->wlSurface()->resource(),
        popups[0]->wlSurface()->resource(),
        popups[1]->wlSurface()->resource(),
    };
    const std::array<float, 3> EXPECTED_FADE = {
        EXPECTED_PARENT_FADE,
        EXPECTED_PARENT_FADE * 0.4F,
        EXPECTED_PARENT_FADE * 0.8F,
    };
    std::array<size_t, 3> counts = {};
    std::string           probeError;
    if (redirected && (rootEntries.size() != BEGIN || !previousPass.m_passElements.empty()))
        probeError = std::format("Pass leakage: root additions {}, previous-pass additions {}; ", rootEntries.size() - BEGIN, previousPass.m_passElements.size());
    for (size_t i = redirected ? 0 : BEGIN; i < entries.size(); ++i) {
        if (entries[i].element->type() != EK_SURFACE)
            continue;

        const auto& data = sc<CSurfacePassElement*>(entries[i].element.get())->m_data;
        const auto  IT   = std::ranges::find(SURFACES, data.surface);
        if (IT == SURFACES.end())
            return {.success = false, .error = "Unexpected surface in popup opacity probe"};

        const auto INDEX = std::distance(SURFACES.begin(), IT);
        ++counts[INDEX];
        if (data.pWindow != window || data.popup != (INDEX != 0) || data.mainSurface != (INDEX == 0))
            return {.success = false, .error = std::format("Incorrect identity/flags for surface {}", INDEX)};
        if (data.workspacePresentation != RESOLVED_PRESENTATION)
            return {.success = false, .error = std::format("Incorrect workspace presentation for surface {}", INDEX)};
        if (CHECK_PRESENTATION) {
            const auto POSITION = EXPECTED_POSITION + (INDEX == 0 ? Vector2D{} : popups[INDEX - 1]->coordsRelativeToParent() - window->backend().geometry().box.pos());
            if (!((data.pos - POSITION).size() <= 0.00001) || data.localPos != Vector2D{})
                probeError += std::format("surface {}: incorrect position for presentation {}; ", INDEX, presentationMode);
            if ((!presentation || ISOLATED) && (data.clipBox.w != 0 || data.clipBox.h != 0))
                probeError += std::format("surface {}: presentation {} retained transition clipping; ", INDEX, presentationMode);
        }
        if (!(std::abs(data.fadeAlpha - EXPECTED_FADE[INDEX]) <= 0.00001F) || !(std::abs(data.alpha - 0.75F) <= 0.00001F))
            probeError += std::format("surface {}: expected fade {} alpha 0.75, got fade {} alpha {}; ", INDEX, EXPECTED_FADE[INDEX], data.fadeAlpha, data.alpha);
    }

    const std::array<size_t, 3> EXPECTED_COUNTS = {
        mode == Render::RENDER_PASS_ALL ? 1UZ : 0UZ,
        1,
        1,
    };
    if (counts != EXPECTED_COUNTS)
        probeError += std::format("Expected {} surface counts {}/{}/{}, got {}/{}/{}", redirected ? "redirected" : "root", EXPECTED_COUNTS[0], EXPECTED_COUNTS[1],
                                  EXPECTED_COUNTS[2], counts[0], counts[1], counts[2]);

    return {.success = probeError.empty(), .error = probeError};
}

static SDispatchResult armPopupOpacity(const std::string& cls, float parentFade, bool popupOnly, bool redirected, const std::string& presentationMode) {
    g_popupRenderRecording.reset();
    const auto WINDOW = windowByClass(cls);
    if (!WINDOW || !WINDOW->mapped() || !WINDOW->m_monitor || !WINDOW->m_workspace || WINDOW->popupHead()->allMappedChildrenCount() != 2)
        return {.success = false, .error = "Waiting for popup-render parent and two mapped popups"};
    if (!std::isfinite(parentFade) || parentFade < 0.F || parentFade > 1.F)
        return {.success = false, .error = "Invalid parent fade"};
    if (!presentationMode.empty() && presentationMode != "normal" && presentationMode != "none" && presentationMode != "custom" &&
        presentationMode != "isolated-windows-window-offset" && presentationMode != "isolated-windows-workspace-offset" && presentationMode != "isolated-shell-window-offset" &&
        presentationMode != "isolated-shell-workspace-offset")
        return {.success = false, .error = "Invalid workspace presentation mode"};

    g_popupRenderRecording                         = makeShared<SPopupRenderRecording>();
    const WP<SPopupRenderRecording> WEAK_RECORDING = g_popupRenderRecording;
    const PHLWINDOWREF              WEAK_WINDOW    = WINDOW;
    g_popupRenderRecording->stage =
        Event::bus()->m_events.render.stage.listen([WEAK_RECORDING, WEAK_WINDOW, parentFade, popupOnly, redirected, presentationMode](Event::SRenderStageEvent event) {
            const auto RECORDING = WEAK_RECORDING.lock();
            const auto WINDOW    = WEAK_WINDOW.lock();
            if (!RECORDING || RECORDING->complete || !WINDOW || event.stage != RENDER_BEGIN || event.monitor != WINDOW->m_monitor || !event.context)
                return;

            // RENDER_BEGIN has a bound framebuffer; renderWindow only queues the probe.
            bool ready = false;
            RECORDING->result =
                probePopupOpacity(event.context->get(), WINDOW, parentFade, popupOnly ? Render::RENDER_PASS_POPUP : Render::RENDER_PASS_ALL, redirected, presentationMode, ready);
            RECORDING->complete = ready;
            if (const auto MONITOR = WINDOW->m_monitor.lock(); !ready && MONITOR) {
                g_pHyprRenderer->damageMonitor(MONITOR);
                MONITOR->scheduleFrame(Aquamarine::IOutput::AQ_SCHEDULE_DAMAGE);
            }
        });
    g_pHyprRenderer->damageMonitor(WINDOW->m_monitor.lock());
    return {};
}

static SDispatchResult testDragLifecycle(const std::string& cls) {
    const auto WINDOW = windowByClass(cls);
    if (!WINDOW)
        return {.success = false, .error = std::format("No window with class '{}'", cls)};

    const auto TARGET = WINDOW->layoutTarget();
    if (!TARGET)
        return {.success = false, .error = "Window has no layout target"};

    const auto& CONTROLLER = g_layoutManager->dragController();
    if (CONTROLLER->target())
        return {.success = false, .error = "A drag is already active"};

    std::vector<std::string> events;
    bool                     motionHadTarget = false;
    bool                     endedWasReset   = false;
    const auto               MOTION_LISTENER = CONTROLLER->m_events.motion.listen([&] {
        events.emplace_back("motion");
        motionHadTarget = !!CONTROLLER->target();
    });
    const auto               ENDED_LISTENER  = CONTROLLER->m_events.ended.listen([&] {
        events.emplace_back("ended");
        endedWasReset = !CONTROLLER->target() && CONTROLLER->mode() == MBIND_INVALID;
    });

    const auto               START = TARGET->position().middle();
    Pointer::pointerController()->warpTo(START, true);
    g_layoutManager->beginDragTarget(TARGET, MBIND_MOVE);
    g_layoutManager->moveMouse(START + Vector2D{100, 100});
    const bool ENDED       = g_layoutManager->endDragTarget();
    const bool ENDED_AGAIN = g_layoutManager->endDragTarget();

    if (!ENDED || ENDED_AGAIN)
        return {.success = false, .error = std::format("Unexpected drag end results: first {}, second {}", ENDED, ENDED_AGAIN)};
    if (events != std::vector<std::string>{"motion", "ended"})
        return {.success = false, .error = std::format("Expected one motion and one ended event, got {} total events", events.size())};
    if (!motionHadTarget)
        return {.success = false, .error = "Drag motion event fired without an active target"};
    if (!endedWasReset)
        return {.success = false, .error = "Drag ended event fired before state was reset"};

    return {};
}

// Perform a full move-drag of the window with the given class and drop it at (x, y).
static SDispatchResult dragWindow(std::string in) {
    CVarList2 data(std::move(in));

    if (data.size() < 3)
        return {.success = false, .error = "invalid input"};

    const std::string cls = std::string{data[0]};

    double            x;
    double            y;
    try {
        x = std::stod(std::string{data[1]});
        y = std::stod(std::string{data[2]});
    } catch (...) { return {.success = false, .error = "invalid input"}; }

    for (const auto& window : Desktop::windowState()->windows()) {
        if (window->metadata().appID() != cls)
            continue;

        const auto target = window->layoutTarget();
        if (!target)
            return {.success = false, .error = "Window has no layout target"};

        Pointer::pointerController()->warpTo({x, y}, true);
        g_layoutManager->beginDragTarget(target, MBIND_MOVE);
        g_layoutManager->endDragTarget();

        return {};
    }

    return {.success = false, .error = std::format("No window with class '{}'", cls)};
}

class CTestKeyboard : public IKeyboard {
  public:
    static SP<CTestKeyboard> create(bool isVirtual) {
        auto keeb           = SP<CTestKeyboard>(new CTestKeyboard());
        keeb->m_self        = keeb;
        keeb->m_isVirtual   = isVirtual;
        keeb->m_shareStates = !isVirtual;
        keeb->m_hlName      = "test-keyboard";
        keeb->m_deviceName  = "test-keyboard";
        return keeb;
    }

    virtual bool isVirtual() const {
        return m_isVirtual;
    }

    virtual SP<Aquamarine::IKeyboard> aq() {
        return nullptr;
    }

    void sendKey(uint32_t key, bool pressed) {
        auto event = IKeyboard::SKeyEvent{
            .timeMs  = sc<uint32_t>(Time::millis(Time::steadyNow())),
            .keycode = key,
            .state   = pressed ? WL_KEYBOARD_KEY_STATE_PRESSED : WL_KEYBOARD_KEY_STATE_RELEASED,
        };
        updatePressed(event.keycode, pressed);
        m_keyboardEvents.key.emit(event);
    }

    void setMods(uint32_t depressed, uint32_t latched, uint32_t locked, uint32_t group) {
        updateModifiers(depressed, latched, locked, group);
    }

    void destroy() {
        m_events.destroy.emit();
    }

  private:
    bool m_isVirtual = false;
};

class CTestMouse : public IPointer {
  public:
    static SP<CTestMouse> create(bool isVirtual) {
        auto maus          = SP<CTestMouse>(new CTestMouse());
        maus->m_self       = maus;
        maus->m_isVirtual  = isVirtual;
        maus->m_deviceName = "test-mouse";
        maus->m_hlName     = "test-mouse";
        return maus;
    }

    virtual bool isVirtual() const {
        return m_isVirtual;
    }

    virtual SP<Aquamarine::IPointer> aq() {
        return nullptr;
    }

    void destroy() {
        m_events.destroy.emit();
    }

  private:
    bool m_isVirtual = false;
};

class CKeyboardEventRecorder : public IKeyboardEventHandler {
  public:
    struct SEvent {
        uint32_t              keycode = 0;
        wl_keyboard_key_state state   = WL_KEYBOARD_KEY_STATE_RELEASED;
    };

    virtual void onKeyboardKey(const IKeyboard::SKeyEvent& event, SP<IKeyboard>) override {
        m_events.emplace_back(SEvent{
            .keycode = event.keycode,
            .state   = event.state,
        });
    }

    std::vector<SEvent> m_events;
};

struct SPinchScaleEvents {
    size_t             beginCount = 0;
    size_t             endCount   = 0;
    float              beginScale = 0.F;
    float              endScale   = 0.F;
    std::vector<float> updateScales;
};

class CPinchScaleRecorder : public ITrackpadGesture {
  public:
    CPinchScaleRecorder(SP<SPinchScaleEvents> events) : m_events(std::move(events)) {
        ;
    }

    virtual void begin(const STrackpadGestureBegin& event) override {
        ++m_events->beginCount;
        m_events->beginScale = event.scale;
        ITrackpadGesture::begin(event);
    }

    virtual void update(const STrackpadGestureUpdate& event) override {
        m_events->updateScales.emplace_back(event.scale);
    }

    virtual void end(const STrackpadGestureEnd& event) override {
        ++m_events->endCount;
        m_events->endScale = event.scale;
    }

  private:
    SP<SPinchScaleEvents> m_events;
};

SP<CTestMouse>             g_mouse;
SP<CTestKeyboard>          g_keyboard;
SP<CTestKeyboard>          g_keyboard2;
SP<CKeyboardEventRecorder> g_keyboardEventRecorder;

static SDispatchResult     registerKeyboardEventRecorder(std::string in) {
    if (!g_keyboardEventRecorder)
        g_keyboardEventRecorder = makeShared<CKeyboardEventRecorder>();
    else
        g_pSeatManager->m_keyboardEventHandlers.remove(g_keyboardEventRecorder);

    g_keyboardEventRecorder->m_events.clear();
    g_pSeatManager->m_keyboardEventHandlers.push(g_keyboardEventRecorder);
    return {};
}

static SDispatchResult testPinchDeltaScale(float scale) {
    constexpr size_t                    FINGERS         = 42;
    constexpr eTrackpadGestureDirection DIRECTION       = TRACKPAD_GESTURE_DIR_PINCH;
    constexpr bool                      DISABLE_INHIBIT = true;

    if (g_pTrackpadGestures->m_activeGesture)
        return {.success = false, .error = "A trackpad gesture is already active"};

    const auto  OLD_MODS = g_pInputManager->m_lastMods;
    CScopeGuard RESTORE_MODS([OLD_MODS] { g_pInputManager->m_lastMods = OLD_MODS; });
    g_pInputManager->m_lastMods = Input::HL_MODIFIER_NONE;

    const auto EVENTS = makeShared<SPinchScaleEvents>();
    const auto ADDED  = g_pTrackpadGestures->addGesture(makeUnique<CPinchScaleRecorder>(EVENTS), FINGERS, DIRECTION, Input::HL_MODIFIER_NONE, scale, DISABLE_INHIBIT);
    if (!ADDED)
        return {.success = false, .error = ADDED.error()};

    g_pTrackpadGestures->gestureBegin(IPointer::SPinchBeginEvent{.fingers = FINGERS});
    g_pTrackpadGestures->gestureUpdate(IPointer::SPinchUpdateEvent{.fingers = FINGERS, .scale = 1.2});
    g_pTrackpadGestures->gestureUpdate(IPointer::SPinchUpdateEvent{.fingers = FINGERS, .scale = 1.4});
    g_pTrackpadGestures->gestureEnd(IPointer::SPinchEndEvent{.cancelled = true});

    const auto REMOVED = g_pTrackpadGestures->removeGesture(FINGERS, DIRECTION, Input::HL_MODIFIER_NONE, scale, DISABLE_INHIBIT);
    if (!REMOVED)
        return {.success = false, .error = REMOVED.error()};

    const auto SCALE_MATCHES = [scale](float actual) { return std::abs(actual - scale) < 0.001F; };
    if (EVENTS->beginCount != 1 || EVENTS->updateScales.size() != 2 || EVENTS->endCount != 1)
        return {.success = false,
                .error   = std::format("Unexpected pinch callback counts: begin {}, update {}, end {}", EVENTS->beginCount, EVENTS->updateScales.size(), EVENTS->endCount)};
    if (!SCALE_MATCHES(EVENTS->beginScale) || !std::ranges::all_of(EVENTS->updateScales, SCALE_MATCHES) || !SCALE_MATCHES(EVENTS->endScale))
        return {.success = false, .error = std::format("Configured pinch scale {} was not propagated to every callback", scale)};

    return {};
}

static SDispatchResult removeKeyboardEventRecorder(std::string in) {
    if (g_keyboardEventRecorder)
        g_pSeatManager->m_keyboardEventHandlers.remove(g_keyboardEventRecorder);

    return {};
}

static SDispatchResult expectKeyboardEvents(const std::vector<CKeyboardEventRecorder::SEvent>& expected) {
    if (!g_keyboardEventRecorder)
        return {.success = false, .error = "Keyboard event recorder has not been registered"};

    if (g_keyboardEventRecorder->m_events.size() != expected.size())
        return {.success = false, .error = std::format("Expected {} keyboard events, recorded {}", expected.size(), g_keyboardEventRecorder->m_events.size())};

    for (size_t i = 0; i < expected.size(); ++i) {
        const auto& ACTUAL = g_keyboardEventRecorder->m_events[i];
        if (ACTUAL.keycode == expected[i].keycode && ACTUAL.state == expected[i].state)
            continue;

        return {.success = false,
                .error   = std::format("Keyboard event {}: expected keycode {} state {}, recorded keycode {} state {}", i, expected[i].keycode, sc<uint32_t>(expected[i].state),
                                       ACTUAL.keycode, sc<uint32_t>(ACTUAL.state))};
    }

    return {};
}

static SDispatchResult pressAlt(std::string in) {
    g_pInputManager->m_lastMods = in == "1" ? Input::HL_MODIFIER_ALT : Input::HL_MODIFIER_NONE;

    return {.success = true};
}

static SDispatchResult simulateGesture(std::string in) {
    CVarList data(in);

    uint32_t fingers = 3;
    try {
        fingers = std::stoul(data[1]);
    } catch (...) { return {.success = false}; }

    if (data[0] == "down") {
        g_pTrackpadGestures->gestureBegin(IPointer::SSwipeBeginEvent{});
        g_pTrackpadGestures->gestureUpdate(IPointer::SSwipeUpdateEvent{.fingers = fingers, .delta = {0, 300}});
        g_pTrackpadGestures->gestureEnd(IPointer::SSwipeEndEvent{});
    } else if (data[0] == "up") {
        g_pTrackpadGestures->gestureBegin(IPointer::SSwipeBeginEvent{});
        g_pTrackpadGestures->gestureUpdate(IPointer::SSwipeUpdateEvent{.fingers = fingers, .delta = {0, -300}});
        g_pTrackpadGestures->gestureEnd(IPointer::SSwipeEndEvent{});
    } else if (data[0] == "left") {
        g_pTrackpadGestures->gestureBegin(IPointer::SSwipeBeginEvent{});
        g_pTrackpadGestures->gestureUpdate(IPointer::SSwipeUpdateEvent{.fingers = fingers, .delta = {-300, 0}});
        g_pTrackpadGestures->gestureEnd(IPointer::SSwipeEndEvent{});
    } else {
        g_pTrackpadGestures->gestureBegin(IPointer::SSwipeBeginEvent{});
        g_pTrackpadGestures->gestureUpdate(IPointer::SSwipeUpdateEvent{.fingers = fingers, .delta = {300, 0}});
        g_pTrackpadGestures->gestureEnd(IPointer::SSwipeEndEvent{});
    }

    return {.success = true};
}

static SDispatchResult pinchUpdate(std::string in) {
    CVarList data(in);
    uint32_t fingers = 2;
    double   scale   = 1.0;
    Vector2D delta   = {};
    double   rotation{};

    if (data.size() < 2)
        return {.success = false, .error = "invalid input"};

    if (const auto n = strToNumber<uint32_t>(data[0]); n)
        fingers = n.value();
    else
        return {.success = false, .error = "invalid input"};

    if (const auto n = strToNumber<double>(data[1]); n)
        scale = n.value();
    else
        return {.success = false, .error = "invalid input"};

    if (data.size() > 2) {
        if (const auto n = strToNumber<double>(data[2]); n)
            delta.x = n.value();
        else
            return {.success = false, .error = "invalid input"};
    }

    if (data.size() > 3) {
        if (const auto n = strToNumber<double>(data[3]); n)
            delta.y = n.value();
        else
            return {.success = false, .error = "invalid input"};
    }

    if (data.size() > 4) {
        if (const auto n = strToNumber<double>(data[4]); n)
            rotation = n.value();
        else
            return {.success = false, .error = "invalid input"};
    }

    g_pTrackpadGestures->gestureUpdate(IPointer::SPinchUpdateEvent{
        .fingers  = fingers,
        .delta    = delta,
        .scale    = scale,
        .rotation = rotation,
    });

    return {};
}

static SDispatchResult pinchEnd(std::string in) {
    g_pTrackpadGestures->gestureEnd(IPointer::SPinchEndEvent{});

    return {};
}

static SDispatchResult expectCursorZoom(std::string in) {
    CVarList data(in);
    float    expected = 1.F;
    float    delta    = 0.01F;

    if (data.size() < 1)
        return {.success = false, .error = "invalid input"};

    if (const auto n = strToNumber<float>(data[0]); n)
        expected = n.value();
    else
        return {.success = false, .error = "invalid input"};

    if (data.size() > 1) {
        if (const auto n = strToNumber<float>(data[1]); n)
            delta = n.value();
        else
            return {.success = false, .error = "invalid input"};
    }

    const auto PMONITOR = State::monitorState()->query().vec(Pointer::mgr()->untransformedPosition()).run();

    if (!PMONITOR)
        return {.success = false, .error = "No monitor under cursor"};

    const auto actual = PMONITOR->m_cursorZoom->value();

    if (std::abs(actual - expected) > delta)
        return {.success = false, .error = std::format("Expected cursor zoom {} ± {}, got {}", expected, delta, actual)};

    return {};
}

static SDispatchResult vkb(std::string in) {
    auto tkb0 = CTestKeyboard::create(false);
    auto tkb1 = CTestKeyboard::create(false);
    auto vkb0 = CTestKeyboard::create(true);

    g_pInputManager->newKeyboard(tkb0);
    g_pInputManager->newKeyboard(tkb1);
    g_pInputManager->newKeyboard(vkb0);

    CScopeGuard    x([&] {
        tkb0->destroy();
        tkb1->destroy();
        vkb0->destroy();
    });

    const auto&    PRESSED = g_pInputManager->getKeysFromAllKBs();
    const uint32_t TESTKEY = 1;

    tkb0->sendKey(TESTKEY, true);
    if (!std::ranges::contains(PRESSED, TESTKEY)) {
        return {
            .success = false,
            .error   = "Expected pressed key not found",
        };
    }

    tkb1->sendKey(TESTKEY, true);
    tkb0->sendKey(TESTKEY, false);
    if (!std::ranges::contains(PRESSED, TESTKEY)) {
        return {
            .success = false,
            .error   = "Expected pressed key not found (kb share state)",
        };
    }

    vkb0->sendKey(TESTKEY, true);
    tkb1->sendKey(TESTKEY, false);
    if (std::ranges::contains(PRESSED, TESTKEY)) {
        return {
            .success = false,
            .error   = "Expected released key found in pressed (vkb no share state)",
        };
    }

    return {};
}

static SDispatchResult scroll(std::string in) {
    double by;
    try {
        by = std::stod(in);
    } catch (...) { return SDispatchResult{.success = false, .error = "invalid input"}; }

    LOG(Log::DEBUG, "tester: scrolling by {}", by);

    g_mouse->m_pointerEvents.axis.emit(IPointer::SAxisEvent{
        .delta         = by,
        .deltaDiscrete = 120,
        .mouse         = true,
    });

    return {};
}

static SDispatchResult click(std::string in) {
    CVarList2 data(std::move(in));

    uint32_t  button;
    bool      pressed;
    try {
        button  = std::stoul(std::string{data[0]});
        pressed = std::stoul(std::string{data[1]}) == 1;
    } catch (...) { return {.success = false, .error = "invalid input"}; }

    LOG(Log::DEBUG, "tester: mouse button {} state {}", button, pressed);

    g_mouse->m_pointerEvents.button.emit(IPointer::SButtonEvent{
        .timeMs = sc<uint32_t>(Time::millis(Time::steadyNow())),
        .button = button,
        .state  = pressed ? WL_POINTER_BUTTON_STATE_PRESSED : WL_POINTER_BUTTON_STATE_RELEASED,
        .mouse  = true,
    });

    return {};
}

static SDispatchResult keybind(std::string in) {
    CVarList2 data(std::move(in));
    // 0 = release, 1 = press
    bool press;
    // See src/devices/IKeyboard.hpp : eKeyboardModifiers for modifier bitmasks
    // 0 = none, eKeyboardModifiers is shifted to start at 1
    uint32_t modifier;
    // keycode
    uint32_t key;
    try {
        press    = std::stoul(std::string{data[0]}) == 1;
        modifier = std::stoul(std::string{data[1]});
        key      = std::stoul(std::string{data[2]}) - 8; // xkb offset
    } catch (...) { return {.success = false, .error = "invalid input"}; }

    Input::ModifierMask modifierMask = Input::HL_MODIFIER_NONE;
    if (modifier > 0)
        modifierMask = sc<Input::ModifierMask>(1 << (modifier - 1));
    g_pInputManager->m_lastMods = modifierMask;
    g_keyboard->sendKey(key, press);

    return {};
}

static SDispatchResult keybind2(std::string in) {
    CVarList2 data(std::move(in));
    bool      press;
    uint32_t  modifier;
    uint32_t  key;
    try {
        press    = std::stoul(std::string{data[0]}) == 1;
        modifier = std::stoul(std::string{data[1]});
        key      = std::stoul(std::string{data[2]}) - 8;
    } catch (...) { return {.success = false, .error = "invalid input"}; }

    Input::ModifierMask modifierMask = Input::HL_MODIFIER_NONE;
    if (modifier > 0)
        modifierMask = sc<Input::ModifierMask>(1 << (modifier - 1));
    g_pInputManager->m_lastMods = modifierMask;
    g_keyboard2->sendKey(key, press);

    return {};
}

static SDispatchResult keybindModmask(std::string in) {
    CVarList2 data(std::move(in));
    // 0 = release, 1 = press
    bool press;
    // See src/devices/IKeyboard.hpp : eKeyboardModifiers for modifier bitmasks
    // 0 = none, eKeyboardModifiers is shifted to start at 1
    uint32_t modifierMask;
    // keycode
    uint32_t key;
    try {
        press        = std::stoul(std::string{data[0]}) == 1;
        modifierMask = std::stoul(std::string{data[1]});
        key          = std::stoul(std::string{data[2]}) - 8; // xkb offset
    } catch (...) { return {.success = false, .error = "invalid input"}; }

    g_pInputManager->m_lastMods = g_pInputManager->xkbModsToHyprland(g_keyboard, modifierMask);
    g_keyboard->setMods(modifierMask, 0, 0, 0);
    g_keyboard->sendKey(key, press);

    return {};
}

static SDispatchResult setMods(std::string in) {
    CVarList2 data(std::move(in));
    try {
        uint32_t          kbIndex   = std::stoul(std::string{data[0]});
        uint32_t          depressed = std::stoul(std::string{data[1]});
        uint32_t          latched   = std::stoul(std::string{data[2]});
        uint32_t          locked    = std::stoul(std::string{data[3]});
        uint32_t          group     = std::stoul(std::string{data[4]});

        SP<CTestKeyboard> kb = (kbIndex == 0) ? g_keyboard : g_keyboard2;
        kb->setMods(depressed, latched, locked, group);
    } catch (...) { return {.success = false, .error = "invalid input"}; }

    return {};
}

static SDispatchResult nullfocus(std::string in) {
    g_pSeatManager->setKeyboardFocus(nullptr);
    return {};
}

static SDispatchResult clearSurfaceFocus(std::string in) {
    Desktop::focusState()->m_focusSurface.reset();
    return {};
}

static SDispatchResult checkKeyboardFocusWindow(std::string in) {
    const auto KBSURF = g_pSeatManager->m_state.keyboardFocus.lock();
    if (!KBSURF)
        return {.success = false, .error = "No keyboard focus"};

    const auto PWINDOW = Desktop::focusState()->window();
    if (!PWINDOW)
        return {.success = false, .error = "Keyboard focus surface is not a window"};

    if (PWINDOW->metadata().appID() != in)
        return {.success = false, .error = std::format("Keyboard focus window class is '{}', expected '{}'", PWINDOW->metadata().appID(), in)};

    return {};
}

static Desktop::Rule::CWindowRuleEffectContainer::storageType windowRuleIDX = 0;

//
static SDispatchResult addWindowRule(std::string in) {
    windowRuleIDX = Desktop::Rule::windowEffects()->registerEffect("plugin_rule");

    if (Desktop::Rule::windowEffects()->registerEffect("plugin_rule") != windowRuleIDX)
        return {.success = false, .error = "re-registering returned a different id?"};
    return {};
}

static SDispatchResult checkWindowRule(std::string in) {
    const auto PLASTWINDOW = Desktop::focusState()->window();

    if (!PLASTWINDOW)
        return {.success = false, .error = "No window"};

    if (!PLASTWINDOW->m_ruleApplicator->m_otherProps.props.contains(windowRuleIDX))
        return {.success = false, .error = "No rule"};

    if (PLASTWINDOW->m_ruleApplicator->m_otherProps.props[windowRuleIDX]->effect != "effect")
        return {.success = false, .error = "Effect isn't \"effect\""};

    return {};
}

static Desktop::Rule::CLayerRuleEffectContainer::storageType layerRuleIDX = 0;

static SDispatchResult                                       addLayerRule(std::string in) {
    layerRuleIDX = Desktop::Rule::layerEffects()->registerEffect("plugin_rule");

    if (Desktop::Rule::layerEffects()->registerEffect("plugin_rule") != layerRuleIDX)
        return {.success = false, .error = "re-registering returned a different id?"};
    return {};
}

static SDispatchResult checkLayerRule(std::string in) {
    if (Desktop::layerState()->layers().size() != 3)
        return {.success = false, .error = "Layers under test not here"};

    for (const auto& layer : Desktop::layerState()->layers()) {
        if (layer->m_namespace == "rule-layer") {

            if (!layer->m_ruleApplicator->m_otherProps.props.contains(layerRuleIDX))
                return {.success = false, .error = "No rule"};

            if (layer->m_ruleApplicator->m_otherProps.props[layerRuleIDX]->effect != "effect")
                return {.success = false, .error = "Effect isn't \"effect\""};

        } else if (layer->m_namespace == "norule-layer") {

            if (layer->m_ruleApplicator->m_otherProps.props.contains(layerRuleIDX))
                return {.success = false, .error = "Rule even though it shouldn't"};

        } else
            return {.success = false, .error = "Unrecognized layer"};
    }

    return {};
}

static SDispatchResult checkPointerFocusWindow(std::string in) {
    const auto POINTERSURF = g_pSeatManager->m_state.pointerFocus.lock();
    if (!POINTERSURF)
        return {.success = false, .error = "No pointer focus"};

    const auto WINDOW = Desktop::viewState()->query().type(Desktop::View::VIEW_TYPE_WINDOW).surface(POINTERSURF).runWindow();
    if (!WINDOW)
        return {.success = false, .error = "Pointer focus surface is not a window"};

    if (WINDOW->metadata().appID() != in)
        return {.success = false, .error = std::format("Pointer focus window class is '{}', expected '{}'", WINDOW->metadata().appID(), in)};

    return {};
}

static SDispatchResult checkPointerFocusLayer(std::string in) {
    const auto POINTERSURF = g_pSeatManager->m_state.pointerFocus.lock();

    if (!POINTERSURF)
        return {.success = false, .error = "No pointer focus"};

    const auto HLSURF = Desktop::View::CWLSurface::fromResource(POINTERSURF);
    const auto VIEW   = HLSURF ? HLSURF->view() : nullptr;
    const auto LAYER  = Desktop::View::CLayerSurface::fromView(VIEW);

    if (!LAYER) {
        const auto WINDOW = Desktop::viewState()->query().type(Desktop::View::VIEW_TYPE_WINDOW).surface(POINTERSURF).runWindow();
        if (WINDOW)
            return {.success = false, .error = std::format("Pointer focus is a window surface with class '{}'", WINDOW->metadata().appID())};

        return {.success = false, .error = std::format("Pointer focus is not a layer surface, view type is {}", VIEW ? sc<int>(VIEW->type()) : -1)};
    }

    if (LAYER->m_namespace != in)
        return {.success = false, .error = std::format("Pointer focus layer namespace is '{}', expected '{}'", LAYER->m_namespace, in)};

    return {};
}

static SDispatchResult setPointerFocusLayer(std::string in) {
    for (const auto& layer : Desktop::layerState()->layers()) {
        if (layer->m_namespace != in)
            continue;

        const auto SURFACE = layer->wlSurface() ? layer->wlSurface()->resource() : nullptr;
        if (!SURFACE)
            return {.success = false, .error = std::format("Layer '{}' has no surface", in)};

        const auto LOCAL = layer->m_geometry.size() / 2.0;

        g_pSeatManager->setPointerFocus(SURFACE, LOCAL);
        g_pSeatManager->sendPointerMotion(Time::millis(Time::steadyNow()), LOCAL);
        return {};
    }

    return {.success = false, .error = std::format("No layer with namespace '{}'", in)};
}

static SDispatchResult softFocusWindowByClass(std::string in) {
    for (const auto& window : Desktop::windowState()->windows()) {
        if (window->metadata().appID() != in)
            continue;

        Desktop::focusState()->rawWindowFocus(window, Desktop::FOCUS_REASON_FFM);
        return {};
    }

    return {.success = false, .error = std::format("No window with class '{}'", in)};
}

static SDispatchResult floatingFocusOnFullscreen(std::string in) {
    const auto PLASTWINDOW = Desktop::focusState()->window();

    if (!PLASTWINDOW)
        return {.success = false, .error = "No window"};

    if (!PLASTWINDOW->isFloating())
        return {.success = false, .error = "Window must be floating"};

    if (PLASTWINDOW->presentation().alphaTotalGoal() != 1.F)
        return {.success = false, .error = "floating window doesnt restore it opacity when focused on fullscreen workspace"};

    if (!PLASTWINDOW->fullscreenPolicy().allowedOverFullscreen())
        return {.success = false, .error = "floating window doesnt get flagged as allowedOverFullscreen"};

    return {};
}

static SDispatchResult expectWorkspaceLifecycleState(std::string monitorName) {
    const auto MONITOR = std::ranges::find(State::monitorState()->monitors(), monitorName, &Monitor::CMonitor::m_name);
    if (MONITOR == State::monitorState()->monitors().end() || !(*MONITOR)->m_activeWorkspace)
        return {.success = false, .error = std::format("Monitor '{}' has no active workspace", monitorName)};

    const auto WORKSPACE = (*MONITOR)->m_activeWorkspace;
    if (!WORKSPACE->visible())
        return {.success = false, .error = "Active lifecycle workspace is not visible"};
    if (WORKSPACE->m_alpha->value() != 1.F || WORKSPACE->m_alpha->goal() != 1.F)
        return {.success = false, .error = "Active lifecycle workspace alpha is not instantly IN"};
    if (WORKSPACE->m_renderOffset->value() != Vector2D{} || WORKSPACE->m_renderOffset->goal() != Vector2D{})
        return {.success = false, .error = "Active lifecycle workspace offset is not instantly IN"};

    return {};
}

static SDispatchResult expectNoMaximizeEcho(std::string in) {
    const auto WINDOW = Desktop::focusState()->window();
    if (!WINDOW)
        return {.success = false, .error = "No window"};

    if (WINDOW->fullscreenPolicy().consumeExpectedMaximizeEcho(true))
        return {.success = false, .error = "Window has a stale maximize echo expectation"};

    return {};
}

static int luaResult(lua_State* L, const SDispatchResult& result) {
    if (result.success)
        return 0;

    lua_pushstring(L, result.error.empty() ? "plugin function failed" : result.error.c_str());
    return lua_error(L);
}

static int luaTest(lua_State* L) {
    return luaResult(L, ::test(""));
}

static int luaStartCursorFixture(lua_State* L) {
    return luaResult(L, startCursorFixture(luaL_checkstring(L, 1)));
}

static int luaRestoreCursorFixture(lua_State* L) {
    restoreCursorFixture();
    return 0;
}

static int luaCursorFixtureImage(lua_State* L) {
    const std::string ACTION = luaL_checkstring(L, 1);
    if (!g_cursorFixture)
        return luaResult(L, {.success = false, .error = "Cursor fixture is not active"});

    if (ACTION == "default")
        Pointer::Cursor::mgr()->setCursorFromName("default");
    else if (ACTION == "reset")
        Pointer::mgr()->resetCursorImage();
    else if (ACTION == "surface")
        Pointer::mgr()->setCursorSurface(nullptr, {});
    else if (ACTION == "refresh") {
        Pointer::Cursor::mgr()->tickAnimatedCursor();
        Pointer::Cursor::mgr()->updateTheme();
        Pointer::Cursor::mgr()->tickAnimatedCursor();
    } else
        return luaResult(L, {.success = false, .error = "Unknown cursor fixture action"});
    return 0;
}

static int luaArmCursorRender(lua_State* L) {
    return luaResult(L, armCursorRender(luaL_checkstring(L, 1), sc<int>(luaL_checkinteger(L, 2))));
}

static int luaCheckCursorRender(lua_State* L) {
    if (!g_cursorRenderRecording)
        return luaResult(L, {.success = false, .error = "Cursor render probe is not armed"});
    if (!g_cursorRenderRecording->complete)
        return luaResult(L, {.success = false, .error = "Cursor render pending"});
    return luaResult(L, g_cursorRenderRecording->result);
}

static int luaSnapMove(lua_State* L) {
    return luaResult(L, ::snapMove(""));
}

static int luaDragWindow(lua_State* L) {
    const auto cls = std::string{luaL_checkstring(L, 1)};
    const auto x   = (double)luaL_checknumber(L, 2);
    const auto y   = (double)luaL_checknumber(L, 3);
    return luaResult(L, ::dragWindow(std::format("{},{},{}", cls, x, y)));
}

static int luaExpectWindowAtWorkspace(lua_State* L) {
    const auto WORKSPACE = std::string{luaL_checkstring(L, 1)};
    const auto POS       = Vector2D{luaL_checknumber(L, 2), luaL_checknumber(L, 3)};
    const auto EXPECTED  = std::string{luaL_checkstring(L, 4)};
    const auto IGNORE    = lua_gettop(L) > 4 ? std::string{luaL_checkstring(L, 5)} : std::string{};
    return luaResult(L, ::expectWindowAtWorkspace(WORKSPACE, POS, EXPECTED, IGNORE));
}

static int luaExpectWorkspaceRenameEvent(lua_State* L) {
    return luaResult(L, ::expectWorkspaceRenameEvent(luaL_checkstring(L, 1), luaL_checkstring(L, 2)));
}

static int luaArmMonitorRenderRecording(lua_State* L) {
    std::optional<bool> workspaceProbe;
    if (lua_gettop(L) > 1) {
        luaL_checktype(L, 2, LUA_TBOOLEAN);
        workspaceProbe = lua_toboolean(L, 2);
    }
    return luaResult(L, ::armMonitorRenderRecording(luaL_checkstring(L, 1), workspaceProbe));
}

static int luaCheckMonitorRenderRecording(lua_State* L) {
    luaL_checktype(L, 1, LUA_TBOOLEAN);
    return luaResult(L, ::checkMonitorRenderRecording(lua_toboolean(L, 1)));
}

static int luaResetMonitorRenderRecording(lua_State* L) {
    g_monitorRenderRecording.reset();
    return 0;
}

static int luaTestDragLifecycle(lua_State* L) {
    return luaResult(L, ::testDragLifecycle(luaL_checkstring(L, 1)));
}

static int luaArmPopupOpacity(lua_State* L) {
    const auto CLS  = std::string{luaL_checkstring(L, 1)};
    const auto FADE = sc<float>(luaL_checknumber(L, 2));
    luaL_checktype(L, 3, LUA_TBOOLEAN);
    if (lua_gettop(L) > 3)
        luaL_checktype(L, 4, LUA_TBOOLEAN);
    return luaResult(L, armPopupOpacity(CLS, FADE, lua_toboolean(L, 3), lua_toboolean(L, 4), luaL_optstring(L, 5, "")));
}

static int luaCheckPopupOpacity(lua_State* L) {
    if (!g_popupRenderRecording)
        return luaResult(L, {.success = false, .error = "Popup opacity probe pending"});
    if (!g_popupRenderRecording->complete)
        return luaResult(L, {.success = false, .error = std::format("Popup opacity probe pending: {}", g_popupRenderRecording->result.error)});
    return luaResult(L, g_popupRenderRecording->result);
}

static int luaResetPopupOpacity(lua_State* L) {
    g_popupRenderRecording.reset();
    return 0;
}

static int luaExpectDragState(lua_State* L) {
    luaL_checktype(L, 1, LUA_TBOOLEAN);
    luaL_checktype(L, 2, LUA_TBOOLEAN);
    const bool  ACTIVE     = lua_toboolean(L, 1);
    const bool  REACHED    = lua_toboolean(L, 2);
    const auto& CONTROLLER = g_layoutManager->dragController();
    return luaResult(
        L,
        {
            .success = !!CONTROLLER->target() == ACTIVE && CONTROLLER->dragThresholdReached() == REACHED,
            .error   = std::format("Expected drag active {} threshold {}, got active {} threshold {}", ACTIVE, REACHED, !!CONTROLLER->target(), CONTROLLER->dragThresholdReached()),
        });
}

static int luaTestPinchDeltaScale(lua_State* L) {
    return luaResult(L, ::testPinchDeltaScale(luaL_checknumber(L, 1)));
}

static int luaVkb(lua_State* L) {
    return luaResult(L, ::vkb(""));
}

static int luaAlt(lua_State* L) {
    return luaResult(L, ::pressAlt(std::to_string((int)luaL_checkinteger(L, 1))));
}

static int luaGesture(lua_State* L) {
    const auto direction = std::string{luaL_checkstring(L, 1)};
    const auto fingers   = (int)luaL_optinteger(L, 2, 3);
    return luaResult(L, ::simulateGesture(std::format("{},{}", direction, fingers)));
}

static int luaPinchUpdate(lua_State* L) {
    std::string in = std::format("{},{}", (int)luaL_checkinteger(L, 1), (double)luaL_checknumber(L, 2));

    if (lua_gettop(L) > 2)
        in += std::format(",{}", (double)luaL_checknumber(L, 3));
    if (lua_gettop(L) > 3)
        in += std::format(",{}", (double)luaL_checknumber(L, 4));
    if (lua_gettop(L) > 4)
        in += std::format(",{}", (double)luaL_checknumber(L, 5));

    return luaResult(L, ::pinchUpdate(in));
}

static int luaPinchEnd(lua_State* L) {
    return luaResult(L, ::pinchEnd(""));
}

static int luaExpectCursorZoom(lua_State* L) {
    const auto expected = (double)luaL_checknumber(L, 1);

    if (lua_gettop(L) > 1)
        return luaResult(L, ::expectCursorZoom(std::format("{},{}", expected, (double)luaL_checknumber(L, 2))));

    return luaResult(L, ::expectCursorZoom(std::format("{}", expected)));
}

static int luaScroll(lua_State* L) {
    return luaResult(L, ::scroll(std::to_string((double)luaL_checknumber(L, 1))));
}

static int luaClick(lua_State* L) {
    const auto button  = (int)luaL_checkinteger(L, 1);
    const auto pressed = (int)luaL_checkinteger(L, 2);
    return luaResult(L, ::click(std::format("{},{}", button, pressed)));
}

static int luaKeybind(lua_State* L) {
    const auto press    = (int)luaL_checkinteger(L, 1);
    const auto modifier = (int)luaL_checkinteger(L, 2);
    const auto key      = (int)luaL_checkinteger(L, 3);
    return luaResult(L, ::keybind(std::format("{},{},{}", press, modifier, key)));
}

static int luaKeybind2(lua_State* L) {
    const auto press    = (int)luaL_checkinteger(L, 1);
    const auto modifier = (int)luaL_checkinteger(L, 2);
    const auto key      = (int)luaL_checkinteger(L, 3);
    return luaResult(L, ::keybind2(std::format("{},{},{}", press, modifier, key)));
}

static int luaKeybindMask(lua_State* L) {
    const auto press        = (int)luaL_checkinteger(L, 1);
    const auto modifierMask = (int)luaL_checkinteger(L, 2);
    const auto key          = (int)luaL_checkinteger(L, 3);
    return luaResult(L, ::keybindModmask(std::format("{},{},{}", press, modifierMask, key)));
}

static int luaSetMods(lua_State* L) {
    const auto kbIndex   = (int)luaL_checkinteger(L, 1);
    const auto depressed = (int)luaL_checkinteger(L, 2);
    const auto latched   = (int)luaL_checkinteger(L, 3);
    const auto locked    = (int)luaL_checkinteger(L, 4);
    const auto group     = (int)luaL_checkinteger(L, 5);
    return luaResult(L, ::setMods(std::format("{},{},{},{},{}", kbIndex, depressed, latched, locked, group)));
}

static int luaRegisterKeyboardEventRecorder(lua_State* L) {
    return luaResult(L, ::registerKeyboardEventRecorder(""));
}

static int luaRemoveKeyboardEventRecorder(lua_State* L) {
    return luaResult(L, ::removeKeyboardEventRecorder(""));
}

static int luaExpectKeyboardEvents(lua_State* L) {
    const int ARGS = lua_gettop(L);
    if (ARGS % 2 != 0)
        return luaL_error(L, "expected keycode/state pairs");

    std::vector<CKeyboardEventRecorder::SEvent> expected;
    expected.reserve(ARGS / 2);
    for (int i = 1; i <= ARGS; i += 2) {
        expected.emplace_back(CKeyboardEventRecorder::SEvent{
            .keycode = sc<uint32_t>(luaL_checkinteger(L, i)),
            .state   = sc<wl_keyboard_key_state>(luaL_checkinteger(L, i + 1)),
        });
    }

    return luaResult(L, ::expectKeyboardEvents(expected));
}

static int luaNullfocus(lua_State* L) {
    return luaResult(L, ::nullfocus(""));
}

static int luaClearSurfaceFocus(lua_State* L) {
    return luaResult(L, ::clearSurfaceFocus(""));
}

static int luaCheckKeyboardFocusWindow(lua_State* L) {
    return luaResult(L, ::checkKeyboardFocusWindow(luaL_checkstring(L, 1)));
}

static int luaAddWindowRule(lua_State* L) {
    return luaResult(L, ::addWindowRule(""));
}

static int luaCheckWindowRule(lua_State* L) {
    return luaResult(L, ::checkWindowRule(""));
}

static int luaAddLayerRule(lua_State* L) {
    return luaResult(L, ::addLayerRule(""));
}

static int luaCheckLayerRule(lua_State* L) {
    return luaResult(L, ::checkLayerRule(""));
}

static int luaCheckPointerFocusWindow(lua_State* L) {
    return luaResult(L, ::checkPointerFocusWindow(luaL_checkstring(L, 1)));
}

static int luaCheckPointerFocusLayer(lua_State* L) {
    return luaResult(L, ::checkPointerFocusLayer(luaL_checkstring(L, 1)));
}

static int luaSetPointerFocusLayer(lua_State* L) {
    return luaResult(L, ::setPointerFocusLayer(luaL_checkstring(L, 1)));
}

static int luaSoftFocusWindowByClass(lua_State* L) {
    return luaResult(L, ::softFocusWindowByClass(luaL_checkstring(L, 1)));
}

static int luaFloatingFocusOnFullscreen(lua_State* L) {
    return luaResult(L, ::floatingFocusOnFullscreen(""));
}

static int luaExpectWorkspaceLifecycleState(lua_State* L) {
    return luaResult(L, ::expectWorkspaceLifecycleState(luaL_checkstring(L, 1)));
}

static int luaExpectNoMaximizeEcho(lua_State* L) {
    return luaResult(L, ::expectNoMaximizeEcho(""));
}

APICALL EXPORT PLUGIN_DESCRIPTION_INFO PLUGIN_INIT(HANDLE handle) {
    PHANDLE = handle;

    auto addLuaFn = [](const std::string& name, PLUGIN_LUA_FN fn) {
        if (!HyprlandAPI::addLuaFunction(PHANDLE, "test", name, fn))
            LOG(Log::ERR, "hyprtester plugin: failed to register hl.plugin.test.{}", name);
    };

    addLuaFn("test", ::luaTest);
    addLuaFn("start_cursor_fixture", ::luaStartCursorFixture);
    addLuaFn("restore_cursor_fixture", ::luaRestoreCursorFixture);
    addLuaFn("cursor_fixture_image", ::luaCursorFixtureImage);
    addLuaFn("arm_cursor_render", ::luaArmCursorRender);
    addLuaFn("check_cursor_render", ::luaCheckCursorRender);
    addLuaFn("snapmove", ::luaSnapMove);
    addLuaFn("drag_window", ::luaDragWindow);
    addLuaFn("expect_window_at_workspace", ::luaExpectWindowAtWorkspace);
    addLuaFn("expect_workspace_rename_event", ::luaExpectWorkspaceRenameEvent);
    addLuaFn("arm_monitor_render_recording", ::luaArmMonitorRenderRecording);
    addLuaFn("check_monitor_render_recording", ::luaCheckMonitorRenderRecording);
    addLuaFn("reset_monitor_render_recording", ::luaResetMonitorRenderRecording);
    addLuaFn("arm_popup_opacity", ::luaArmPopupOpacity);
    addLuaFn("check_popup_opacity", ::luaCheckPopupOpacity);
    addLuaFn("reset_popup_opacity", ::luaResetPopupOpacity);
    addLuaFn("test_drag_lifecycle", ::luaTestDragLifecycle);
    addLuaFn("expect_drag_state", ::luaExpectDragState);
    addLuaFn("test_pinch_delta_scale", ::luaTestPinchDeltaScale);
    addLuaFn("vkb", ::luaVkb);
    addLuaFn("alt", ::luaAlt);
    addLuaFn("gesture", ::luaGesture);
    addLuaFn("pinch_update", ::luaPinchUpdate);
    addLuaFn("pinch_end", ::luaPinchEnd);
    addLuaFn("expect_cursor_zoom", ::luaExpectCursorZoom);
    addLuaFn("scroll", ::luaScroll);
    addLuaFn("click", ::luaClick);
    addLuaFn("keybind", ::luaKeybind);
    addLuaFn("keybind2", ::luaKeybind2);
    addLuaFn("keybind_modmask", ::luaKeybindMask);
    addLuaFn("set_mods", ::luaSetMods);
    addLuaFn("register_keyboard_event_recorder", ::luaRegisterKeyboardEventRecorder);
    addLuaFn("remove_keyboard_event_recorder", ::luaRemoveKeyboardEventRecorder);
    addLuaFn("expect_keyboard_events", ::luaExpectKeyboardEvents);
    addLuaFn("nullfocus", ::luaNullfocus);
    addLuaFn("clear_surface_focus", ::luaClearSurfaceFocus);
    addLuaFn("check_keyboard_focus_window", ::luaCheckKeyboardFocusWindow);
    addLuaFn("add_window_rule", ::luaAddWindowRule);
    addLuaFn("check_window_rule", ::luaCheckWindowRule);
    addLuaFn("add_layer_rule", ::luaAddLayerRule);
    addLuaFn("check_layer_rule", ::luaCheckLayerRule);
    addLuaFn("check_pointer_focus_window", ::luaCheckPointerFocusWindow);
    addLuaFn("check_pointer_focus_layer", ::luaCheckPointerFocusLayer);
    addLuaFn("set_pointer_focus_layer", ::luaSetPointerFocusLayer);
    addLuaFn("window_soft_focus", ::luaSoftFocusWindowByClass);
    addLuaFn("floating_focus_on_fullscreen", ::luaFloatingFocusOnFullscreen);
    addLuaFn("expect_workspace_lifecycle_state", ::luaExpectWorkspaceLifecycleState);
    addLuaFn("expect_no_maximize_echo", ::luaExpectNoMaximizeEcho);
    WorkspaceGestures::registerFunctions();
    SpecialWorkspaceGestures::registerFunctions();
    XWaylandSelection::registerFunctions();

    // init mouse
    g_mouse = CTestMouse::create(false);
    g_pInputManager->newMouse(g_mouse);

    // init keyboard
    g_keyboard = CTestKeyboard::create(false);
    g_pInputManager->newKeyboard(g_keyboard);

    // init keyboard2
    g_keyboard2 = CTestKeyboard::create(false);
    g_pInputManager->newKeyboard(g_keyboard2);

    return {"hyprtestplugin", "hyprtestplugin", "Vaxry", "1.0"};
}

APICALL EXPORT void PLUGIN_EXIT() {
    WorkspaceGestures::reset();
    SpecialWorkspaceGestures::reset();
    restoreCursorFixture();
    g_popupRenderRecording.reset();
    g_monitorRenderRecording.reset();
    removeKeyboardEventRecorder("");
    g_keyboardEventRecorder.reset();
    g_mouse->destroy();
    g_mouse.reset();
    g_keyboard->destroy();
    g_keyboard.reset();
    g_keyboard2->destroy();
    g_keyboard2.reset();
}
