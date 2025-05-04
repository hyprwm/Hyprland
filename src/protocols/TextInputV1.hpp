#pragma once

#include "../defines.hpp"
#include "../protocols/core/Compositor.hpp"
#include "text-input-unstable-v1.hpp"
#include "WaylandProtocol.hpp"

#include <vector>

class CTextInput;

class CTextInputV1 {
  public:
    CTextInputV1(SP<CZwpTextInputV1> resource);
    ~CTextInputV1();

    void       enter(SP<CWLSurfaceResource> surface);
    void       leave();

    void       preeditCursor(int32_t index);
    void       preeditStyling(uint32_t index, uint32_t length, zwpTextInputV1PreeditStyle style);
    void       preeditString(uint32_t serial, const char* text, const char* commit);
    void       commitString(uint32_t serial, const char* text);
    void       deleteSurroundingText(int32_t index, uint32_t length);

    bool       good();
    wl_client* client();

  private:
    SP<CZwpTextInputV1> m_resource;

    uint32_t            m_serial = 0;
    bool                m_active = false;

    struct {
        CSignal onCommit;
        CSignal enable;
        CSignal disable;
        CSignal reset;
        CSignal destroy;
    } m_events;

    struct SPendingSurr {
        bool        isPending = false;
        std::string text      = "";
        uint32_t    cursor    = 0;
        uint32_t    anchor    = 0;
    } m_pendingSurrounding;

    struct SPendingCT {
        bool     isPending = false;
        uint32_t hint      = 0;
        uint32_t purpose   = 0;
    } m_pendingContentType;

    CBox m_cursorRectangle = {0, 0, 0, 0};

    friend class CTextInput;
    friend class CTextInputV1Protocol;
};

class CTextInputV1Protocol : public IWaylandProtocol {
  public:
    CTextInputV1Protocol(const wl_interface* iface, const int& ver, const std::string& name);

    virtual void bindManager(wl_client* client, void* data, uint32_t version, uint32_t id);
    void         destroyResource(CTextInputV1* resource);
    void         destroyResource(CZwpTextInputManagerV1* client);

    struct {
        CSignal newTextInput; // WP<CTextInputV3>
    } m_events;

  private:
    std::vector<SP<CZwpTextInputManagerV1>> m_managers;
    std::vector<SP<CTextInputV1>>           m_clients;

    friend class CTextInputV1;
};

namespace PROTO {
    inline UP<CTextInputV1Protocol> textInputV1;
};
