#include <state/MonitorPositionController.hpp>

#include <config/shared/monitor/MonitorRule.hpp>

#include <gtest/gtest.h>

#include <optional>
#include <string>
#include <vector>

class CPositionTestMonitor : public Monitor::IMonitorArrangeable {
  public:
    CPositionTestMonitor(MONITORID id, const std::string& name, const Vector2D& size) : m_id(id), m_name(name), m_size(size), m_transformedSize(size) {
        ;
    }

    virtual MONITORID id() const override {
        return m_id;
    }

    virtual std::string_view name() const override {
        return m_name;
    }

    virtual std::string_view description() const override {
        return m_description;
    }

    virtual std::string_view shortDescription() const override {
        return m_shortDescription;
    }

    virtual bool matchesStaticSelector(std::string_view selector) const override {
        return selector == m_name;
    }

    virtual Vector2D position() const override {
        return m_position;
    }

    virtual Vector2D size() const override {
        return m_size;
    }

    virtual Vector2D pixelSize() const override {
        return m_size;
    }

    virtual Vector2D transformedSize() const override {
        return m_transformedSize;
    }

    virtual float scale() const override {
        return m_scale;
    }

    virtual Hyprutils::Math::eTransform transform() const override {
        return m_transform;
    }

    virtual CBox logicalBox() const override {
        return {m_position, m_size};
    }

    virtual CBox logicalBoxMinusReserved() const override {
        return logicalBox();
    }

    virtual Vector2D middle() const override {
        return m_position + m_size / 2.F;
    }

    virtual void moveTo(const Vector2D& pos) override {
        m_position = pos;
    }

    virtual std::optional<Vector2D> explicitPosition() const override {
        return m_explicitPosition;
    }

    virtual Config::eAutoDirs autoDirection() const override {
        return m_autoDirection;
    }

    virtual Vector2D xwaylandPosition() const override {
        return m_xwaylandPosition;
    }

    virtual float xwaylandScale() const override {
        return m_xwaylandScale;
    }

    virtual void setXWaylandPosition(const Vector2D& pos) override {
        m_xwaylandPosition = pos;
    }

    virtual void setXWaylandScale(float scale) override {
        m_xwaylandScale = scale;
    }

    MONITORID                   m_id = 0;
    std::string                 m_name;
    std::string                 m_description;
    std::string                 m_shortDescription;
    Vector2D                    m_position;
    Vector2D                    m_size;
    Vector2D                    m_transformedSize;
    float                       m_scale            = 1.F;
    Hyprutils::Math::eTransform m_transform        = Hyprutils::Math::HYPRUTILS_TRANSFORM_NORMAL;
    std::optional<Vector2D>     m_explicitPosition = {};
    Config::eAutoDirs           m_autoDirection    = Config::DIR_AUTO_RIGHT;
    Vector2D                    m_xwaylandPosition;
    float                       m_xwaylandScale = 1.F;
};

static SP<CPositionTestMonitor> testMonitor(MONITORID id, const std::string& name, const Vector2D& size) {
    return makeShared<CPositionTestMonitor>(id, name, size);
}

static SP<Monitor::IMonitorArrangeable> arrangeable(const SP<CPositionTestMonitor>& monitor) {
    return dynamicPointerCast<Monitor::IMonitorArrangeable>(monitor);
}

static void arrange(const std::vector<SP<CPositionTestMonitor>>& monitors, bool xwaylandForceZeroScaling = false) {
    std::vector<SP<Monitor::IMonitorArrangeable>> arrangeableMonitors;
    arrangeableMonitors.reserve(monitors.size());

    for (const auto& monitor : monitors)
        arrangeableMonitors.push_back(arrangeable(monitor));

    State::CMonitorPositionController{}.arrange(arrangeableMonitors, xwaylandForceZeroScaling);
}

TEST(MonitorPositionController, explicitPositionsAreAppliedFirst) {
    const auto left  = testMonitor(0, "left", {100, 100});
    const auto right = testMonitor(1, "right", {100, 100});
    const auto autoM = testMonitor(2, "auto", {50, 50});

    left->m_explicitPosition  = Vector2D{-100, 0};
    right->m_explicitPosition = Vector2D{100, 0};
    autoM->m_autoDirection    = Config::DIR_AUTO_RIGHT;

    arrange({left, autoM, right});

    EXPECT_EQ(left->m_position, Vector2D(-100, 0));
    EXPECT_EQ(right->m_position, Vector2D(100, 0));
    EXPECT_EQ(autoM->m_position, Vector2D(200, 0));
}

