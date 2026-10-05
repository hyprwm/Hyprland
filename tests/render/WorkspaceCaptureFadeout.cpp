#include <desktop/state/Fadeout.hpp>
#include <protocols/types/Buffer.hpp>
#include <render/Context.hpp>
#include <render/Framebuffer.hpp>
#include <render/WindowRenderPresentation.hpp>
#include <render/scene/SceneSelection.hpp>

#include <gtest/gtest.h>

using namespace Desktop;
using namespace Render;

class CWorkspaceFadeoutTestFramebuffer : public IFramebuffer {
  public:
    void release() override {
        ;
    }
    bool readPixels(CHLBufferReference, uint32_t, uint32_t, uint32_t, uint32_t) override {
        ADD_FAILURE() << "Unexpected snapshot readback";
        return false;
    }
    void bind() override {
        ADD_FAILURE() << "Unexpected snapshot bind";
    }
    void addStencil(SP<ITexture>) override {
        ADD_FAILURE() << "Unexpected stencil allocation";
    }

  private:
    bool internalAlloc(int, int, DRMFormat) override {
        ADD_FAILURE() << "Unexpected snapshot allocation";
        return false;
    }
};

class CWorkspaceCaptureTestFadeout : public IFadeout {
  public:
    CWorkspaceCaptureTestFadeout(eFadeoutPlane plane, const SFadeoutSource& source, SP<IFramebuffer> monitorSnapshot, SP<IFramebuffer> workspaceSnapshot) : m_plane(plane) {
        m_source               = source;
        m_framebuffer          = std::move(monitorSnapshot);
        m_workspaceFramebuffer = std::move(workspaceSnapshot);
    }

    PHLMONITORREF monitor() const override {
        return {};
    }
    eFadeoutPlane plane() const override {
        return m_plane;
    }
    int zIndex() const override {
        return 0;
    }
    CBox renderBox() const override {
        return {};
    }
    float alpha() const override {
        return 0.5F;
    }
    bool done() const override {
        return false;
    }

  private:
    eFadeoutPlane m_plane;
};

TEST(WorkspaceCaptureFadeout, WindowAndWindowPopupSnapshotsNeverCrossScenes) {
    const auto monitorSnapshot   = makeShared<CWorkspaceFadeoutTestFramebuffer>();
    const auto workspaceSnapshot = makeShared<CWorkspaceFadeoutTestFramebuffer>();

    for (const auto plane : {FADEOUT_PLANE_WINDOW_TILED, FADEOUT_PLANE_WINDOW_FLOATING, FADEOUT_PLANE_WINDOW_OVER_FULLSCREEN, FADEOUT_PLANE_POPUP}) {
        for (const bool monitorAvailable : {false, true}) {
            for (const bool workspaceAvailable : {false, true}) {
                const CWorkspaceCaptureTestFadeout fadeout{
                    plane, {.type = eFadeoutSource::WINDOW}, monitorAvailable ? monitorSnapshot : nullptr, workspaceAvailable ? workspaceSnapshot : nullptr};
                EXPECT_EQ(fadeout.framebuffer(), monitorAvailable ? monitorSnapshot : nullptr);
                EXPECT_EQ(fadeout.framebuffer(eSceneMode::MONITOR), fadeout.framebuffer());
                for (const auto mode : {eSceneMode::WORKSPACE_WINDOWS, eSceneMode::WORKSPACE_WITH_SHELL})
                    EXPECT_EQ(fadeout.framebuffer(mode), workspaceAvailable ? workspaceSnapshot : nullptr);
            }
        }
    }
}

TEST(WorkspaceCaptureFadeout, MissingNeutralSnapshotOmitsPinnedWindowAndPopupEvenInShellMode) {
    const auto monitorSnapshot = makeShared<CWorkspaceFadeoutTestFramebuffer>();
    for (const auto plane : {FADEOUT_PLANE_WINDOW_FLOATING, FADEOUT_PLANE_WINDOW_OVER_FULLSCREEN, FADEOUT_PLANE_POPUP}) {
        const CWorkspaceCaptureTestFadeout fadeout{plane, {.type = eFadeoutSource::WINDOW, .pinned = true}, monitorSnapshot, nullptr};
        const auto                         source = fadeout.source();
        EXPECT_TRUE(sceneSelectsFadeout(eSceneMode::WORKSPACE_WITH_SHELL, source.type, false, source.pinned));
        EXPECT_FALSE(fadeout.framebuffer(eSceneMode::WORKSPACE_WITH_SHELL));
        EXPECT_EQ(fadeout.framebuffer(eSceneMode::MONITOR), monitorSnapshot);
    }
}

