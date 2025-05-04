#include "TextInputV1.hpp"

#include "core/Compositor.hpp"

CTextInputV1::~CTextInputV1() {
    m_events.destroy.emit();
}

CTextInputV1::CTextInputV1(SP<CZwpTextInputV1> resource_) : m_resource(resource_) {
    if UNLIKELY (!good())
        return;

    m_resource->setOnDestroy([this](CZwpTextInputV1* pMgr) { PROTO::textInputV1->destroyResource(this); });

    m_resource->setActivate([this](CZwpTextInputV1* pMgr, wl_resource* seat, wl_resource* surface) {
        if UNLIKELY (!surface) {
            LOGM(WARN, "Text-input-v1 PTI{:x}: No surface to activate text input on!", (uintptr_t)this);
            return;
        }

        m_active = true;
        m_events.enable.emit(CWLSurfaceResource::fromResource(surface));
    });

    m_resource->setDeactivate([this](CZwpTextInputV1* pMgr, wl_resource* seat) {
        m_active = false;
        m_events.disable.emit();
    });

    m_resource->setReset([this](CZwpTextInputV1* pMgr) {
        m_pendingSurrounding.isPending = false;
        m_pendingContentType.isPending = false;
        m_events.reset.emit();
    });

    m_resource->setSetSurroundingText(
        [this](CZwpTextInputV1* pMgr, const char* text, uint32_t cursor, uint32_t anchor) { m_pendingSurrounding = {true, std::string(text), cursor, anchor}; });

    m_resource->setSetContentType([this](CZwpTextInputV1* pMgr, uint32_t hint, uint32_t purpose) {
        m_pendingContentType = {true, hint == (uint32_t)ZWP_TEXT_INPUT_V1_CONTENT_HINT_DEFAULT ? (uint32_t)ZWP_TEXT_INPUT_V1_CONTENT_HINT_NONE : hint,
                                purpose > (uint32_t)ZWP_TEXT_INPUT_V1_CONTENT_PURPOSE_PASSWORD ? hint + 1 : hint};
    });

    m_resource->setSetCursorRectangle([this](CZwpTextInputV1* pMgr, int32_t x, int32_t y, int32_t width, int32_t height) { m_cursorRectangle = CBox{x, y, width, height}; });

    m_resource->setCommitState([this](CZwpTextInputV1* pMgr, uint32_t serial_) {
        m_serial = serial_;
        m_events.onCommit.emit();
    });

    // nothing
    m_resource->setShowInputPanel([](CZwpTextInputV1* pMgr) {});
    m_resource->setHideInputPanel([](CZwpTextInputV1* pMgr) {});
    m_resource->setSetPreferredLanguage([](CZwpTextInputV1* pMgr, const char* language) {});
    m_resource->setInvokeAction([](CZwpTextInputV1* pMgr, uint32_t button, uint32_t index) {});
}

bool CTextInputV1::good() {
    return m_resource->resource();
}

wl_client* CTextInputV1::client() {
    return m_resource->client();
}

void CTextInputV1::enter(SP<CWLSurfaceResource> surface) {
    m_resource->sendEnter(surface->getResource()->resource());
    m_active = true;
}

void CTextInputV1::leave() {
    m_resource->sendLeave();
    m_active = false;
}

void CTextInputV1::preeditCursor(int32_t index) {
    m_resource->sendPreeditCursor(index);
}

void CTextInputV1::preeditStyling(uint32_t index, uint32_t length, zwpTextInputV1PreeditStyle style) {
    m_resource->sendPreeditStyling(index, length, style);
}

void CTextInputV1::preeditString(uint32_t serial, const char* text, const char* commit) {
    m_resource->sendPreeditString(serial, text, commit);
}

void CTextInputV1::commitString(uint32_t serial, const char* text) {
    m_resource->sendCommitString(serial, text);
}

void CTextInputV1::deleteSurroundingText(int32_t index, uint32_t length) {
    m_resource->sendDeleteSurroundingText(index, length);
}

CTextInputV1Protocol::CTextInputV1Protocol(const wl_interface* iface, const int& ver, const std::string& name) : IWaylandProtocol(iface, ver, name) {
    ;
}

void CTextInputV1Protocol::bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id) {
    const auto RESOURCE = m_managers.emplace_back(makeShared<CZwpTextInputManagerV1>(client, ver, id));

    RESOURCE->setOnDestroy([](CZwpTextInputManagerV1* pMgr) { PROTO::textInputV1->destroyResource(pMgr); });
    RESOURCE->setCreateTextInput([this](CZwpTextInputManagerV1* pMgr, uint32_t id) {
        const auto PTI = m_clients.emplace_back(makeShared<CTextInputV1>(makeShared<CZwpTextInputV1>(pMgr->client(), pMgr->version(), id)));
        LOGM(LOG, "New TI V1 at {:x}", (uintptr_t)PTI.get());

        if UNLIKELY (!PTI->good()) {
            LOGM(ERR, "Could not alloc wl_resource for TIV1");
            pMgr->noMemory();
            PROTO::textInputV1->destroyResource(PTI.get());
            return;
        }

        m_events.newTextInput.emit(WP<CTextInputV1>(PTI));
    });
}

void CTextInputV1Protocol::destroyResource(CTextInputV1* client) {
    std::erase_if(m_clients, [&](const auto& other) { return other.get() == client; });
}

void CTextInputV1Protocol::destroyResource(CZwpTextInputManagerV1* client) {
    std::erase_if(m_managers, [&](const auto& other) { return other.get() == client; });
}