TEST(MonitorPositionController, autoRightPlacesAfterRightmostEdge) {
    const auto explicitM = testMonitor(0, "explicit", {100, 100});
    const auto autoM     = testMonitor(1, "auto", {50, 50});

    explicitM->m_explicitPosition = Vector2D{0, 0};
    autoM->m_autoDirection        = Config::DIR_AUTO_RIGHT;

    arrange({explicitM, autoM});

    EXPECT_EQ(autoM->m_position, Vector2D(100, 0));
}

TEST(MonitorPositionController, autoLeftPlacesBeforeLeftmostEdge) {
    const auto explicitM = testMonitor(0, "explicit", {100, 100});
    const auto autoM     = testMonitor(1, "auto", {50, 50});

    explicitM->m_explicitPosition = Vector2D{0, 0};
    autoM->m_autoDirection        = Config::DIR_AUTO_LEFT;

    arrange({explicitM, autoM});

    EXPECT_EQ(autoM->m_position, Vector2D(-50, 0));
}

TEST(MonitorPositionController, autoDownPlacesBelowLowestEdge) {
    const auto explicitM = testMonitor(0, "explicit", {100, 100});
    const auto autoM     = testMonitor(1, "auto", {50, 50});

    explicitM->m_explicitPosition = Vector2D{0, 0};
    autoM->m_autoDirection        = Config::DIR_AUTO_DOWN;

    arrange({explicitM, autoM});

    EXPECT_EQ(autoM->m_position, Vector2D(0, 100));
}

TEST(MonitorPositionController, autoUpPlacesAboveHighestEdge) {
    const auto explicitM = testMonitor(0, "explicit", {100, 100});
    const auto autoM     = testMonitor(1, "auto", {50, 50});

    explicitM->m_explicitPosition = Vector2D{0, 0};
    autoM->m_autoDirection        = Config::DIR_AUTO_UP;

    arrange({explicitM, autoM});

    EXPECT_EQ(autoM->m_position, Vector2D(0, -50));
}

TEST(MonitorPositionController, autoCenterRightCentersVertically) {
    const auto explicitM = testMonitor(0, "explicit", {100, 100});
    const auto autoM     = testMonitor(1, "auto", {50, 40});

    explicitM->m_explicitPosition = Vector2D{0, 0};
    autoM->m_autoDirection        = Config::DIR_AUTO_CENTER_RIGHT;

    arrange({explicitM, autoM});

    EXPECT_EQ(autoM->m_position, Vector2D(100, 30));
}

TEST(MonitorPositionController, autoCenterDownCentersHorizontally) {
    const auto explicitM = testMonitor(0, "explicit", {100, 100});
    const auto autoM     = testMonitor(1, "auto", {40, 50});

    explicitM->m_explicitPosition = Vector2D{0, 0};
    autoM->m_autoDirection        = Config::DIR_AUTO_CENTER_DOWN;

    arrange({explicitM, autoM});

    EXPECT_EQ(autoM->m_position, Vector2D(30, 100));
}

TEST(MonitorPositionController, xwaylandPositionsUseLogicalSizeByDefault) {
    const auto first  = testMonitor(0, "first", {100, 100});
    const auto second = testMonitor(1, "second", {50, 50});

    first->m_explicitPosition  = Vector2D{0, 0};
    second->m_explicitPosition = Vector2D{100, 0};
    first->m_transformedSize   = Vector2D{200, 100};
    first->m_scale             = 2.F;

    arrange({first, second}, false);

    EXPECT_EQ(first->m_xwaylandPosition, Vector2D(0, 0));
    EXPECT_EQ(second->m_xwaylandPosition, Vector2D(100, 0));
    EXPECT_FLOAT_EQ(first->m_xwaylandScale, 1.F);
    EXPECT_FLOAT_EQ(second->m_xwaylandScale, 1.F);
}

TEST(MonitorPositionController, xwaylandPositionsUseTransformedSizeWhenForceZeroScaling) {
    const auto first  = testMonitor(0, "first", {100, 100});
    const auto second = testMonitor(1, "second", {50, 50});

    first->m_explicitPosition  = Vector2D{0, 0};
    second->m_explicitPosition = Vector2D{100, 0};
    first->m_transformedSize   = Vector2D{200, 100};
    first->m_scale             = 2.F;

    arrange({first, second}, true);

    EXPECT_EQ(first->m_xwaylandPosition, Vector2D(0, 0));
    EXPECT_EQ(second->m_xwaylandPosition, Vector2D(200, 0));
    EXPECT_FLOAT_EQ(first->m_xwaylandScale, 2.F);
    EXPECT_FLOAT_EQ(second->m_xwaylandScale, 1.F);
}
