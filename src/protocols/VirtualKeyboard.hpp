#pragma once

#include <memory>
#include <vector>
#include <cstdint>
#include "WaylandProtocol.hpp"
#include "virtual-keyboard-unstable-v1.hpp"
#include "../helpers/signal/Signal.hpp"

class CVirtualKeyboard {
  public:
    CVirtualKeyboard(SP<CZwpVirtualKeyboardV1> resource_);
    ~CVirtualKeyboard();

    struct {
        CSignal destroy;
    } events;

    bool          good();
    wlr_keyboard* wlr();
    wl_client*    client();

  private:
    SP<CZwpVirtualKeyboardV1> resource;
    wlr_keyboard              keyboard;

    bool                      hasKeymap = false;
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
    void destroyResource(CVirtualKeyboard* keeb);
    void onCreateKeeb(CZwpVirtualKeyboardManagerV1* pMgr, wl_resource* seat, uint32_t id);

    //
    std::vector<UP<CZwpVirtualKeyboardManagerV1>> m_vManagers;
    std::vector<SP<CVirtualKeyboard>>             m_vKeyboards;

    friend class CVirtualKeyboard;
};

namespace PROTO {
    inline UP<CVirtualKeyboardProtocol> virtualKeyboard;
};
