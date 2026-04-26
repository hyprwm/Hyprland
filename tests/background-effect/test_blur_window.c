/*
 * Test: Window blur via ext-background-effect-v1
 *
 * Creates a regular xdg-shell window (not a layer surface) and sets a blur
 * region covering the left half. Verifies that the protocol works for
 * normal application windows, not just layer surfaces.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <wayland-client.h>

#include "ext-background-effect-v1-client-protocol.h"
#include "xdg-shell-client-protocol.h"

#define WIDTH 400
#define HEIGHT 300

static struct wl_display*                       display;
static struct wl_registry*                      registry;
static struct wl_compositor*                    compositor;
static struct wl_shm*                           shm;
static struct xdg_wm_base*                      wm_base;
static struct ext_background_effect_manager_v1* bg_effect_manager;
static int                                      running = 1;

static void xdg_wm_base_ping(void* data, struct xdg_wm_base* base, uint32_t serial) {
    (void)data;
    xdg_wm_base_pong(base, serial);
}

static const struct xdg_wm_base_listener wm_base_listener = {
    .ping = xdg_wm_base_ping,
};

static void xdg_surface_configure(void* data, struct xdg_surface* surface, uint32_t serial) {
    (void)data;
    xdg_surface_ack_configure(surface, serial);
}

static const struct xdg_surface_listener xdg_surface_listener = {
    .configure = xdg_surface_configure,
};

static void xdg_toplevel_configure(void* data, struct xdg_toplevel* toplevel, int32_t w, int32_t h, struct wl_array* states) {
    (void)data;
    (void)toplevel;
    (void)w;
    (void)h;
    (void)states;
}

static void xdg_toplevel_close(void* data, struct xdg_toplevel* toplevel) {
    (void)data;
    (void)toplevel;
    running = 0;
}

static void xdg_toplevel_configure_bounds(void* data, struct xdg_toplevel* toplevel, int32_t w, int32_t h) {
    (void)data;
    (void)toplevel;
    (void)w;
    (void)h;
}

static void xdg_toplevel_wm_capabilities(void* data, struct xdg_toplevel* toplevel, struct wl_array* caps) {
    (void)data;
    (void)toplevel;
    (void)caps;
}

static const struct xdg_toplevel_listener xdg_toplevel_listener = {
    .configure        = xdg_toplevel_configure,
    .close            = xdg_toplevel_close,
    .configure_bounds = xdg_toplevel_configure_bounds,
    .wm_capabilities  = xdg_toplevel_wm_capabilities,
};

static void bg_effect_manager_capabilities(void* data, struct ext_background_effect_manager_v1* manager, uint32_t caps) {
    (void)data;
    (void)manager;
    printf("Background effect capabilities: 0x%x\n", caps);
    if (caps & EXT_BACKGROUND_EFFECT_MANAGER_V1_CAPABILITY_BLUR)
        printf("  Blur is supported\n");
}

static const struct ext_background_effect_manager_v1_listener bg_effect_manager_listener = {
    .capabilities = bg_effect_manager_capabilities,
};

static void registry_global(void* data, struct wl_registry* reg, uint32_t name, const char* interface, uint32_t version) {
    (void)data;
    if (strcmp(interface, wl_compositor_interface.name) == 0)
        compositor = wl_registry_bind(reg, name, &wl_compositor_interface, 4);
    else if (strcmp(interface, wl_shm_interface.name) == 0)
        shm = wl_registry_bind(reg, name, &wl_shm_interface, 1);
    else if (strcmp(interface, xdg_wm_base_interface.name) == 0) {
        wm_base = wl_registry_bind(reg, name, &xdg_wm_base_interface, version < 5 ? version : 5);
        xdg_wm_base_add_listener(wm_base, &wm_base_listener, NULL);
    } else if (strcmp(interface, ext_background_effect_manager_v1_interface.name) == 0) {
        bg_effect_manager = wl_registry_bind(reg, name, &ext_background_effect_manager_v1_interface, version);
        ext_background_effect_manager_v1_add_listener(bg_effect_manager, &bg_effect_manager_listener, NULL);
    }
}

static void registry_global_remove(void* data, struct wl_registry* reg, uint32_t name) {
    (void)data;
    (void)reg;
    (void)name;
}

static const struct wl_registry_listener registry_listener = {
    .global        = registry_global,
    .global_remove = registry_global_remove,
};

static struct wl_buffer* create_shm_buffer(int width, int height) {
    int    stride = width * 4;
    int    size   = stride * height;
    char   name[] = "/tmp/wl-shm-XXXXXX";
    int    fd     = mkstemp(name);
    unlink(name);
    ftruncate(fd, size);
    void* data = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

    uint32_t* pixels = (uint32_t*)data;
    for (int i = 0; i < width * height; i++)
        pixels[i] = 0x80202040;

    struct wl_shm_pool* pool   = wl_shm_create_pool(shm, fd, size);
    struct wl_buffer*   buffer = wl_shm_pool_create_buffer(pool, 0, width, height, stride, WL_SHM_FORMAT_ARGB8888);
    wl_shm_pool_destroy(pool);
    munmap(data, size);
    close(fd);
    return buffer;
}

int main(void) {
    display  = wl_display_connect(NULL);
    registry = wl_display_get_registry(display);
    wl_registry_add_listener(registry, &registry_listener, NULL);
    wl_display_roundtrip(display);

    if (!compositor || !shm || !wm_base || !bg_effect_manager) {
        fprintf(stderr, "Missing required globals\n");
        return 1;
    }

    wl_display_roundtrip(display);

    struct wl_surface*     surface     = wl_compositor_create_surface(compositor);
    struct xdg_surface*    xdg_surf    = xdg_wm_base_get_xdg_surface(wm_base, surface);
    struct xdg_toplevel*   toplevel    = xdg_surface_get_toplevel(xdg_surf);

    xdg_surface_add_listener(xdg_surf, &xdg_surface_listener, NULL);
    xdg_toplevel_add_listener(toplevel, &xdg_toplevel_listener, NULL);
    xdg_toplevel_set_title(toplevel, "test-blur-window");
    xdg_toplevel_set_app_id(toplevel, "test-blur-window");
    wl_surface_commit(surface);
    wl_display_roundtrip(display);

    struct ext_background_effect_surface_v1* bg_effect = ext_background_effect_manager_v1_get_background_effect(bg_effect_manager, surface);
    struct wl_region*                        region    = wl_compositor_create_region(compositor);
    wl_region_add(region, 0, 0, WIDTH / 2, HEIGHT);
    ext_background_effect_surface_v1_set_blur_region(bg_effect, region);
    wl_region_destroy(region);

    struct wl_buffer* buffer = create_shm_buffer(WIDTH, HEIGHT);
    wl_surface_attach(surface, buffer, 0, 0);
    wl_surface_commit(surface);

    printf("Window blur test running (left half blurred). Press Ctrl+C or close window to exit.\n");

    while (running && wl_display_dispatch(display) != -1)
        ;

    ext_background_effect_surface_v1_destroy(bg_effect);
    xdg_toplevel_destroy(toplevel);
    xdg_surface_destroy(xdg_surf);
    wl_surface_destroy(surface);
    wl_buffer_destroy(buffer);
    ext_background_effect_manager_v1_destroy(bg_effect_manager);
    wl_display_disconnect(display);
    return 0;
}
