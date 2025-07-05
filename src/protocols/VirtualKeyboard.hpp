#pragma once

#include <vector>
#include <cstdint>
#include "WaylandProtocol.hpp"
#include "../devices/IKeyboard.hpp"
#include "../devices/VirtualKeyboard.hpp"
#include "virtual-keyboard-unstable-v1.hpp"
#include "../helpers/signal/Signal.hpp"
#include <hyprutils/os/FileDescriptor.hpp>

class CVirtualKeyboardV1Resource {
  public:
    CVirtualKeyboardV1Resource(SP<CZwpVirtualKeyboardV1> resource_);
    ~CVirtualKeyboardV1Resource();

    struct {
        CSignalT<>                           destroy;
        CSignalT<IKeyboard::SKeyEvent>       key;
        CSignalT<IKeyboard::SModifiersEvent> modifiers;
        CSignalT<IKeyboard::SKeymapEvent>    keymap;
    } m_events;

    bool        good();
    wl_client*  client();

    std::string m_name = "";

  private:
    SP<CZwpVirtualKeyboardV1> m_resource;

    void                      releasePressed();

    bool                      m_hasKeymap = false;

    std::vector<uint32_t>     m_pressed;
};

class CVirtualKeyboardProtocol : public IWaylandProtocol {
  public:
    CVirtualKeyboardProtocol(const wl_interface* iface, const int& ver, const std::string& name);

    virtual void bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id);

    struct {
        CSignalT<SP<CVirtualKeyboardV1Resource>> newKeyboard;
    } m_events;

  private:
    void onManagerResourceDestroy(wl_resource* res);
    void destroyResource(CVirtualKeyboardV1Resource* keeb);
    void onCreateKeeb(CZwpVirtualKeyboardManagerV1* pMgr, wl_resource* seat, uint32_t id);

    //
    std::vector<UP<CZwpVirtualKeyboardManagerV1>> m_managers;
    std::vector<SP<CVirtualKeyboardV1Resource>>   m_keyboards;

    friend class CVirtualKeyboardV1Resource;
};

namespace PROTO {
    inline UP<CVirtualKeyboardProtocol> virtualKeyboard;
};
