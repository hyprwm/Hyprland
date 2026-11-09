#include <layout/algorithm/Algorithm.hpp>
#include <layout/algorithm/floating/default/DefaultFloatingAlgorithm.hpp>
#include <layout/algorithm/tiled/dwindle/DwindleAlgorithm.hpp>
#include <layout/space/Space.hpp>
#include <layout/target/Target.hpp>
#include <managers/fullscreen/handler/FullscreenHandler.hpp>

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

class CGeometryFullscreenHandler : public Fullscreen::IFullscreenHandler {
  public:
    CGeometryFullscreenHandler(IModeAlgorithm* algorithm) : IFullscreenHandler(algorithm) {
        ;
    }
    bool isFullscreen(SP<ITarget>, std::optional<Fullscreen::eFullscreenMode>, std::optional<bool>) override {
        return m_mode != Fullscreen::FSMODE_NONE;
    }
    Fullscreen::eFullscreenMode m_mode = Fullscreen::FSMODE_NONE;
};

class CGeometryFloatingAlgorithm : public Floating::CDefaultFloatingAlgorithm {
  public:
    WP<Fullscreen::IFullscreenHandler> getFSHandler() override {
        return m_handler;
    }
    void setMode(Fullscreen::eFullscreenMode mode) {
        m_handler->m_mode = mode;
    }

  private:
    UP<CGeometryFullscreenHandler> m_handler = makeUnique<CGeometryFullscreenHandler>(this);
};

class FloatingGeometry : public ::testing::Test {
  protected:
    FloatingGeometry() {
        m_space->setAlgorithmProvider(m_algorithm);
    }

    CGeometryFloatingAlgorithm& floating() {
        return dynamic_cast<CGeometryFloatingAlgorithm&>(*m_algorithm->floatingAlgo());
    }

    SP<CSpace>          m_space     = CSpace::create(nullptr);
    SP<CAlgorithm>      m_algorithm = CAlgorithm::create(makeUnique<Tiled::CDwindleAlgorithm>(), makeUnique<CGeometryFloatingAlgorithm>(), m_space);
    SP<CGeometryTarget> m_target    = CGeometryTarget::create(m_space);
};

TEST_F(FloatingGeometry, DirectClientGeometryChangesUpdateTheRestoreBox) {
    m_target->setPositionGlobal(CBox{150, 160, 600, 400});
    floating().updateTarget(m_target); // Seed the cache as newTarget does.

    // X11 configure requests and Wayland size clamping use this shared path, not setTargetGeom.
    const CBox CLIENT_BOX{170, 180, 800, 500};
    m_target->setPositionGlobal(CLIENT_BOX, TARGET_UPDATE_NO_CLIENT_CONFIGURE);

    for (const auto MODE : {Fullscreen::FSMODE_FULLSCREEN, Fullscreen::FSMODE_MAXIMIZED}) {
        floating().setMode(MODE);
        m_target->setPositionGlobal(MODE == Fullscreen::FSMODE_FULLSCREEN ? CBox{0, 0, 1920, 1080} : CBox{20, 20, 1880, 1040});

        floating().setMode(Fullscreen::FSMODE_NONE);
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

TEST_F(FloatingGeometry, ExplicitCacheUpdatesRemainAvailableDuringFullscreen) {
    const CBox TRANSFERRED_BOX{170, 180, 800, 500};
    m_target->setPositionGlobal(CBox{150, 160, 600, 400});
    floating().setMode(Fullscreen::FSMODE_FULLSCREEN);
    m_target->setPositionGlobal(TRANSFERRED_BOX);

    // Initialization and transfers still explicitly seed the owning algorithm's cache.
    floating().updateTarget(m_target);
    m_target->setPositionGlobal(CBox{0, 0, 1920, 1080});
    floating().setMode(Fullscreen::FSMODE_NONE);
    floating().recenter(m_target);
    EXPECT_EQ(m_target->position(), TRANSFERRED_BOX);
}

TEST_F(FloatingGeometry, GeometryChangesNotifyTheCurrentSpace) {
    const CBox ORIGINAL_BOX{150, 160, 600, 400};
    const CBox TRANSFERRED_BOX{170, 180, 800, 500};
    m_target->setPositionGlobal(ORIGINAL_BOX);

    const auto OTHER_SPACE     = CSpace::create(nullptr);
    const auto OTHER_ALGORITHM = CAlgorithm::create(makeUnique<Tiled::CDwindleAlgorithm>(), makeUnique<CGeometryFloatingAlgorithm>(), OTHER_SPACE);
    OTHER_SPACE->setAlgorithmProvider(OTHER_ALGORITHM);
    auto& otherFloating = dynamic_cast<CGeometryFloatingAlgorithm&>(*OTHER_ALGORITHM->floatingAlgo());

    m_target->setSpace(OTHER_SPACE);
    m_target->setPositionGlobal(TRANSFERRED_BOX);
    otherFloating.setMode(Fullscreen::FSMODE_MAXIMIZED);
    m_target->setPositionGlobal(CBox{20, 20, 1880, 1040});
    otherFloating.setMode(Fullscreen::FSMODE_NONE);
    otherFloating.recenter(m_target);
    EXPECT_EQ(m_target->position(), TRANSFERRED_BOX);

    m_target->setSpace(m_space);
    floating().recenter(m_target);
    EXPECT_EQ(m_target->position(), ORIGINAL_BOX);
}
