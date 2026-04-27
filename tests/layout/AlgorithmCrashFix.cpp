#include <layout/algorithm/Algorithm.hpp>
#include <layout/algorithm/TiledAlgorithm.hpp>
#include <layout/algorithm/FloatingAlgorithm.hpp>

#include <gtest/gtest.h>
#include <memory>

using namespace Layout;

// Mock algorithm that can simulate various failure modes
class MockFailingTiled : public ITiledAlgorithm {
  public:
    enum FailureMode {
        NONE,
        BAD_VARIANT,
        THROW_RUNTIME,
        SEGFAULT_SIMULATION
    };
    
    FailureMode failureMode = NONE;
    int recalculateCalls = 0;
    
    void recalculate() override {
        recalculateCalls++;
        
        switch (failureMode) {
            case BAD_VARIANT:
                throw std::bad_variant_access();
            case THROW_RUNTIME:
                throw std::runtime_error("Simulated error");
            case SEGFAULT_SIMULATION:
                // Don't actually segfault in test
                throw std::runtime_error("Would segfault here");
            case NONE:
            default:
                break;
        }
    }
    
    void newTarget(SP<ITarget> target) override {}
    void movedTarget(SP<ITarget> target, std::optional<Vector2D> focalPoint) override {}
    void removeTarget(SP<ITarget> target) override {}
    void resizeTarget(const Vector2D& delta, SP<ITarget> target, eRectCorner corner) override {}
    void swapTargets(SP<ITarget> a, SP<ITarget> b) override {}
    void moveTargetInDirection(SP<ITarget> t, Math::eDirection dir, bool silent) override {}
    SP<ITarget> getNextCandidate(SP<ITarget> old) override { return nullptr; }
};

// Mock floating algorithm
class MockFailingFloating : public IFloatingAlgorithm {
  public:
    bool shouldThrow = false;
    
    void recalculate() override {
        if (shouldThrow) {
            throw std::bad_variant_access();
        }
    }
    
    void newTarget(SP<ITarget> target) override {}
    void movedTarget(SP<ITarget> target, std::optional<Vector2D> focalPoint) override {}
    void removeTarget(SP<ITarget> target) override {}
    void resizeTarget(const Vector2D& delta, SP<ITarget> target, eRectCorner corner) override {}
    void swapTargets(SP<ITarget> a, SP<ITarget> b) override {}
    void moveTargetInDirection(SP<ITarget> t, Math::eDirection dir, bool silent) override {}
    void moveTarget(const Vector2D& delta, SP<ITarget> target) override {}
    void setTargetGeom(const CBox& geom, SP<ITarget> target) override {}
};

// Proposed safe wrapper that implements the fix
class SafeAlgorithmWrapper {
  public:
    std::unique_ptr<ITiledAlgorithm> m_tiled;
    std::unique_ptr<IFloatingAlgorithm> m_floating;
    bool hasValidWorkspace = true;
    bool hasValidMonitor = true;
    
    void recalculate() {
        // FIX 1: Null checks
        if (!m_tiled || !m_floating) {
            return; // Log error in production
        }
        
        // FIX 2: Check workspace/monitor state first
        if (!hasValidWorkspace) {
            return; // Log warning in production
        }
        
        if (!hasValidMonitor) {
            // This is the condition from the crash
            return; // Log warning about fullscreen transition
        }
        
        // FIX 3: Exception handling
        try {
            m_tiled->recalculate();
        } catch (const std::bad_variant_access& e) {
            // Log error and continue
            // Don't crash the entire compositor
        } catch (const std::exception& e) {
            // Handle other exceptions gracefully
        }
        
        try {
            m_floating->recalculate();
        } catch (const std::bad_variant_access& e) {
            // Log error and continue
        } catch (const std::exception& e) {
            // Handle other exceptions gracefully
        }
    }
};

TEST(LayoutFix, HandlesNullAlgorithms) {
    SafeAlgorithmWrapper wrapper;
    // Both algorithms are null
    
    // Should not crash
    EXPECT_NO_THROW(wrapper.recalculate());
}

TEST(LayoutFix, HandlesBadVariantAccess) {
    SafeAlgorithmWrapper wrapper;
    auto tiled = std::make_unique<MockFailingTiled>();
    auto floating = std::make_unique<MockFailingFloating>();
    
    tiled->failureMode = MockFailingTiled::BAD_VARIANT;
    
    wrapper.m_tiled = std::move(tiled);
    wrapper.m_floating = std::move(floating);
    
    // Should catch and handle the exception
    EXPECT_NO_THROW(wrapper.recalculate());
}

TEST(LayoutFix, HandlesNoMonitorCondition) {
    SafeAlgorithmWrapper wrapper;
    auto tiled = std::make_unique<MockFailingTiled>();
    auto floating = std::make_unique<MockFailingFloating>();
    
    auto* tiledPtr = tiled.get();
    
    wrapper.m_tiled = std::move(tiled);
    wrapper.m_floating = std::move(floating);
    wrapper.hasValidMonitor = false; // Workspace lost monitor
    
    // Should not call recalculate when monitor is invalid
    wrapper.recalculate();
    EXPECT_EQ(tiledPtr->recalculateCalls, 0);
}

TEST(LayoutFix, HandlesNoWorkspaceCondition) {
    SafeAlgorithmWrapper wrapper;
    auto tiled = std::make_unique<MockFailingTiled>();
    auto floating = std::make_unique<MockFailingFloating>();
    
    auto* tiledPtr = tiled.get();
    
    wrapper.m_tiled = std::move(tiled);
    wrapper.m_floating = std::move(floating);
    wrapper.hasValidWorkspace = false;
    
    // Should not call recalculate when workspace is invalid
    wrapper.recalculate();
    EXPECT_EQ(tiledPtr->recalculateCalls, 0);
}

TEST(LayoutFix, WorksNormallyWithValidState) {
    SafeAlgorithmWrapper wrapper;
    auto tiled = std::make_unique<MockFailingTiled>();
    auto floating = std::make_unique<MockFailingFloating>();
    
    auto* tiledPtr = tiled.get();
    
    wrapper.m_tiled = std::move(tiled);
    wrapper.m_floating = std::move(floating);
    wrapper.hasValidWorkspace = true;
    wrapper.hasValidMonitor = true;
    
    // Should call recalculate normally
    wrapper.recalculate();
    EXPECT_EQ(tiledPtr->recalculateCalls, 1);
}

TEST(LayoutFix, HandlesMultipleExceptionTypes) {
    SafeAlgorithmWrapper wrapper;
    auto tiled = std::make_unique<MockFailingTiled>();
    auto floating = std::make_unique<MockFailingFloating>();
    
    // Test various exception types
    tiled->failureMode = MockFailingTiled::THROW_RUNTIME;
    wrapper.m_tiled = std::make_unique<MockFailingTiled>(*tiled);
    wrapper.m_floating = std::make_unique<MockFailingFloating>();
    EXPECT_NO_THROW(wrapper.recalculate());
    
    // Both algorithms throwing
    tiled->failureMode = MockFailingTiled::BAD_VARIANT;
    floating->shouldThrow = true;
    wrapper.m_tiled = std::make_unique<MockFailingTiled>(*tiled);
    wrapper.m_floating = std::make_unique<MockFailingFloating>(*floating);
    EXPECT_NO_THROW(wrapper.recalculate());
}