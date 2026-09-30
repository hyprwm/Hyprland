#include <render/scene/MonitorScene.hpp>
#include <helpers/time/Time.hpp>

#include <gtest/gtest.h>

TEST(MonitorScene, NullMonitorDrawIsNoOpThroughSceneInterface) {
    Render::CMonitorScene monitorScene{PHLMONITORREF{}};
    Render::IScene&       scene = monitorScene;

    // No compositor or renderer is initialized in this test.
    EXPECT_NO_THROW(scene.draw(Time::steady_tp{}));
}
