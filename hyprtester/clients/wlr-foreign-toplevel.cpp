#include <algorithm>
#include <print>
#include <string>
#include <string_view>
#include <vector>

#include <wayland-client.h>
#include <wayland.hpp>
#include <wlr-foreign-toplevel-management-unstable-v1.hpp>

#include <hyprutils/memory/Casts.hpp>
#include <hyprutils/memory/SharedPtr.hpp>

using namespace Hyprutils::Memory;

struct SHandle {
    CSharedPointer<CCZwlrForeignToplevelHandleV1> resource;
    std::string                                   appID;
};

struct SState {
    wl_display*                                    display = nullptr;
    CSharedPointer<CCWlRegistry>                   registry;
    CSharedPointer<CCZwlrForeignToplevelManagerV1> manager;
    std::vector<CSharedPointer<SHandle>>           handles;
    CSharedPointer<CCZwlrForeignToplevelHandleV1>  target;
    std::string                                    targetAppID;
};

static void disconnect(SState& state) {
    auto* display = state.display;
    state         = {};
    if (display)
        wl_display_disconnect(display);
}

static bool bindManager(SState& state) {
    state.registry = makeShared<CCWlRegistry>(rc<wl_proxy*>(wl_display_get_registry(state.display)));
    state.registry->setGlobal([&state](CCWlRegistry* registry, uint32_t name, const char* interface, uint32_t version) {
        if (std::string_view{interface} != "zwlr_foreign_toplevel_manager_v1")
            return;

        state.manager = makeShared<CCZwlrForeignToplevelManagerV1>(
            rc<wl_proxy*>(wl_registry_bind(rc<wl_registry*>(registry->resource()), name, &zwlr_foreign_toplevel_manager_v1_interface, std::min(version, 3U))));
        state.manager->setToplevel([&state](CCZwlrForeignToplevelManagerV1*, wl_proxy* resource) {
            const auto HANDLE = makeShared<SHandle>();
            HANDLE->resource  = makeShared<CCZwlrForeignToplevelHandleV1>(resource);
            auto* handle      = HANDLE.get();

            HANDLE->resource->setAppId([handle](CCZwlrForeignToplevelHandleV1*, const char* appID) { handle->appID = appID; });
            HANDLE->resource->setDone([&state, handle](CCZwlrForeignToplevelHandleV1*) {
                if (handle->appID == state.targetAppID)
                    state.target = handle->resource;
            });
            HANDLE->resource->setClosed([&state, handle](CCZwlrForeignToplevelHandleV1*) {
                if (state.target == handle->resource)
                    state.target.reset();
            });
            state.handles.emplace_back(HANDLE);
        });
        state.manager->setFinished([](CCZwlrForeignToplevelManagerV1*) {});
    });
    state.registry->setGlobalRemove([](CCWlRegistry*, uint32_t) {});

    if (wl_display_roundtrip(state.display) < 0)
        return false;

    return !!state.manager;
}

static bool findTarget(SState& state) {
    for (size_t i = 0; i < 3 && !state.target; ++i) {
        if (wl_display_roundtrip(state.display) < 0)
            return false;
    }

    return !!state.target;
}

int main(int argc, char** argv) {
    if (argc != 3) {
        std::println(stderr, "usage: wlr-foreign-toplevel APP_ID minimize|restore");
        return 1;
    }

    const std::string_view ACTION = argv[2];
    if (ACTION != "minimize" && ACTION != "restore") {
        std::println(stderr, "unknown action: {}", ACTION);
        return 1;
    }

    SState state{
        .targetAppID = argv[1],
    };
    state.display = wl_display_connect(nullptr);
    if (!state.display) {
        std::println(stderr, "failed to connect to Wayland display");
        return 1;
    }

    if (!bindManager(state)) {
        std::println(stderr, "foreign toplevel manager is unavailable");
        disconnect(state);
        return 1;
    }

    if (!findTarget(state)) {
        std::println(stderr, "no foreign toplevel found for app ID {}", state.targetAppID);
        disconnect(state);
        return 1;
    }

    if (ACTION == "minimize")
        state.target->sendSetMinimized();
    else
        state.target->sendUnsetMinimized();

    const bool ROUNDTRIP_OK = wl_display_roundtrip(state.display) >= 0;
    disconnect(state);

    if (!ROUNDTRIP_OK) {
        std::println(stderr, "failed to synchronize minimize request");
        return 1;
    }

    return 0;
}
