#include "TextInputV1.hpp"

#include "../Compositor.hpp"
#include "core/Compositor.hpp"

CTextInputV1::~CTextInputV1() {
    events.destroy.emit();
}

CTextInputV1::CTextInputV1(SP<CZwpTextInputV1> resource_) : resource(resource_) {
    if (!good())
        return;

    resource->setOnDestroy([this](CZwpTextInputV1* pMgr) { PROTO::textInputV1->destroyResource(this); });

    resource->setActivate([this](CZwpTextInputV1* pMgr, wl_resource* seat, wl_resource* surface) {
        if (!surface) {
            LOGM(WARN, "Text-input-v1 PTI{:x}: No surface to activate text input on!", (uintptr_t)this);
            return;
        }

        active = true;
        events.enable.emit(CWLSurfaceResource::fromResource(surface));
    });

    resource->setDeactivate([this](CZwpTextInputV1* pMgr, wl_resource* seat) {
        active = false;
        events.disable.emit();
    });

    resource->setReset([this](CZwpTextInputV1* pMgr) {
        pendingSurrounding.isPending = false;
        pendingContentType.isPending = false;
    });

    resource->setSetSurroundingText(
        [this](CZwpTextInputV1* pMgr, const char* text, uint32_t cursor, uint32_t anchor) { pendingSurrounding = {true, std::string(text), cursor, anchor}; });

    resource->setSetContentType([this](CZwpTextInputV1* pMgr, uint32_t hint, uint32_t purpose) {
        pendingContentType = {true, hint == (uint32_t)ZWP_TEXT_INPUT_V1_CONTENT_HINT_DEFAULT ? (uint32_t)ZWP_TEXT_INPUT_V1_CONTENT_HINT_NONE : hint,
                              purpose > (uint32_t)ZWP_TEXT_INPUT_V1_CONTENT_PURPOSE_PASSWORD ? hint + 1 : hint};
    });

    resource->setSetCursorRectangle([this](CZwpTextInputV1* pMgr, int32_t x, int32_t y, int32_t width, int32_t height) { cursorRectangle = CBox{x, y, width, height}; });

    resource->setCommitState([this](CZwpTextInputV1* pMgr, uint32_t serial_) {
        serial = serial_;
        events.onCommit.emit();
    });

    // nothing
    resource->setShowInputPanel([this](CZwpTextInputV1* pMgr) {});
    resource->setHideInputPanel([this](CZwpTextInputV1* pMgr) {});
    resource->setSetPreferredLanguage([this](CZwpTextInputV1* pMgr, const char* language) {});
    resource->setInvokeAction([this](CZwpTextInputV1* pMgr, uint32_t button, uint32_t index) {});
}

bool CTextInputV1::good() {
    return resource->resource();
}

wl_client* CTextInputV1::client() {
    return resource->client();
}

void CTextInputV1::enter(SP<CWLSurfaceResource> surface) {
    resource->sendEnter(surface->getResource()->resource());
    active = true;
}

void CTextInputV1::leave() {
    resource->sendLeave();
    active = false;
}

void CTextInputV1::preeditCursor(int32_t index) {
    resource->sendPreeditCursor(index);
}

void CTextInputV1::preeditStyling(uint32_t index, uint32_t length, zwpTextInputV1PreeditStyle style) {
    resource->sendPreeditStyling(index, length, style);
}

void CTextInputV1::preeditString(uint32_t serial, const char* text, const char* commit) {
    resource->sendPreeditString(serial, text, commit);
}

void CTextInputV1::commitString(uint32_t serial, const char* text) {
    resource->sendCommitString(serial, text);
}

void CTextInputV1::deleteSurroundingText(int32_t index, uint32_t length) {
    resource->sendDeleteSurroundingText(index, length);
}

CTextInputV1Protocol::CTextInputV1Protocol(const wl_interface* iface, const int& ver, const std::string& name) : IWaylandProtocol(iface, ver, name) {
    ;
}

void CTextInputV1Protocol::bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id) {
    const auto RESOURCE = m_vManagers.emplace_back(makeShared<CZwpTextInputManagerV1>(client, ver, id));

    RESOURCE->setOnDestroy([this](CZwpTextInputManagerV1* pMgr) { PROTO::textInputV1->destroyResource(pMgr); });
    RESOURCE->setCreateTextInput([this](CZwpTextInputManagerV1* pMgr, uint32_t id) {
        const auto PTI = m_vClients.emplace_back(makeShared<CTextInputV1>(makeShared<CZwpTextInputV1>(pMgr->client(), pMgr->version(), id)));
        LOGM(LOG, "New TI V1 at {:x}", (uintptr_t)PTI.get());

        if (!PTI->good()) {
            LOGM(ERR, "Could not alloc wl_resource for TIV1");
            pMgr->noMemory();
            PROTO::textInputV1->destroyResource(PTI.get());
            return;
        }

        events.newTextInput.emit(WP<CTextInputV1>(PTI));
    });
}

void CTextInputV1Protocol::destroyResource(CTextInputV1* client) {
    std::erase_if(m_vClients, [&](const auto& other) { return other.get() == client; });
}

void CTextInputV1Protocol::destroyResource(CZwpTextInputManagerV1* client) {
    std::erase_if(m_vManagers, [&](const auto& other) { return other.get() == client; });
}
