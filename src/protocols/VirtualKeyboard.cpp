#include "VirtualKeyboard.hpp"
#include <sys/mman.h>

#define LOGM PROTO::virtualKeyboard->protoLog

static const struct wlr_keyboard_impl virtualKeyboardImpl = {
    .name = "virtual-keyboard",
};

CVirtualKeyboardV1Resource::CVirtualKeyboardV1Resource(SP<CZwpVirtualKeyboardV1> resource_) : resource(resource_) {
    if (!good())
        return;

    resource->setDestroy([this](CZwpVirtualKeyboardV1* r) {
        releasePressed();
        events.destroy.emit();
        PROTO::virtualKeyboard->destroyResource(this);
    });
    resource->setOnDestroy([this](CZwpVirtualKeyboardV1* r) {
        releasePressed();
        events.destroy.emit();
        PROTO::virtualKeyboard->destroyResource(this);
    });

    resource->setKey([this](CZwpVirtualKeyboardV1* r, uint32_t timeMs, uint32_t key, uint32_t state) {
        if (!hasKeymap) {
            r->error(ZWP_VIRTUAL_KEYBOARD_V1_ERROR_NO_KEYMAP, "Key event received before a keymap was set");
            return;
        }

        wlr_keyboard_key_event event = {
            .time_msec    = timeMs,
            .keycode      = key,
            .update_state = false,
            .state        = (wl_keyboard_key_state)state,
        };
        wlr_keyboard_notify_key(&keyboard, &event);
    });

    resource->setModifiers([this](CZwpVirtualKeyboardV1* r, uint32_t depressed, uint32_t latched, uint32_t locked, uint32_t group) {
        if (!hasKeymap) {
            r->error(ZWP_VIRTUAL_KEYBOARD_V1_ERROR_NO_KEYMAP, "Mods event received before a keymap was set");
            return;
        }

        wlr_keyboard_notify_modifiers(&keyboard, depressed, latched, locked, group);
    });

    resource->setKeymap([this](CZwpVirtualKeyboardV1* r, uint32_t fmt, int32_t fd, uint32_t len) {
        auto xkbContext = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
        if (!xkbContext) {
            LOGM(ERR, "xkbContext creation failed");
            r->noMemory();
            close(fd);
            return;
        }

        auto keymapData = mmap(nullptr, len, PROT_READ, MAP_PRIVATE, fd, 0);
        if (keymapData == MAP_FAILED) {
            LOGM(ERR, "keymapData alloc failed");
            xkb_context_unref(xkbContext);
            r->noMemory();
            close(fd);
            return;
        }

        auto xkbKeymap = xkb_keymap_new_from_string(xkbContext, (const char*)keymapData, XKB_KEYMAP_FORMAT_TEXT_V1, XKB_KEYMAP_COMPILE_NO_FLAGS);
        munmap(keymapData, len);

        if (!xkbKeymap) {
            LOGM(ERR, "xkbKeymap creation failed");
            xkb_context_unref(xkbContext);
            r->noMemory();
            close(fd);
            return;
        }

        wlr_keyboard_set_keymap(&keyboard, xkbKeymap);
        hasKeymap = true;

        xkb_keymap_unref(xkbKeymap);
        xkb_context_unref(xkbContext);
        close(fd);
    });

    wlr_keyboard_init(&keyboard, &virtualKeyboardImpl, "CVirtualKeyboard");
}

CVirtualKeyboardV1Resource::~CVirtualKeyboardV1Resource() {
    events.destroy.emit();
    wlr_keyboard_finish(&keyboard);
}

bool CVirtualKeyboardV1Resource::good() {
    return resource->resource();
}

wlr_keyboard* CVirtualKeyboardV1Resource::wlr() {
    return &keyboard;
}

wl_client* CVirtualKeyboardV1Resource::client() {
    return resource->resource() ? resource->client() : nullptr;
}

void CVirtualKeyboardV1Resource::releasePressed() {
    timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    size_t keycodesNum = keyboard.num_keycodes;

    for (size_t i = 0; i < keycodesNum; ++i) {
        struct wlr_keyboard_key_event event = {
            .time_msec    = (now.tv_sec * 1000 + now.tv_nsec / 1000000),
            .keycode      = keyboard.keycodes[keycodesNum - i - 1],
            .update_state = false,
            .state        = WL_KEYBOARD_KEY_STATE_RELEASED,
        };
        wlr_keyboard_notify_key(&keyboard, &event); // updates num_keycodes
    }
}

CVirtualKeyboardProtocol::CVirtualKeyboardProtocol(const wl_interface* iface, const int& ver, const std::string& name) : IWaylandProtocol(iface, ver, name) {
    ;
}

void CVirtualKeyboardProtocol::bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id) {
    const auto RESOURCE = m_vManagers.emplace_back(std::make_unique<CZwpVirtualKeyboardManagerV1>(client, ver, id)).get();
    RESOURCE->setOnDestroy([this](CZwpVirtualKeyboardManagerV1* p) { this->onManagerResourceDestroy(p->resource()); });

    RESOURCE->setCreateVirtualKeyboard([this](CZwpVirtualKeyboardManagerV1* pMgr, wl_resource* seat, uint32_t id) { this->onCreateKeeb(pMgr, seat, id); });
}

void CVirtualKeyboardProtocol::onManagerResourceDestroy(wl_resource* res) {
    std::erase_if(m_vManagers, [&](const auto& other) { return other->resource() == res; });
}

void CVirtualKeyboardProtocol::destroyResource(CVirtualKeyboardV1Resource* keeb) {
    std::erase_if(m_vKeyboards, [&](const auto& other) { return other.get() == keeb; });
}

void CVirtualKeyboardProtocol::onCreateKeeb(CZwpVirtualKeyboardManagerV1* pMgr, wl_resource* seat, uint32_t id) {

    const auto RESOURCE = m_vKeyboards.emplace_back(makeShared<CVirtualKeyboardV1Resource>(makeShared<CZwpVirtualKeyboardV1>(pMgr->client(), pMgr->version(), id)));

    if (!RESOURCE->good()) {
        pMgr->noMemory();
        m_vKeyboards.pop_back();
        return;
    }

    LOGM(LOG, "New VKeyboard at id {}", id);

    events.newKeyboard.emit(RESOURCE);
}
