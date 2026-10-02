#include <render/scene/MonitorScene.hpp>
#include <render/Context.hpp>
#include <helpers/time/Time.hpp>

#include <gtest/gtest.h>

TEST(MonitorScene, NullMonitorDrawIsNoOpThroughSceneInterface) {
    Render::CMonitorScene monitorScene{PHLMONITORREF{}};
    Render::IScene&       scene = monitorScene;

    // No compositor or renderer is initialized in this test.
    Render::CRenderContext ctx;
    EXPECT_NO_THROW(scene.draw(ctx, Time::steady_tp{}));
}
