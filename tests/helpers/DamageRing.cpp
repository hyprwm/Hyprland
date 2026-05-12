#include <helpers/DamageRing.hpp>

#include <gtest/gtest.h>

// --- setSize ---

TEST(DamageRing, setSizeMarksDamageEntire) {
    CDamageRing ring;
    ring.setSize({100, 200});

    // After setSize the entire screen should be damaged
    EXPECT_TRUE(ring.hasChanged());
}

TEST(DamageRing, setSizeSameSizeNoChange) {
    CDamageRing ring;
    ring.setSize({100, 200});
    ring.rotate(); // clear current damage

    // Setting same size again must not re-damage
    ring.setSize({100, 200});
    EXPECT_FALSE(ring.hasChanged());
}

TEST(DamageRing, setSizeNewSizeReDamages) {
    CDamageRing ring;
    ring.setSize({100, 200});
    ring.rotate(); // clear current damage

    ring.setSize({300, 400});
    EXPECT_TRUE(ring.hasChanged());
}

TEST(DamageRing, setSizeZeroSize) {
    CDamageRing ring;
    ring.setSize({0, 0});

    // Zero-size screen: damageEntire covers a 0x0 box, which is empty
    EXPECT_FALSE(ring.hasChanged());
}

// --- damage ---

TEST(DamageRing, damageReturnsTrueForRegionInsideSize) {
    CDamageRing ring;
    ring.setSize({200, 200});
    ring.rotate(); // clear current damage

    CRegion rg(10, 10, 50, 50);
    EXPECT_TRUE(ring.damage(rg));
}

TEST(DamageRing, damageReturnsFalseForEmptyRegion) {
    CDamageRing ring;
    ring.setSize({200, 200});
    ring.rotate();

    CRegion empty;
    EXPECT_FALSE(ring.damage(empty));
}

TEST(DamageRing, damageReturnsFalseForRegionOutsideSize) {
    CDamageRing ring;
    ring.setSize({100, 100});
    ring.rotate();

    // Region entirely outside the screen bounds
    CRegion outside(200, 200, 50, 50);
    EXPECT_FALSE(ring.damage(outside));
}

TEST(DamageRing, damageClipsRegionToSize) {
    CDamageRing ring;
    ring.setSize({100, 100});
    ring.rotate();

    // Region that partially overlaps the screen
    CRegion partial(50, 50, 200, 200);
    EXPECT_TRUE(ring.damage(partial));
    EXPECT_TRUE(ring.hasChanged());
}

TEST(DamageRing, damageSetsHasChanged) {
    CDamageRing ring;
    ring.setSize({200, 200});
    ring.rotate();

    EXPECT_FALSE(ring.hasChanged());

    CRegion rg(0, 0, 10, 10);
    ring.damage(rg);
    EXPECT_TRUE(ring.hasChanged());
}

// --- damageEntire ---

TEST(DamageRing, damageEntireSetsHasChanged) {
    CDamageRing ring;
    ring.setSize({200, 200});
    ring.rotate();

    EXPECT_FALSE(ring.hasChanged());
    ring.damageEntire();
    EXPECT_TRUE(ring.hasChanged());
}

TEST(DamageRing, damageEntireOnZeroSizeDoesNotSetHasChanged) {
    CDamageRing ring;
    ring.setSize({0, 0});
    ring.rotate();

    ring.damageEntire();
    EXPECT_FALSE(ring.hasChanged());
}

// --- rotate ---

TEST(DamageRing, rotateClearsCurrentDamage) {
    CDamageRing ring;
    ring.setSize({200, 200});
    // setSize already damaged; rotate should clear it
    ring.rotate();
    EXPECT_FALSE(ring.hasChanged());
}

TEST(DamageRing, rotatePreservesCurrentAsPreviousForNextFrame) {
    CDamageRing ring;
    ring.setSize({200, 200});
    ring.rotate(); // clear initial damage

    CRegion rg(0, 0, 50, 50);
    ring.damage(rg);
    ring.rotate(); // the damage just applied becomes "previous[0]"

    // With age=2 we should see that previous damage accumulated
    CRegion bufDamage = ring.getBufferDamage(2);
    EXPECT_FALSE(bufDamage.empty());
}

TEST(DamageRing, rotateMultipleTimes) {
    CDamageRing ring;
    ring.setSize({200, 200});

    // Rotate more times than the ring length — must not crash
    for (int i = 0; i < DAMAGE_RING_PREVIOUS_LEN + 5; ++i) {
        ring.damageEntire();
        ring.rotate();
    }
    EXPECT_FALSE(ring.hasChanged());
}

// --- getBufferDamage ---

TEST(DamageRing, getBufferDamageAge0ReturnsFullScreen) {
    CDamageRing ring;
    ring.setSize({100, 100});
    ring.rotate();

    // age <= 0 returns full screen
    CRegion full = ring.getBufferDamage(0);
    EXPECT_FALSE(full.empty());
}

