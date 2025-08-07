#include "VirtualKeyboard.hpp"
#include <filesystem>
#include <sys/mman.h>
#include "../config/ConfigValue.hpp"
#include "../config/ConfigManager.hpp"
#include "../devices/IKeyboard.hpp"
#include "../helpers/time/Time.hpp"
#include "../helpers/MiscFunctions.hpp"
using namespace Hyprutils::OS;

static std::string virtualKeyboardNameForWlClient(wl_client* client) {
    std::string name = "hl-virtual-keyboard";

    static auto PVKNAMEPROC = CConfigValue<Hyprlang::INT>("misc:name_vk_after_proc");
    if (!*PVKNAMEPROC)
        return name;

    name += "-";
    const auto CLIENTNAME = binaryNameForWlClient(client);
    if (CLIENTNAME.has_value()) {
        const auto PATH = std::filesystem::path(CLIENTNAME.value());
        if (PATH.has_filename()) {
            const auto FILENAME = PATH.filename();
            const auto NAME     = deviceNameToInternalString(FILENAME);
            name += NAME;
            return name;
        }
    }

    name += "unknown";
    return name;
}

CVirtualKeyboardV1Resource::CVirtualKeyboardV1Resource(SP<CZwpVirtualKeyboardV1> resource_) : m_resource(resource_) {
    if UNLIKELY (!good())
        return;

    m_resource->setDestroy([this](CZwpVirtualKeyboardV1* r) { destroy(); });
    m_resource->setOnDestroy([this](CZwpVirtualKeyboardV1* r) { destroy(); });

    m_resource->setKey([this](CZwpVirtualKeyboardV1* r, uint32_t timeMs, uint32_t key, uint32_t state) {
        if UNLIKELY (!m_hasKeymap) {
            r->error(ZWP_VIRTUAL_KEYBOARD_V1_ERROR_NO_KEYMAP, "Key event received before a keymap was set");
            return;
        }

        m_events.key.emit(IKeyboard::SKeyEvent{
            .timeMs  = timeMs,
            .keycode = key,
            .state   = sc<wl_keyboard_key_state>(state),
        });

        const bool CONTAINS = std::ranges::contains(m_pressed, key);
        if (state && !CONTAINS)
            m_pressed.emplace_back(key);
        else if (!state && CONTAINS)
            std::erase(m_pressed, key);
    });

    m_resource->setModifiers([this](CZwpVirtualKeyboardV1* r, uint32_t depressed, uint32_t latched, uint32_t locked, uint32_t group) {
        if UNLIKELY (!m_hasKeymap) {
            r->error(ZWP_VIRTUAL_KEYBOARD_V1_ERROR_NO_KEYMAP, "Mods event received before a keymap was set");
            return;
        }

        m_events.modifiers.emit(IKeyboard::SModifiersEvent{
            .depressed = depressed,
            .latched   = latched,
            .locked    = locked,
            .group     = group,
        });
    });

    m_resource->setKeymap([this](CZwpVirtualKeyboardV1* r, uint32_t fmt, int32_t fd, uint32_t len) {
        auto            xkbContext = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
        CFileDescriptor keymapFd{fd};
        if UNLIKELY (!xkbContext) {
            LOGM(ERR, "xkbContext creation failed");
            r->noMemory();
            return;
        }

        auto keymapData = mmap(nullptr, len, PROT_READ, MAP_PRIVATE, keymapFd.get(), 0);
        if UNLIKELY (keymapData == MAP_FAILED) {
            LOGM(ERR, "keymapData alloc failed");
            xkb_context_unref(xkbContext);
            r->noMemory();
            return;
        }

        auto xkbKeymap = xkb_keymap_new_from_string(xkbContext, sc<const char*>(keymapData), XKB_KEYMAP_FORMAT_TEXT_V1, XKB_KEYMAP_COMPILE_NO_FLAGS);
        munmap(keymapData, len);

        if UNLIKELY (!xkbKeymap) {
            LOGM(ERR, "xkbKeymap creation failed");
            xkb_context_unref(xkbContext);
            r->noMemory();
            return;
        }

        m_events.keymap.emit(IKeyboard::SKeymapEvent{
            .keymap = xkbKeymap,
        });
        m_hasKeymap = true;

        xkb_keymap_unref(xkbKeymap);
        xkb_context_unref(xkbContext);
    });

    m_name = virtualKeyboardNameForWlClient(resource_->client());
}

CVirtualKeyboardV1Resource::~CVirtualKeyboardV1Resource() {
    m_events.destroy.emit();
}

bool CVirtualKeyboardV1Resource::good() {
    return m_resource->resource();
}

wl_client* CVirtualKeyboardV1Resource::client() {
    return m_resource->resource() ? m_resource->client() : nullptr;
}

void CVirtualKeyboardV1Resource::releasePressed() {
    for (auto const& p : m_pressed) {
        m_events.key.emit(IKeyboard::SKeyEvent{
            .timeMs  = Time::millis(Time::steadyNow()),
            .keycode = p,
            .state   = WL_KEYBOARD_KEY_STATE_RELEASED,
        });
    }

    m_pressed.clear();
}

void CVirtualKeyboardV1Resource::destroy() {
    const auto RELEASEPRESSED = g_pConfigManager->getDeviceInt(m_name, "release_pressed_on_close", "input:virtualkeyboard:release_pressed_on_close");
    if (RELEASEPRESSED)
        releasePressed();
    m_events.destroy.emit();
    PROTO::virtualKeyboard->destroyResource(this);
}

CVirtualKeyboardProtocol::CVirtualKeyboardProtocol(const wl_interface* iface, const int& ver, const std::string& name) : IWaylandProtocol(iface, ver, name) {
    ;
}

void CVirtualKeyboardProtocol::bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id) {
    const auto RESOURCE = m_managers.emplace_back(makeUnique<CZwpVirtualKeyboardManagerV1>(client, ver, id)).get();
    RESOURCE->setOnDestroy([this](CZwpVirtualKeyboardManagerV1* p) { this->onManagerResourceDestroy(p->resource()); });

    RESOURCE->setCreateVirtualKeyboard([this](CZwpVirtualKeyboardManagerV1* pMgr, wl_resource* seat, uint32_t id) { this->onCreateKeeb(pMgr, seat, id); });
}

void CVirtualKeyboardProtocol::onManagerResourceDestroy(wl_resource* res) {
    std::erase_if(m_managers, [&](const auto& other) { return other->resource() == res; });
}

void CVirtualKeyboardProtocol::destroyResource(CVirtualKeyboardV1Resource* keeb) {
    std::erase_if(m_keyboards, [&](const auto& other) { return other.get() == keeb; });
}

void CVirtualKeyboardProtocol::onCreateKeeb(CZwpVirtualKeyboardManagerV1* pMgr, wl_resource* seat, uint32_t id) {

    const auto RESOURCE = m_keyboards.emplace_back(makeShared<CVirtualKeyboardV1Resource>(makeShared<CZwpVirtualKeyboardV1>(pMgr->client(), pMgr->version(), id)));

    if UNLIKELY (!RESOURCE->good()) {
        pMgr->noMemory();
        m_keyboards.pop_back();
        return;
    }

    LOGM(LOG, "New VKeyboard at id {}", id);

    m_events.newKeyboard.emit(RESOURCE);
}
