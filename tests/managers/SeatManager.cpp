#include <Compositor.hpp>
#include <managers/SeatManager.hpp>
#include <protocols/core/Seat.hpp>

#include <gtest/gtest.h>

// A touch on an output with no accepting surface (e.g. locked, no lock surface yet) used to
// dereference a null surface and abort the compositor.
TEST(SeatManagerTouch, MissingSurfaceIsIgnored) {
    auto previousCompositor = std::move(g_pCompositor);
    auto previousSeat       = std::move(PROTO::seat);
    g_pCompositor           = makeUnique<CCompositor>(true);
    auto* display           = wl_display_create();
    ASSERT_NE(display, nullptr);
    g_pCompositor->m_wlDisplay = display;
    PROTO::seat                = makeUnique<CWLSeatProtocol>(&wl_seat_interface, 9, "wl_seat");
    {
        CSeatManager seat;
        int          focusChanges = 0;
        auto         listener     = seat.m_events.touchFocusChange.listen([&] { ++focusChanges; });
        seat.sendTouchDown(nullptr, 1, 0, {0, 0});
        seat.sendTouchDown(nullptr, 2, 1, {10, 20});
        EXPECT_TRUE(seat.m_state.touchFocus.expired());
        EXPECT_TRUE(seat.m_state.touchFocusResource.expired());
        EXPECT_EQ(focusChanges, 0);
    }
    PROTO::seat.reset();
    wl_display_destroy(display);
    g_pCompositor->m_wlDisplay = nullptr;
    PROTO::seat                = std::move(previousSeat);
    g_pCompositor              = std::move(previousCompositor);
}