TEST(WorkspaceCaptureFadeout, ShellLayersAndLayerPopupsKeepLiveSnapshots) {
    const auto monitorSnapshot   = makeShared<CWorkspaceFadeoutTestFramebuffer>();
    const auto workspaceSnapshot = makeShared<CWorkspaceFadeoutTestFramebuffer>();
    for (const auto plane : {FADEOUT_PLANE_LAYER_BACKGROUND, FADEOUT_PLANE_LAYER_BOTTOM, FADEOUT_PLANE_LAYER_TOP, FADEOUT_PLANE_LAYER_OVERLAY, FADEOUT_PLANE_POPUP}) {
        const CWorkspaceCaptureTestFadeout fadeout{plane, {.type = eFadeoutSource::LAYER}, monitorSnapshot, workspaceSnapshot};
        EXPECT_EQ(fadeout.framebuffer(), monitorSnapshot);
        EXPECT_EQ(fadeout.framebuffer(eSceneMode::MONITOR), monitorSnapshot);
        EXPECT_EQ(fadeout.framebuffer(eSceneMode::WORKSPACE_WITH_SHELL), monitorSnapshot);
        EXPECT_FALSE(fadeout.framebuffer(eSceneMode::WORKSPACE_WINDOWS));
    }
}

TEST(WorkspaceCaptureFadeout, UnclassifiedPopupDoesNotExposeEitherSnapshotToWorkspaceScenes) {
    const auto                         monitorSnapshot   = makeShared<CWorkspaceFadeoutTestFramebuffer>();
    const auto                         workspaceSnapshot = makeShared<CWorkspaceFadeoutTestFramebuffer>();
    const CWorkspaceCaptureTestFadeout fadeout{FADEOUT_PLANE_POPUP, {}, monitorSnapshot, workspaceSnapshot};
    EXPECT_EQ(fadeout.framebuffer(eSceneMode::MONITOR), monitorSnapshot);
    EXPECT_FALSE(fadeout.framebuffer(eSceneMode::WORKSPACE_WINDOWS));
    EXPECT_FALSE(fadeout.framebuffer(eSceneMode::WORKSPACE_WITH_SHELL));
}

TEST(WorkspaceCaptureFadeout, SnapshotAndOriginSurviveProducerLifetime) {
    for (const auto plane : {FADEOUT_PLANE_WINDOW_FLOATING, FADEOUT_PLANE_POPUP}) {
        WP<IFramebuffer>                 monitorSnapshot;
        WP<IFramebuffer>                 workspaceSnapshot;
        UP<CWorkspaceCaptureTestFadeout> fadeout;
        {
            auto           monitor   = makeShared<CWorkspaceFadeoutTestFramebuffer>();
            auto           workspace = makeShared<CWorkspaceFadeoutTestFramebuffer>();
            SFadeoutSource source{.type = eFadeoutSource::WINDOW, .pinned = true, .noScreenShare = true};
            monitorSnapshot   = monitor;
            workspaceSnapshot = workspace;
            fadeout           = makeUnique<CWorkspaceCaptureTestFadeout>(plane, source, monitor, workspace);
            source            = {};
        }

        EXPECT_FALSE(monitorSnapshot.expired());
        EXPECT_FALSE(workspaceSnapshot.expired());
        EXPECT_EQ(fadeout->framebuffer(eSceneMode::MONITOR), monitorSnapshot.lock());
        EXPECT_EQ(fadeout->framebuffer(eSceneMode::WORKSPACE_WITH_SHELL), workspaceSnapshot.lock());
        EXPECT_EQ(fadeout->source().type, eFadeoutSource::WINDOW);
        EXPECT_TRUE(fadeout->source().pinned);
        EXPECT_TRUE(fadeout->source().noScreenShare);

        CRenderContext ctx;
        ctx.m_renderingCapture = true;
        EXPECT_FALSE(ctx.shouldRenderContent(fadeout->source().noScreenShare));
        ctx.m_renderingCapture = false;
        EXPECT_TRUE(ctx.shouldRenderContent(fadeout->source().noScreenShare));

        fadeout.reset();
        EXPECT_TRUE(monitorSnapshot.expired());
        EXPECT_TRUE(workspaceSnapshot.expired());
    }
}

TEST(WorkspaceCaptureFadeout, NeutralOnlySnapshotIsRetainedWithoutExposingItToMonitorRendering) {
    WP<IFramebuffer> workspaceSnapshot;
    {
        auto snapshot     = makeShared<CWorkspaceFadeoutTestFramebuffer>();
        workspaceSnapshot = snapshot;
        CWorkspaceCaptureTestFadeout fadeout{FADEOUT_PLANE_POPUP, {.type = eFadeoutSource::WINDOW}, nullptr, snapshot};
        snapshot.reset();
        EXPECT_FALSE(workspaceSnapshot.expired());
        EXPECT_FALSE(fadeout.framebuffer());
        EXPECT_FALSE(fadeout.framebuffer(eSceneMode::MONITOR));
        EXPECT_EQ(fadeout.framebuffer(eSceneMode::WORKSPACE_WINDOWS), workspaceSnapshot.lock());
    }
    EXPECT_TRUE(workspaceSnapshot.expired());
}

