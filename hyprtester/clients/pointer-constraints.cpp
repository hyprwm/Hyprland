#include <format>
#include <iostream>
#include <sstream>
#include <string>
#include <sys/mman.h>
#include <unistd.h>

#include <wayland-client.h>
#include <wayland.hpp>
#include <xdg-shell.hpp>
#include <pointer-constraints-unstable-v1.hpp>
#include <hyprutils/memory/SharedPtr.hpp>
#include <hyprutils/os/FileDescriptor.hpp>

using namespace Hyprutils::Memory;
using namespace Hyprutils::OS;

struct SWlState {
    wl_display*                               display = nullptr;
    CSharedPointer<CCWlRegistry>              registry;
    CSharedPointer<CCWlCompositor>            compositor;
    CSharedPointer<CCWlSubcompositor>         subcompositor;
    CSharedPointer<CCWlShm>                   shm;
    CSharedPointer<CCWlSeat>                  seat;
    CSharedPointer<CCXdgWmBase>               shell;
    CSharedPointer<CCZwpPointerConstraintsV1> constraints;
    CSharedPointer<CCWlSurface>               root, child;
    CSharedPointer<CCWlSubsurface>            subsurface;
    CSharedPointer<CCXdgSurface>              xdgSurface;
    CSharedPointer<CCXdgToplevel>             toplevel;
    CSharedPointer<CCWlBuffer>                rootBuffer, childBuffer;
    CSharedPointer<CCWlPointer>               pointer;
    CSharedPointer<CCWlKeyboard>              keyboard;
    CSharedPointer<CCZwpLockedPointerV1>      lock;
    CSharedPointer<CCZwpConfinedPointerV1>    confine;
    bool                                      configured = false;
    bool                                      active     = false;
    int                                       locked = 0, unlocked = 0, confined = 0, unconfined = 0;
    int                                       rootEnters = 0, childEnters = 0, keyboardEnters = 0, keyboardLeaves = 0;
    int                                       motions = 0, lockedMotions = 0, confinedMotions = 0, outsideMotions = 0, badActivations = 0;
    int                                       motionX = 0, motionY = 0;
    std::string                               pointerTarget = "none", keyboardTarget = "none";
    std::string                               constraintTarget;
};

static bool sync(SWlState& state) {
    return wl_display_roundtrip(state.display) >= 0;
}

static CSharedPointer<CCWlBuffer> createBuffer(SWlState& state, int width, int height) {
    const int       size = width * height * 4;
    CFileDescriptor fd(memfd_create("hyprtester-pointer-constraints", MFD_CLOEXEC));
    if (fd.get() < 0 || ftruncate(fd.get(), size) < 0)
        return nullptr;

    // A zero-filled XRGB buffer is opaque black; no mapping is needed.
    auto pool = makeShared<CCWlShmPool>(state.shm->sendCreatePool(fd.get(), size));
    if (!pool->resource())
        return nullptr;
    auto buffer = makeShared<CCWlBuffer>(pool->sendCreateBuffer(0, width, height, width * 4, WL_SHM_FORMAT_XRGB8888));
    pool->sendDestroy();
    return buffer->resource() ? buffer : nullptr;
}

static std::string surfaceName(const SWlState& state, wl_proxy* surface) {
    if (surface && state.root && surface == state.root->resource())
        return "root";
    if (surface && state.child && surface == state.child->resource())
        return "child";
    return "other";
}

