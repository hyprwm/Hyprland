#include <state/MonitorQueryCore.hpp>

#include <aquamarine/output/Output.hpp>
#include <gtest/gtest.h>
#include <hyprutils/string/String.hpp>

#include <optional>
#include <string>
#include <utility>
#include <vector>

using namespace Hyprutils::String;

class CQueryTestMonitor : public Monitor::IMonitorQueryable {
  public:
    CQueryTestMonitor(MONITORID id, const std::string& name, const Vector2D& position, const Vector2D& size) : m_id(id), m_name(name), m_position(position), m_size(size) {
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
        if (m_selectorResult.has_value())
            return *m_selectorResult;

        if (selector.starts_with("desc:")) {
            const auto DESCRIPTIONSELECTOR = trim(selector.substr(5));
            return m_description.starts_with(DESCRIPTIONSELECTOR) || m_shortDescription.starts_with(DESCRIPTIONSELECTOR);
        }

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
        return m_size;
    }

    virtual float scale() const override {
        return 1.F;
    }

    virtual Hyprutils::Math::eTransform transform() const override {
        return Hyprutils::Math::HYPRUTILS_TRANSFORM_NORMAL;
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

    virtual bool enabled() const override {
        return m_enabled;
    }

    virtual bool hasOutput() const override {
        return m_hasOutput;
    }

    virtual SP<Aquamarine::IOutput> output() const override {
        return m_output;
    }

    MONITORID               m_id = 0;
    std::string             m_name;
    std::string             m_description;
    std::string             m_shortDescription;
    Vector2D                m_position;
    Vector2D                m_size;
    bool                    m_enabled   = true;
    bool                    m_hasOutput = true;
    SP<Aquamarine::IOutput> m_output;
    std::optional<bool>     m_selectorResult;
};

static SP<CQueryTestMonitor> testMonitor(MONITORID id, const std::string& name, const Vector2D& position, const Vector2D& size = {100, 100}) {
    return makeShared<CQueryTestMonitor>(id, name, position, size);
}

static SP<Monitor::IMonitorQueryable> queryable(const SP<CQueryTestMonitor>& monitor) {
    return dynamicPointerCast<Monitor::IMonitorQueryable>(monitor);
}

static std::vector<SP<Monitor::IMonitorQueryable>> queryables(const std::vector<SP<CQueryTestMonitor>>& monitors) {
    std::vector<SP<Monitor::IMonitorQueryable>> result;
    result.reserve(monitors.size());

    for (const auto& monitor : monitors)
        result.push_back(queryable(monitor));

    return result;
}

TEST(MonitorQueryCore, queryById) {
    const auto first    = testMonitor(0, "DP-1", {0, 0});
    const auto second   = testMonitor(1, "DP-2", {100, 0});
    auto       monitors = queryables({first, second});

    EXPECT_EQ(std::move(State::CMonitorQueryCore{monitors}).id(1).run(), queryable(second));
}

TEST(MonitorQueryCore, queryByName) {
    const auto first    = testMonitor(0, "DP-1", {0, 0});
    const auto second   = testMonitor(1, "HDMI-A-1", {100, 0});
    auto       monitors = queryables({first, second});

    EXPECT_EQ(std::move(State::CMonitorQueryCore{monitors}).name("HDMI-A-1").run(), queryable(second));
}

TEST(MonitorQueryCore, queryByDescriptionPrefix) {
    const auto first    = testMonitor(0, "DP-1", {0, 0});
    auto       monitors = queryables({first});

    first->m_description = "Dell Inc. ABC";

    EXPECT_EQ(std::move(State::CMonitorQueryCore{monitors}).description("Dell").run(), queryable(first));
    EXPECT_FALSE(std::move(State::CMonitorQueryCore{monitors}).description("Nope").run());
}

TEST(MonitorQueryCore, queryByVectorInsideBox) {
    const auto first    = testMonitor(0, "DP-1", {0, 0});
    const auto second   = testMonitor(1, "DP-2", {100, 0});
    auto       monitors = queryables({first, second});

    EXPECT_EQ(std::move(State::CMonitorQueryCore{monitors}).vec({150, 50}).run(), queryable(second));
}

TEST(MonitorQueryCore, closestMonitorForVectorOutsideAllBoxes) {
    const auto first    = testMonitor(0, "DP-1", {0, 0});
    const auto second   = testMonitor(1, "DP-2", {300, 0});
    auto       monitors = queryables({first, second});

    EXPECT_EQ(std::move(State::CMonitorQueryCore{monitors}).vec({250, 50}).run(), queryable(second));
}

TEST(MonitorQueryCore, directionLookupRight) {
    const auto first    = testMonitor(0, "DP-1", {0, 0});
    const auto second   = testMonitor(1, "DP-2", {100, 0});
    auto       monitors = queryables({first, second});

    EXPECT_EQ(std::move(State::CMonitorQueryCore{monitors}).relativeTo(queryable(first)).inDirection(Math::DIRECTION_RIGHT).run(), queryable(second));
}

TEST(MonitorQueryCore, directionLookupChoosesLongestIntersection) {
    const auto reference = testMonitor(0, "reference", {0, 0}, {100, 100});
    const auto small     = testMonitor(1, "small", {100, 80}, {100, 20});
    const auto large     = testMonitor(2, "large", {100, 0}, {100, 60});
    auto       monitors  = queryables({reference, small, large});

    EXPECT_EQ(std::move(State::CMonitorQueryCore{monitors}).relativeTo(queryable(reference)).inDirection(Math::DIRECTION_RIGHT).run(), queryable(large));
}

TEST(MonitorQueryCore, configCurrentReturnsRelativeMonitor) {
    const auto first    = testMonitor(0, "DP-1", {0, 0});
    const auto second   = testMonitor(1, "DP-2", {100, 0});
    auto       monitors = queryables({first, second});

    EXPECT_EQ(std::move(State::CMonitorQueryCore{monitors}).relativeTo(queryable(second)).configString("current").run(), queryable(second));
}

TEST(MonitorQueryCore, configDirectionUsesRelativeMonitor) {
    const auto first    = testMonitor(0, "DP-1", {0, 0});
    const auto second   = testMonitor(1, "DP-2", {100, 0});
    auto       monitors = queryables({first, second});

    EXPECT_EQ(std::move(State::CMonitorQueryCore{monitors}).relativeTo(queryable(first)).configString("r").run(), queryable(second));
}

TEST(MonitorQueryCore, configRelativePositiveWraps) {
    const auto first    = testMonitor(0, "DP-1", {0, 0});
    const auto second   = testMonitor(1, "DP-2", {100, 0});
    const auto third    = testMonitor(2, "DP-3", {200, 0});
    auto       monitors = queryables({first, second, third});

    EXPECT_EQ(std::move(State::CMonitorQueryCore{monitors}).relativeTo(queryable(second)).configString("+1").run(), queryable(third));
    EXPECT_EQ(std::move(State::CMonitorQueryCore{monitors}).relativeTo(queryable(third)).configString("+1").run(), queryable(first));
}

TEST(MonitorQueryCore, configRelativeNegativeWraps) {
    const auto first    = testMonitor(0, "DP-1", {0, 0});
    const auto second   = testMonitor(1, "DP-2", {100, 0});
    const auto third    = testMonitor(2, "DP-3", {200, 0});
    auto       monitors = queryables({first, second, third});

    EXPECT_EQ(std::move(State::CMonitorQueryCore{monitors}).relativeTo(queryable(first)).configString("-1").run(), queryable(third));
}

TEST(MonitorQueryCore, configNumericId) {
    const auto first    = testMonitor(0, "DP-1", {0, 0});
    const auto second   = testMonitor(1, "DP-2", {100, 0});
    auto       monitors = queryables({first, second});

    EXPECT_EQ(std::move(State::CMonitorQueryCore{monitors}).configString("1").run(), queryable(second));
}

TEST(MonitorQueryCore, configSelectorSkipsNoOutput) {
    const auto withoutOutput = testMonitor(0, "DP-1", {0, 0});
    const auto withOutput    = testMonitor(1, "DP-1", {100, 0});
    auto       monitors      = queryables({withoutOutput, withOutput});

    withoutOutput->m_hasOutput = false;

    EXPECT_EQ(std::move(State::CMonitorQueryCore{monitors}).configString("DP-1").run(), queryable(withOutput));
}

TEST(MonitorQueryCore, selectorQueryUsesInterfaceMatcher) {
    const auto first    = testMonitor(0, "DP-1", {0, 0});
    const auto second   = testMonitor(1, "DP-2", {100, 0});
    auto       monitors = queryables({first, second});

    first->m_selectorResult  = false;
    second->m_selectorResult = true;

    EXPECT_EQ(std::move(State::CMonitorQueryCore{monitors}).selector("anything").run(), queryable(second));
}