TEST(WorkspaceCaptureFadeout, InactiveWindowSnapshotAppliesFadeOnlyOnReplay) {
    const SWindowPresentationState state{
        .workspaceOffset             = {1920, -1080},
        .floatingOffset              = {37, 23},
        .workspaceAlpha              = 0.F,
        .fade                        = 0.8F,
        .active                      = 0.75F,
        .fullscreen                  = 0.6F,
        .layout                      = 0.5F,
        .moveToWorkspace             = 0.F,
        .moveFromWorkspace           = 0.F,
        .hasWorkspacePresentation    = true,
        .workspaceOffsetAnimating    = true,
        .workspaceVisible            = false,
        .floatingOffsetFromWorkspace = true,
    };
    const auto monitor = resolveWindowPresentation(state);
    EXPECT_FALSE(monitor.alphaVisible);

    for (const auto mode : {eSceneMode::WORKSPACE_WINDOWS, eSceneMode::WORKSPACE_WITH_SHELL}) {
        const auto live     = resolveWindowPresentation(state, mode);
        const auto snapshot = resolveWindowSnapshotPresentation(live);
        EXPECT_TRUE(snapshot.workspaceScene);
        EXPECT_TRUE(snapshot.alphaVisible);
        EXPECT_EQ(snapshot.workspaceOffset, Vector2D{});
        EXPECT_EQ(snapshot.floatingOffset, Vector2D{});
        EXPECT_FLOAT_EQ(snapshot.workspaceAlpha, 1.F);
        EXPECT_FLOAT_EQ(snapshot.alpha, state.active);
        EXPECT_FLOAT_EQ(snapshot.fadeAlpha, 1.F);
        EXPECT_FALSE(snapshot.workspaceOffsetAnimating);
        // Replay starts at the window's fade/fullscreen/layout product (0.24), not its square.
        EXPECT_FLOAT_EQ(live.fadeAlpha, 0.24F);
        EXPECT_FLOAT_EQ(snapshot.alpha * snapshot.fadeAlpha * live.fadeAlpha, 0.18F);
        EXPECT_EQ(resolveWindowPresentation(state, mode), live);
        EXPECT_EQ(resolveWindowPresentation(state), monitor);
    }
}

TEST(WorkspaceCaptureFadeout, SnapshotNormalizationPreservesOriginalVisibilityAndActiveOpacity) {
    for (const auto mode : {eSceneMode::WORKSPACE_WINDOWS, eSceneMode::WORKSPACE_WITH_SHELL}) {
        for (const auto channel : {&SWindowPresentationState::fade, &SWindowPresentationState::active, &SWindowPresentationState::fullscreen, &SWindowPresentationState::layout}) {
            for (const bool animating : {false, true}) {
                SWindowPresentationState state{
                    .active         = 0.75F,
                    .alphaAnimating = animating,
                };
                state.*channel      = 0.F;
                const auto live     = resolveWindowPresentation(state, mode);
                const auto snapshot = resolveWindowSnapshotPresentation(live);
                EXPECT_EQ(snapshot.alphaVisible, animating);
                EXPECT_EQ(snapshot.alphaVisible, live.alphaVisible);
                EXPECT_FLOAT_EQ(snapshot.alpha, state.active);
                EXPECT_FLOAT_EQ(snapshot.fadeAlpha, 1.F);
                EXPECT_EQ(resolveWindowPresentation(state, mode), live);
            }
        }
    }
}

TEST(WorkspaceCaptureFadeout, SnapshotNormalizationPreservesWindowLocalPresentation) {
    for (const auto mode : {eSceneMode::WORKSPACE_WINDOWS, eSceneMode::WORKSPACE_WITH_SHELL}) {
        const SWindowPresentationState state{
            .workspaceOffset          = {1920, -1080},
            .floatingOffset           = {37, 23},
            .workspaceAlpha           = 0.25F,
            .fade                     = 0.8F,
            .active                   = 0.75F,
            .fullscreen               = 0.6F,
            .layout                   = 0.5F,
            .hasWorkspacePresentation = true,
            .workspaceOffsetAnimating = true,
        };
        const auto live     = resolveWindowPresentation(state, mode);
        auto       snapshot = resolveWindowSnapshotPresentation(live);
        EXPECT_EQ(snapshot.floatingOffset, state.floatingOffset);
        EXPECT_FLOAT_EQ(snapshot.fadeAlpha, 1.F);
        snapshot.fadeAlpha = live.fadeAlpha;
        EXPECT_EQ(snapshot, live);
    }
}

TEST(WorkspaceCaptureFadeout, MonitorSnapshotPresentationIsUnchanged) {
    for (const bool visible : {false, true}) {
        for (const bool animating : {false, true}) {
            const SWindowPresentationState state{
                .workspaceOffset          = {1920, -1080},
                .floatingOffset           = {37, 23},
                .workspaceAlpha           = 0.25F,
                .fade                     = 0.8F,
                .active                   = 0.75F,
                .fullscreen               = 0.6F,
                .layout                   = 0.5F,
                .moveToWorkspace          = 0.F,
                .moveFromWorkspace        = 0.3F,
                .hasWorkspacePresentation = true,
                .workspaceOffsetAnimating = true,
                .movingFromMonitor        = true,
                .workspaceVisible         = visible,
                .alphaAnimating           = animating,
            };
            const auto monitor = resolveWindowPresentation(state);
            EXPECT_EQ(resolveWindowSnapshotPresentation(monitor), monitor);
        }
    }
}