static bool setup(SWlState& state) {
    state.registry = makeShared<CCWlRegistry>(reinterpret_cast<wl_proxy*>(wl_display_get_registry(state.display)));
    state.registry->setGlobal([&](CCWlRegistry*, uint32_t id, const char* name, uint32_t) {
        const auto bind = [&](const wl_interface* interface) {
            return reinterpret_cast<wl_proxy*>(wl_registry_bind(reinterpret_cast<wl_registry*>(state.registry->resource()), id, interface, 1));
        };
        const std::string global = name;
        if (global == "wl_compositor")
            state.compositor = makeShared<CCWlCompositor>(bind(&wl_compositor_interface));
        else if (global == "wl_subcompositor")
            state.subcompositor = makeShared<CCWlSubcompositor>(bind(&wl_subcompositor_interface));
        else if (global == "wl_shm")
            state.shm = makeShared<CCWlShm>(bind(&wl_shm_interface));
        else if (global == "wl_seat")
            state.seat = makeShared<CCWlSeat>(bind(&wl_seat_interface));
        else if (global == "xdg_wm_base")
            state.shell = makeShared<CCXdgWmBase>(bind(&xdg_wm_base_interface));
        else if (global == "zwp_pointer_constraints_v1")
            state.constraints = makeShared<CCZwpPointerConstraintsV1>(bind(&zwp_pointer_constraints_v1_interface));
    });
    if (!sync(state) || !state.compositor || !state.subcompositor || !state.shm || !state.seat || !state.shell || !state.constraints)
        return false;

    state.shell->setPing([&](CCXdgWmBase*, uint32_t serial) { state.shell->sendPong(serial); });
    state.pointer = makeShared<CCWlPointer>(state.seat->sendGetPointer());
    if (!state.pointer->resource())
        return false;
    state.pointer->setEnter([&](CCWlPointer*, uint32_t, wl_proxy* surface, wl_fixed_t, wl_fixed_t) {
        state.pointerTarget = surfaceName(state, surface);
        if (state.pointerTarget == "root")
            ++state.rootEnters;
        else if (state.pointerTarget == "child")
            ++state.childEnters;
    });
    state.pointer->setLeave([&](CCWlPointer*, uint32_t, wl_proxy*) { state.pointerTarget = "none"; });
    state.pointer->setMotion([&](CCWlPointer*, uint32_t, wl_fixed_t x, wl_fixed_t y) {
        ++state.motions;
        state.motionX = wl_fixed_to_int(x);
        state.motionY = wl_fixed_to_int(y);
        if (!state.active)
            return;
        if (state.lock)
            ++state.lockedMotions;
        else {
            ++state.confinedMotions;
            // The confinement fixture is [10,42) in constraint-surface coordinates.
            if (x < wl_fixed_from_int(10) || x >= wl_fixed_from_int(42) || y < wl_fixed_from_int(10) || y >= wl_fixed_from_int(42))
                ++state.outsideMotions;
        }
    });
    state.keyboard = makeShared<CCWlKeyboard>(state.seat->sendGetKeyboard());
    if (!state.keyboard->resource())
        return false;
    state.keyboard->setKeymap([](CCWlKeyboard*, wl_keyboard_keymap_format, int32_t fd, uint32_t) { close(fd); });
    state.keyboard->setEnter([&](CCWlKeyboard*, uint32_t, wl_proxy* surface, wl_array*) {
        state.keyboardTarget = surfaceName(state, surface);
        ++state.keyboardEnters;
    });
    state.keyboard->setLeave([&](CCWlKeyboard*, uint32_t, wl_proxy*) {
        state.keyboardTarget = "none";
        ++state.keyboardLeaves;
    });

    state.root  = makeShared<CCWlSurface>(state.compositor->sendCreateSurface());
    state.child = makeShared<CCWlSurface>(state.compositor->sendCreateSurface());
    if (!state.root->resource() || !state.child->resource())
        return false;
    state.subsurface = makeShared<CCWlSubsurface>(state.subcompositor->sendGetSubsurface(state.child.get(), state.root.get()));
    if (!state.subsurface->resource())
        return false;
    state.subsurface->sendSetPosition(80, 60);
    state.rootBuffer  = createBuffer(state, 320, 240);
    state.childBuffer = createBuffer(state, 96, 80);
    if (!state.rootBuffer || !state.childBuffer)
        return false;
    state.child->sendAttach(state.childBuffer.get(), 0, 0);
    state.child->sendCommit();
    state.xdgSurface = makeShared<CCXdgSurface>(state.shell->sendGetXdgSurface(state.root->resource()));
    if (!state.xdgSurface->resource())
        return false;
    state.toplevel = makeShared<CCXdgToplevel>(state.xdgSurface->sendGetToplevel());
    if (!state.toplevel->resource())
        return false;
    state.xdgSurface->setConfigure([&](CCXdgSurface*, uint32_t serial) {
        state.xdgSurface->sendAckConfigure(serial);
        state.root->sendAttach(state.rootBuffer.get(), 0, 0);
        state.root->sendCommit();
        state.configured = true;
    });
    state.toplevel->sendSetAppId("pointer-constraints");
    state.toplevel->sendSetTitle("pointer constraints test");
    state.root->sendCommit();
    while (!state.configured) {
        if (!sync(state))
            return false;
    }
    return sync(state);
}

