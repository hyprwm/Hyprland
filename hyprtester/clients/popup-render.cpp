#include <algorithm>
#include <array>
#include <string>
#include <sys/mman.h>
#include <unistd.h>

#include <wayland-client.h>
#include <wayland.hpp>
#include <xdg-shell.hpp>

#include <hyprutils/memory/Casts.hpp>
#include <hyprutils/memory/SharedPtr.hpp>
#include <hyprutils/os/FileDescriptor.hpp>
#include <hyprutils/utils/ScopeGuard.hpp>

using namespace Hyprutils::Memory;
using namespace Hyprutils::OS;
using namespace Hyprutils::Utils;

struct SSurface {
    CSharedPointer<CCWlSurface>  surface;
    CSharedPointer<CCXdgSurface> xdg;
    CSharedPointer<CCWlBuffer>   buffer;
    CSharedPointer<CCXdgPopup>   popup;
};

struct SState {
    CSharedPointer<CCWlRegistry>   registry;
    CSharedPointer<CCWlCompositor> compositor;
    CSharedPointer<CCWlShm>        shm;
    CSharedPointer<CCXdgWmBase>    wm;
    CSharedPointer<CCWlShmPool>    pool;
    CSharedPointer<CCXdgToplevel>  toplevel;
    SSurface                       parent;
    std::array<SSurface, 2>        popups;
    bool                           mapped = false;
    bool                           done   = false;
};

static void createSurface(SState& state, SSurface& surface, int width, int height, int offset) {
    surface.surface = makeShared<CCWlSurface>(state.compositor->sendCreateSurface());
    surface.xdg     = makeShared<CCXdgSurface>(state.wm->sendGetXdgSurface(surface.surface->resource()));
    surface.buffer  = makeShared<CCWlBuffer>(state.pool->sendCreateBuffer(offset, width, height, width * 4, WL_SHM_FORMAT_XRGB8888));
    surface.xdg->setConfigure([&surface, width, height](CCXdgSurface*, uint32_t serial) {
        surface.xdg->sendAckConfigure(serial);
        surface.xdg->sendSetWindowGeometry(0, 0, width, height);
        surface.surface->sendAttach(surface.buffer.get(), 0, 0);
        surface.surface->sendDamage(0, 0, width, height);
        surface.surface->sendCommit();
    });
}

static void createPopups(SState& state) {
    for (size_t i = 0; i < state.popups.size(); ++i) {
        auto&     popup = state.popups[i];
        const int WIDTH = i == 0 ? 64 : 96;
        createSurface(state, popup, WIDTH, 48, 320 * 240 * 4 + (i == 0 ? 0 : 64 * 48 * 4));
        const auto POSITIONER = makeShared<CCXdgPositioner>(state.wm->sendCreatePositioner());
        POSITIONER->sendSetSize(WIDTH, 48);
        POSITIONER->sendSetAnchorRect(20 + sc<int>(i) * 120, 20, 1, 1);
        POSITIONER->sendSetAnchor(XDG_POSITIONER_ANCHOR_BOTTOM_RIGHT);
        POSITIONER->sendSetGravity(XDG_POSITIONER_GRAVITY_BOTTOM_RIGHT);
        // Both popups belong directly to the toplevel, with no grab or nesting.
        popup.popup = makeShared<CCXdgPopup>(popup.xdg->sendGetPopup(state.parent.xdg.get(), POSITIONER.get()));
        popup.popup->setConfigure([](CCXdgPopup*, int32_t, int32_t, int32_t, int32_t) { ; });
        popup.popup->setPopupDone([&state](CCXdgPopup*) { state.done = true; });
        popup.surface->sendCommit();
        POSITIONER->sendDestroy();
    }
}

int main() {
    const auto DISPLAY = wl_display_connect(nullptr);
    if (!DISPLAY)
        return 1;
    CScopeGuard disconnect([DISPLAY] { wl_display_disconnect(DISPLAY); });
    SState      state;

    state.registry = makeShared<CCWlRegistry>(rc<wl_proxy*>(wl_display_get_registry(DISPLAY)));
    state.registry->setGlobal([&state](CCWlRegistry* registry, uint32_t id, const char* interface, uint32_t version) {
        const std::string INTERFACE = interface;
        if (INTERFACE == "wl_compositor")
            state.compositor =
                makeShared<CCWlCompositor>(rc<wl_proxy*>(wl_registry_bind(rc<wl_registry*>(registry->resource()), id, &wl_compositor_interface, std::min(version, 6U))));
        else if (INTERFACE == "wl_shm")
            state.shm = makeShared<CCWlShm>(rc<wl_proxy*>(wl_registry_bind(rc<wl_registry*>(registry->resource()), id, &wl_shm_interface, 1)));
        else if (INTERFACE == "xdg_wm_base")
            state.wm = makeShared<CCXdgWmBase>(rc<wl_proxy*>(wl_registry_bind(rc<wl_registry*>(registry->resource()), id, &xdg_wm_base_interface, 1)));
    });
    if (wl_display_roundtrip(DISPLAY) < 0 || !state.compositor || !state.shm || !state.wm)
        return 1;
    state.wm->setPing([&state](CCXdgWmBase*, uint32_t serial) { state.wm->sendPong(serial); });

    // Immutable, zero-filled XRGB buffers suffice: the test inspects render pass data.
    constexpr int         BYTES = (320 * 240 + 64 * 48 + 96 * 48) * 4;
    const CFileDescriptor FD{memfd_create("popup-render", MFD_CLOEXEC)};
    if (FD.get() < 0 || ftruncate(FD.get(), BYTES) < 0)
        return 1;
    state.pool = makeShared<CCWlShmPool>(state.shm->sendCreatePool(FD.get(), BYTES));
    createSurface(state, state.parent, 320, 240, 0);
    state.toplevel = makeShared<CCXdgToplevel>(state.parent.xdg->sendGetToplevel());
    state.toplevel->sendSetAppId("popup-render");
    state.toplevel->sendSetTitle("Popup render test client");
    state.toplevel->setConfigure([&state](CCXdgToplevel*, int32_t, int32_t, wl_array*) { state.mapped = true; });
    state.toplevel->setClose([&state](CCXdgToplevel*) { state.done = true; });
    state.parent.surface->sendCommit();
    while (!state.mapped && !state.done) {
        if (wl_display_roundtrip(DISPLAY) < 0)
            return 1;
    }
    // Ensure the parent buffer commit has reached the server before adding siblings.
    if (wl_display_roundtrip(DISPLAY) < 0 || state.done)
        return 1;
    createPopups(state);
    while (!state.done && wl_display_dispatch(DISPLAY) != -1) {
        ;
    }
    return state.done ? 0 : 1;
}
