#include <layout/algorithm/Algorithm.hpp>
#include <layout/algorithm/floating/default/DefaultFloatingAlgorithm.hpp>
#include <layout/algorithm/tiled/dwindle/DwindleAlgorithm.hpp>
#include <layout/space/Space.hpp>
#include <layout/target/Target.hpp>

#include <gtest/gtest.h>

using namespace Layout;

class CGeometryTarget : public ITarget {
  public:
    static SP<CGeometryTarget> create(SP<CSpace> space) {
        auto target     = makeShared<CGeometryTarget>();
        target->m_self  = target;
        target->m_space = space;
        return target;
    }

    eTargetType type() override {
        return TARGET_TYPE_WINDOW;
    }
    PHLWINDOW window() const override {
        return nullptr;
    }
    bool floating() override {
        return m_floating;
    }
    void setFloating(bool value) override {
        m_floating = value;
    }
    std::expected<SGeometryRequested, eGeometryFailure> desiredGeometry() override {
        return std::unexpected(GEOMETRY_NO_DESIRED);
    }
    std::optional<Vector2D> minSize() override {
        return std::nullopt;
    }
    std::optional<Vector2D> maxSize() override {
        return std::nullopt;
    }
    void damageEntire() override {
        ;
    }
    void warpPositionSize() override {
        ;
    }
    void onUpdateSpace() override {
        ;
    }
    void setGhost(bool value) {
        m_ghostSpace = value;
    }
    void setSpace(SP<CSpace> space) {
        m_space = space;
    }

  private:
    bool m_floating = true;
};

class FloatingGeometry : public ::testing::Test {
  protected:
    FloatingGeometry() {
        m_space->setAlgorithmProvider(m_algorithm);
    }

    Floating::CDefaultFloatingAlgorithm& floating() {
        return dynamic_cast<Floating::CDefaultFloatingAlgorithm&>(*m_algorithm->floatingAlgo());
    }

    SP<CSpace>          m_space     = CSpace::create(nullptr);
    SP<CAlgorithm>      m_algorithm = CAlgorithm::create(makeUnique<Tiled::CDwindleAlgorithm>(), makeUnique<Floating::CDefaultFloatingAlgorithm>(), m_space);
    SP<CGeometryTarget> m_target    = CGeometryTarget::create(m_space);
};

TEST_F(FloatingGeometry, DirectClientGeometryChangesUpdateTheRestoreBox) {
    m_target->setPositionGlobal(CBox{150, 160, 600, 400});
    floating().updateTarget(m_target); // Seed the cache as newTarget does.

    // X11 configure requests and Wayland size clamping use this shared path, not setTargetGeom.
    const CBox CLIENT_BOX{170, 180, 800, 500};
    m_target->setPositionGlobal(CLIENT_BOX, TARGET_UPDATE_NO_CLIENT_CONFIGURE);

    // Simulate the explicitly suppressed geometry writes made by fullscreen/maximize handlers.
    for (const auto& FULLSCREEN_BOX : {CBox{0, 0, 1920, 1080}, CBox{20, 20, 1880, 1040}}) {
        m_target->setPositionGlobal(FULLSCREEN_BOX, TARGET_UPDATE_NO_UPDATE_RESTORE_CACHE);

        // Fullscreen exit can recalculate the existing target before restoring it.
        m_target->recalc();
        floating().recenter(m_target);
        EXPECT_EQ(m_target->position(), CLIENT_BOX);
    }
}

TEST_F(FloatingGeometry, TiledAndGhostChangesDoNotOverwriteTheRestoreBox) {
    const CBox FLOATING_BOX{150, 160, 600, 400};
    m_target->setPositionGlobal(FLOATING_BOX);

    m_target->setFloating(false);
    m_target->setPositionGlobal(CBox{10, 20, 300, 200});
    m_target->setFloating(true);
    floating().recenter(m_target);
    EXPECT_EQ(m_target->position(), FLOATING_BOX);

    m_target->setGhost(true);
    m_target->setPositionGlobal(CBox{20, 30, 400, 300});
    m_target->setGhost(false);
    floating().recenter(m_target);
    EXPECT_EQ(m_target->position(), FLOATING_BOX);
}

TEST_F(FloatingGeometry, ExplicitCacheUpdatesRemainAvailableWhenAutomaticUpdatesAreSuppressed) {
    const CBox TRANSFERRED_BOX{170, 180, 800, 500};
    m_target->setPositionGlobal(CBox{150, 160, 600, 400});
    m_target->setPositionGlobal(TRANSFERRED_BOX, TARGET_UPDATE_NO_UPDATE_RESTORE_CACHE | TARGET_UPDATE_NO_CLIENT_CONFIGURE);

    // Initialization and transfers still explicitly seed the owning algorithm's cache.
    floating().updateTarget(m_target);
    m_target->setPositionGlobal(CBox{0, 0, 1920, 1080}, TARGET_UPDATE_NO_UPDATE_RESTORE_CACHE);
    floating().recenter(m_target);
    EXPECT_EQ(m_target->position(), TRANSFERRED_BOX);
}

TEST_F(FloatingGeometry, GeometryChangesNotifyTheCurrentSpace) {
    const CBox ORIGINAL_BOX{150, 160, 600, 400};
    const CBox TRANSFERRED_BOX{170, 180, 800, 500};
    m_target->setPositionGlobal(ORIGINAL_BOX);

    const auto OTHER_SPACE     = CSpace::create(nullptr);
    const auto OTHER_ALGORITHM = CAlgorithm::create(makeUnique<Tiled::CDwindleAlgorithm>(), makeUnique<Floating::CDefaultFloatingAlgorithm>(), OTHER_SPACE);
    OTHER_SPACE->setAlgorithmProvider(OTHER_ALGORITHM);
    auto& otherFloating = dynamic_cast<Floating::CDefaultFloatingAlgorithm&>(*OTHER_ALGORITHM->floatingAlgo());

    m_target->setSpace(OTHER_SPACE);
    m_target->setPositionGlobal(TRANSFERRED_BOX);
    m_target->setPositionGlobal(CBox{20, 20, 1880, 1040}, TARGET_UPDATE_NO_UPDATE_RESTORE_CACHE);
    otherFloating.recenter(m_target);
    EXPECT_EQ(m_target->position(), TRANSFERRED_BOX);

    m_target->setSpace(m_space);
    floating().recenter(m_target);
    EXPECT_EQ(m_target->position(), ORIGINAL_BOX);
}