TEST(DamageRing, getBufferDamageNegativeAgeReturnsFullScreen) {
    CDamageRing ring;
    ring.setSize({100, 100});
    ring.rotate();

    CRegion full = ring.getBufferDamage(-1);
    EXPECT_FALSE(full.empty());
}

TEST(DamageRing, getBufferDamageExceedsRingLengthReturnsFullScreen) {
    CDamageRing ring;
    ring.setSize({100, 100});
    ring.rotate();

    // age > DAMAGE_RING_PREVIOUS_LEN + 1 returns full screen
    CRegion full = ring.getBufferDamage(DAMAGE_RING_PREVIOUS_LEN + 2);
    EXPECT_FALSE(full.empty());
}

TEST(DamageRing, getBufferDamageAge1ReturnCurrentOnly) {
    CDamageRing ring;
    ring.setSize({200, 200});
    ring.rotate();

    EXPECT_TRUE(ring.getBufferDamage(1).empty());

    CRegion rg(10, 10, 20, 20);
    ring.damage(rg);
    EXPECT_FALSE(ring.getBufferDamage(1).empty());
}

TEST(DamageRing, getBufferDamageAge2AccumulatesOnePreviousFrame) {
    CDamageRing ring;
    ring.setSize({200, 200});
    ring.rotate();

    // Frame N: damage a region, then rotate
    CRegion rg(0, 0, 30, 30);
    ring.damage(rg);
    ring.rotate();

    // Frame N+1: no new damage
    // age=2 should include previous frame's damage
    CRegion buf = ring.getBufferDamage(2);
    EXPECT_FALSE(buf.empty());
}

TEST(DamageRing, getBufferDamageAccumulatesUpToRingLength) {
    CDamageRing ring;
    ring.setSize({200, 200});
    ring.rotate();

    // Fill the ring with damage then check full accumulation
    for (int i = 0; i < DAMAGE_RING_PREVIOUS_LEN; ++i) {
        CRegion rg(i * 10, 0, 5, 5);
        ring.damage(rg);
        ring.rotate();
    }

    // age = DAMAGE_RING_PREVIOUS_LEN + 1 accumulates all stored frames
    CRegion buf = ring.getBufferDamage(DAMAGE_RING_PREVIOUS_LEN + 1);
    EXPECT_FALSE(buf.empty());
}

TEST(DamageRing, getBufferDamageCoalescesWhenTooManyRects) {
    CDamageRing ring;
    ring.setSize({500, 500});
    ring.rotate();

    // Add many non-overlapping rects across several frames so that
    // accumulation produces more than 8 rectangles, triggering the
    // getExtents() fallback path.
    int x = 0;
    for (int frame = 0; frame < DAMAGE_RING_PREVIOUS_LEN; ++frame) {
        for (int r = 0; r < 4; ++r) {
            ring.damage(CRegion(x, 0, 5, 5));
            x += 10;
        }
        ring.rotate();
    }

    // Current frame damage
    ring.damage(CRegion(x, 0, 5, 5));

    // age = DAMAGE_RING_PREVIOUS_LEN + 1 accumulates all frames
    CRegion buf = ring.getBufferDamage(DAMAGE_RING_PREVIOUS_LEN + 1);
    EXPECT_FALSE(buf.empty());

    // The result should be coalesced into a single extents rect (<=1 rect)
    EXPECT_LE(buf.getRects().size(), 1);
}

TEST(DamageRing, getBufferDamageEmptyRingReturnsEmptyForValidAge) {
    CDamageRing ring;
    ring.setSize({200, 200});

    // Rotate enough times to clear all previous slots
    for (int i = 0; i < DAMAGE_RING_PREVIOUS_LEN + 1; ++i)
        ring.rotate();

    // No damage in any slot
    CRegion buf = ring.getBufferDamage(1);
    EXPECT_TRUE(buf.empty());
}

// --- hasChanged ---

TEST(DamageRing, hasChangedFalseInitially) {
    CDamageRing ring;
    // No size set, no damage applied
    EXPECT_FALSE(ring.hasChanged());
}

TEST(DamageRing, hasChangedTrueAfterDamage) {
    CDamageRing ring;
    ring.setSize({100, 100});
    ring.rotate();

    CRegion rg(0, 0, 10, 10);
    ring.damage(rg);
    EXPECT_TRUE(ring.hasChanged());
}

TEST(DamageRing, hasChangedFalseAfterRotate) {
    CDamageRing ring;
    ring.setSize({100, 100});
    // setSize caused damage; rotate clears it
    ring.rotate();
    EXPECT_FALSE(ring.hasChanged());
}

TEST(DamageRing, hasChangedTrueAfterDamageEntire) {
    CDamageRing ring;
    ring.setSize({100, 100});
    ring.rotate();

    ring.damageEntire();
    EXPECT_TRUE(ring.hasChanged());
}

TEST(DamageRing, hasChangedFalseAfterDamageOutsideBounds) {
    CDamageRing ring;
    ring.setSize({100, 100});
    ring.rotate();

    // Damage entirely outside the screen must not change state
    CRegion outside(500, 500, 10, 10);
    ring.damage(outside);
    EXPECT_FALSE(ring.hasChanged());
}
