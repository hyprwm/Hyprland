#include "XWaylandSelection.hpp"
#include "globals.hpp"

#include <src/xwayland/Dnd.hpp>
#include <hyprutils/utils/ScopeGuard.hpp>
#include <array>

// Use the real WM selections without adding a production test API.
#ifndef NO_XWAYLAND
#define private public
#include <src/xwayland/XWM.hpp>
#undef private
#endif
#include <src/xwayland/XWayland.hpp>

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

namespace XWaylandSelection {
    class CDataSourceSpy : public IDataSource {
      public:
        explicit CDataSourceSpy(eDataSourceType type) : m_type(type) {
            ;
        }

        std::vector<std::string> mimes() override {
            ++m_mimeCalls;
            // Stop before transfer/FD allocation even when the guard is missing.
            return {};
        }

        void send(const std::string&, Hyprutils::OS::CFileDescriptor) override {
            ++m_sendCalls;
        }

        void accepted(const std::string&) override {
            ;
        }

        void cancelled() override {
            ;
        }

        void error(uint32_t, const std::string&) override {
            ;
        }

        eDataSourceType type() override {
            return m_type;
        }

        int m_mimeCalls = 0;
        int m_sendCalls = 0;

      private:
        eDataSourceType m_type = DATA_SOURCE_TYPE_WAYLAND;
    };

    static std::string runTest(std::string_view name, bool x11) {
#ifdef NO_XWAYLAND
        return "XWayland selection test requires XWayland support";
#else
        if (!g_pXWayland || !g_pXWayland->enabled() || !g_pXWayland->m_wm || !g_pSeatManager)
            return "XWayland selection test requires a ready XWM and seat";
        if (name != "clipboard" && name != "primary" && name != "dnd")
            return "Expected clipboard, primary or dnd";

        auto& wm   = *g_pXWayland->m_wm;
        auto& seat = g_pSeatManager->m_selection;
        if (!wm.m_clipboard.window || !wm.m_primarySelection.window || !wm.m_dndSelection.window)
            return "XWayland selection windows are not ready";

        const auto                    SOURCE        = makeShared<CDataSourceSpy>(x11 ? DATA_SOURCE_TYPE_X11 : DATA_SOURCE_TYPE_WAYLAND);
        const auto                    OLD_CLIPBOARD = seat.currentSelection;
        const auto                    OLD_PRIMARY   = seat.currentPrimarySelection;
        auto                          oldOffers     = wm.m_dndDataOffers;
        Hyprutils::Utils::CScopeGuard restore([&] {
            seat.currentSelection        = OLD_CLIPBOARD;
            seat.currentPrimarySelection = OLD_PRIMARY;
            wm.m_dndDataOffers           = std::move(oldOffers);
        });

        // Synchronous slot substitution: do not emit selection-change signals or change X ownership.
        seat.currentSelection        = nullptr;
        seat.currentPrimarySelection = nullptr;
        wm.m_dndDataOffers.clear();
        if (name == "clipboard")
            seat.currentSelection = SOURCE;
        else if (name == "primary")
            seat.currentPrimarySelection = SOURCE;
        else {
            auto offer      = makeShared<CX11DataOffer>();
            offer->m_source = SOURCE;
            wm.m_dndDataOffers.emplace_back(offer);
        }

        auto&      selection = name == "clipboard" ? wm.m_clipboard : name == "primary" ? wm.m_primarySelection : wm.m_dndSelection;
        const auto COUNTS    = [&] {
            return std::array{
                wm.m_clipboard.transfers.size(),
                wm.m_primarySelection.transfers.size(),
                wm.m_dndSelection.transfers.size(),
            };
        };
        const auto                    BEFORE  = COUNTS();
        xcb_selection_request_event_t request = {};
        const bool                    SENT    = selection.sendData(&request, "text/plain");
        if (SENT || SOURCE->m_sendCalls != 0 || COUNTS() != BEFORE)
            return "sendData must return false without sending or changing transfer counts";

        const int EXPECTED_MIME_CALLS = x11 && name != "dnd" ? 0 : 1;
        if (SOURCE->m_mimeCalls != EXPECTED_MIME_CALLS)
            return std::format("{} {}: expected {} mimes() calls, got {}", x11 ? "X11" : "Wayland", name, EXPECTED_MIME_CALLS, SOURCE->m_mimeCalls);
        return {};
#endif
    }

    static int luaExpectSelectionGuard(lua_State* L) {
        const auto NAME = luaL_checkstring(L, 1);
        luaL_checktype(L, 2, LUA_TBOOLEAN);
        bool failed = false;
        {
            const auto ERROR = runTest(NAME, lua_toboolean(L, 2));
            failed           = !ERROR.empty();
            if (failed)
                lua_pushstring(L, ERROR.c_str());
        }
        // All C++ state, including the restoration guard, is gone before Lua can longjmp.
        return failed ? lua_error(L) : 0;
    }

    void registerFunctions() {
        if (!HyprlandAPI::addLuaFunction(PHANDLE, "test", "expect_xwayland_selection_guard", luaExpectSelectionGuard))
            LOG(Log::ERR, "hyprtester plugin: failed to register hl.plugin.test.expect_xwayland_selection_guard");
    }
}