static std::string report(const SWlState& state) {
    return std::format("locked={} unlocked={} confined={} unconfined={} active={} pointer={} root_enters={} child_enters={} keyboard={} keyboard_enters={} keyboard_leaves={} "
                       "motions={} motion_x={} motion_y={} locked_motions={} confined_motions={} outside_motions={} bad_activations={}",
                       state.locked, state.unlocked, state.confined, state.unconfined, state.active ? 1 : 0, state.pointerTarget, state.rootEnters, state.childEnters,
                       state.keyboardTarget, state.keyboardEnters, state.keyboardLeaves, state.motions, state.motionX, state.motionY, state.lockedMotions, state.confinedMotions,
                       state.outsideMotions, state.badActivations);
}

static bool constrain(SWlState& state, const std::string& command) {
    std::istringstream input(command);
    std::string        kind, target, lifetime;
    input >> kind >> target >> lifetime;
    if (state.lock || state.confine || (kind != "lock" && kind != "confine") || (target != "root" && target != "child") || (lifetime != "persistent" && lifetime != "oneshot"))
        return false;

    auto       surface     = target == "child" ? state.child : state.root;
    const auto life        = lifetime == "persistent" ? ZWP_POINTER_CONSTRAINTS_V1_LIFETIME_PERSISTENT : ZWP_POINTER_CONSTRAINTS_V1_LIFETIME_ONESHOT;
    state.constraintTarget = target;
    if (kind == "lock") {
        state.lock = makeShared<CCZwpLockedPointerV1>(state.constraints->sendLockPointer(surface->resource(), state.pointer->resource(), nullptr, life));
        if (!state.lock->resource())
            return false;
        state.lock->setLocked([&](CCZwpLockedPointerV1*) {
            if (state.pointerTarget != state.constraintTarget)
                ++state.badActivations;
            ++state.locked;
            state.active = true;
        });
        state.lock->setUnlocked([&](CCZwpLockedPointerV1*) {
            ++state.unlocked;
            state.active = false;
        });
    } else {
        auto region = makeShared<CCWlRegion>(state.compositor->sendCreateRegion());
        if (!region->resource())
            return false;
        region->sendAdd(10, 10, 32, 32);
        state.confine = makeShared<CCZwpConfinedPointerV1>(state.constraints->sendConfinePointer(surface->resource(), state.pointer->resource(), region->resource(), life));
        region->sendDestroy();
        if (!state.confine->resource())
            return false;
        state.confine->setConfined([&](CCZwpConfinedPointerV1*) {
            if (state.pointerTarget != state.constraintTarget)
                ++state.badActivations;
            ++state.confined;
            state.active = true;
        });
        state.confine->setUnconfined([&](CCZwpConfinedPointerV1*) {
            ++state.unconfined;
            state.active = false;
        });
    }
    return sync(state);
}

int main() {
    SWlState state;
    state.display = wl_display_connect(nullptr);
    if (!state.display || !setup(state)) {
        std::cout << "error setup" << std::endl;
        return 1;
    }
    std::cout << "started" << std::endl;

    // Like surface-scale-transform, every command is a display synchronization point.
    std::string command;
    while (std::getline(std::cin, command) && command != "exit") {
        if (!sync(state))
            return 1;
        if (command == "report")
            std::cout << report(state) << std::endl;
        else if (command == "destroy") {
            if (state.lock)
                state.lock->sendDestroy();
            if (state.confine)
                state.confine->sendDestroy();
            state.lock.reset();
            state.confine.reset();
            // Destroy has no unlocked/unconfined event; retain all observed event counters.
            state.active = false;
            std::cout << (sync(state) ? "ok" : "error destroy") << std::endl;
        } else
            std::cout << (constrain(state, command) ? "ok" : "error command") << std::endl;
    }
    const auto display = state.display;
    state              = {};
    wl_display_disconnect(display);
    return 0;
}
