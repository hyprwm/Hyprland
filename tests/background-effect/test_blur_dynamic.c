/*
 * Test: Dynamic blur region via ext-background-effect-v1
 *
 * Creates a centered layer surface and shifts the blur region every 2 seconds:
 * left half -> right half -> center strip -> repeat.
 * Verifies dynamic updates without hide/show.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <time.h>
#include <wayland-client.h>

#include "ext-background-effect-v1-client-protocol.h"
#include "wlr-layer-shell-unstable-v1-client-protocol.h"

#define WIDTH 400
#define HEIGHT 300

static struct wl_display*                       display;
static struct wl_registry*                      registry;
static struct wl_compositor*                    compositor;
static struct wl_shm*                           shm;
static struct zwlr_layer_shell_v1*              layer_shell;
static struct ext_background_effect_manager_v1* bg_effect_manager;
static int                                      running = 1;

static void layer_surface_configure(void* data, struct zwlr_layer_surface_v1* surface, uint32_t serial, uint32_t w, uint32_t h) {
    (void)data;
    (void)w;
    (void)h;
    zwlr_layer_surface_v1_ack_configure(surface, serial);
}

static void layer_surface_closed(void* data, struct zwlr_layer_surface_v1* surface) {
    (void)data;
    (void)surface;
    running = 0;
}

static const struct zwlr_layer_surface_v1_listener layer_surface_listener = {
    .configure = layer_surface_configure,
    .closed    = layer_surface_closed,
};

static void bg_effect_manager_capabilities(void* data, struct ext_background_effect_manager_v1* manager, uint32_t caps) {
    (void)data;
    (void)manager;
    (void)caps;
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
    else if (strcmp(interface, zwlr_layer_shell_v1_interface.name) == 0)
        layer_shell = wl_registry_bind(reg, name, &zwlr_layer_shell_v1_interface, 1);
    else if (strcmp(interface, ext_background_effect_manager_v1_interface.name) == 0) {
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
        pixels[i] = 0x80303050;

    struct wl_shm_pool* pool   = wl_shm_create_pool(shm, fd, size);
    struct wl_buffer*   buffer = wl_shm_pool_create_buffer(pool, 0, width, height, stride, WL_SHM_FORMAT_ARGB8888);
    wl_shm_pool_destroy(pool);
    munmap(data, size);
    close(fd);
    return buffer;
}

static void set_blur_region(struct ext_background_effect_surface_v1* bg_effect, struct wl_surface* surface, int x, int y, int w, int h) {
    struct wl_region* region = wl_compositor_create_region(compositor);
    wl_region_add(region, x, y, w, h);
    ext_background_effect_surface_v1_set_blur_region(bg_effect, region);
    wl_region_destroy(region);
    wl_surface_commit(surface);
}

int main(void) {
    display  = wl_display_connect(NULL);
    registry = wl_display_get_registry(display);
    wl_registry_add_listener(registry, &registry_listener, NULL);
    wl_display_roundtrip(display);

    if (!compositor || !shm || !layer_shell || !bg_effect_manager) {
        fprintf(stderr, "Missing required globals\n");
        return 1;
    }

    wl_display_roundtrip(display);

    struct wl_surface*            surface       = wl_compositor_create_surface(compositor);
    struct zwlr_layer_surface_v1* layer_surface = zwlr_layer_shell_v1_get_layer_surface(layer_shell, surface, NULL, ZWLR_LAYER_SHELL_V1_LAYER_TOP, "test-blur-dynamic");
    zwlr_layer_surface_v1_set_size(layer_surface, WIDTH, HEIGHT);
    /* No anchors = centered */
    zwlr_layer_surface_v1_add_listener(layer_surface, &layer_surface_listener, NULL);
    wl_surface_commit(surface);
    wl_display_roundtrip(display);

    struct ext_background_effect_surface_v1* bg_effect = ext_background_effect_manager_v1_get_background_effect(bg_effect_manager, surface);

    struct wl_buffer* buffer = create_shm_buffer(WIDTH, HEIGHT);
    wl_surface_attach(surface, buffer, 0, 0);

    set_blur_region(bg_effect, surface, 0, 0, WIDTH / 2, HEIGHT);

    printf("Dynamic blur test running. Shifting blur region every 2 seconds.\n");

    int             phase = 0;
    struct timespec last;
    clock_gettime(CLOCK_MONOTONIC, &last);

    while (running) {
        if (wl_display_dispatch_pending(display) == -1)
            break;
        if (wl_display_flush(display) == -1)
            break;

        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        double elapsed = (now.tv_sec - last.tv_sec) + (now.tv_nsec - last.tv_nsec) / 1e9;

        if (elapsed >= 2.0) {
            phase = (phase + 1) % 3;
            switch (phase) {
            case 0:
                printf("  -> left half\n");
                set_blur_region(bg_effect, surface, 0, 0, WIDTH / 2, HEIGHT);
                break;
            case 1:
                printf("  -> right half\n");
                set_blur_region(bg_effect, surface, WIDTH / 2, 0, WIDTH / 2, HEIGHT);
                break;
            case 2:
                printf("  -> center strip\n");
                set_blur_region(bg_effect, surface, WIDTH / 4, 0, WIDTH / 2, HEIGHT);
                break;
            }
            last = now;
        }

        usleep(50000);
    }

    ext_background_effect_surface_v1_destroy(bg_effect);
    zwlr_layer_surface_v1_destroy(layer_surface);
    wl_surface_destroy(surface);
    wl_buffer_destroy(buffer);
    ext_background_effect_manager_v1_destroy(bg_effect_manager);
    wl_display_disconnect(display);
    return 0;
}
