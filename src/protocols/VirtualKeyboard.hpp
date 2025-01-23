#pragma once

#include <vector>
#include <cstdint>
#include "WaylandProtocol.hpp"
#include "virtual-keyboard-unstable-v1.hpp"
#include "../helpers/signal/Signal.hpp"

class CVirtualKeyboardV1Resource {
  public:
    CVirtualKeyboardV1Resource(SP<CZwpVirtualKeyboardV1> resource_);
    ~CVirtualKeyboardV1Resource();

    struct {
        CSignal destroy;
        CSignal key;
        CSignal modifiers;
        CSignal keymap;
    } events;

    bool        good();
    wl_client*  client();

    std::string name = "";

  private:
    SP<CZwpVirtualKeyboardV1> resource;

    void                      releasePressed();

    bool                      hasKeymap = false;

    std::vector<uint32_t>     pressed;
};

class CVirtualKeyboardProtocol : public IWaylandProtocol {
  public:
    CVirtualKeyboardProtocol(const wl_interface* iface, const int& ver, const std::string& name);

    virtual void bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id);

    struct {
        CSignal newKeyboard; // SP<CVirtualKeyboard>
    } events;

  private:
    void onManagerResourceDestroy(wl_resource* res);
    void destroyResource(CVirtualKeyboardV1Resource* keeb);
    void onCreateKeeb(CZwpVirtualKeyboardManagerV1* pMgr, wl_resource* seat, uint32_t id);

    //
    std::vector<UP<CZwpVirtualKeyboardManagerV1>> m_vManagers;
    std::vector<SP<CVirtualKeyboardV1Resource>>   m_vKeyboards;

    friend class CVirtualKeyboardV1Resource;
};

namespace PROTO {
    inline UP<CVirtualKeyboardProtocol> virtualKeyboard;
};
