#pragma once

#include "../defines.hpp"
#include "text-input-unstable-v1-protocol.h"

#include <vector>

struct STextInput;

struct STextInputV1 {
    wl_client*   client         = nullptr;
    wl_resource* resourceCaller = nullptr;

    wl_resource* resourceImpl = nullptr;

    wlr_surface* focusedSurface = nullptr;

    STextInput*  pTextInput = nullptr;

    wl_signal    sEnable;
    wl_signal    sDisable;
    wl_signal    sCommit;
    wl_signal    sDestroy;

    uint32_t     serial = 0;

    bool         active = false;

    struct SPendingSurr {
        bool        isPending = false;
        std::string text      = "";
        uint32_t    cursor    = 0;
        uint32_t    anchor    = 0;
    } pendingSurrounding;

    struct SPendingCT {
        bool     isPending = false;
        uint32_t hint      = 0;
        uint32_t purpose   = 0;
    } pendingContentType;

    wlr_box cursorRectangle = {0, 0, 0, 0};

    bool    operator==(const STextInputV1& other) {
        return other.client == client && other.resourceCaller == resourceCaller && other.resourceImpl == resourceImpl;
    }
};

class CTextInputV1ProtocolManager {
  public:
    CTextInputV1ProtocolManager();

    void bindManager(wl_client* client, void* data, uint32_t version, uint32_t id);
    void createTI(wl_client* client, wl_resource* resource, uint32_t id);
    void removeTI(STextInputV1* pTI);

    void displayDestroy();

    // handlers for tiv1
    void handleActivate(wl_client* client, wl_resource* resource, wl_resource* seat, wl_resource* surface);
    void handleDeactivate(wl_client* client, wl_resource* resource, wl_resource* seat);
    void handleShowInputPanel(wl_client* client, wl_resource* resource);
    void handleHideInputPanel(wl_client* client, wl_resource* resource);
    void handleReset(wl_client* client, wl_resource* resource);
    void handleSetSurroundingText(wl_client* client, wl_resource* resource, const char* text, uint32_t cursor, uint32_t anchor);
    void handleSetContentType(wl_client* client, wl_resource* resource, uint32_t hint, uint32_t purpose);
    void handleSetCursorRectangle(wl_client* client, wl_resource* resource, int32_t x, int32_t y, int32_t width, int32_t height);
    void handleSetPreferredLanguage(wl_client* client, wl_resource* resource, const char* language);
    void handleCommitState(wl_client* client, wl_resource* resource, uint32_t serial);
    void handleInvokeAction(wl_client* client, wl_resource* resource, uint32_t button, uint32_t index);

  private:
    wl_global*                                 m_pGlobal = nullptr;
    wl_listener                                m_liDisplayDestroy;

    std::vector<std::unique_ptr<STextInputV1>> m_pClients;
};