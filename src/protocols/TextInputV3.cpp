#include "TextInputV3.hpp"
#include <algorithm>
#include "core/Compositor.hpp"

void CTextInputV3::SState::reset() {
    cause               = ZWP_TEXT_INPUT_V3_CHANGE_CAUSE_INPUT_METHOD;
    surrounding.updated = false;
    contentType.updated = false;
    box.updated         = false;
}

CTextInputV3::CTextInputV3(SP<CZwpTextInputV3> resource_) : m_resource(resource_) {
    if UNLIKELY (!m_resource->resource())
        return;

    LOGM(LOG, "New tiv3 at {:016x}", (uintptr_t)this);

    m_resource->setDestroy([this](CZwpTextInputV3* r) { PROTO::textInputV3->destroyTextInput(this); });
    m_resource->setOnDestroy([this](CZwpTextInputV3* r) { PROTO::textInputV3->destroyTextInput(this); });

    m_resource->setCommit([this](CZwpTextInputV3* r) {
        bool wasEnabled = m_current.enabled.value;

        m_current = m_pending;
        m_serial++;

        if (wasEnabled && !m_current.enabled.value)
            m_events.disable.emit();
        else if (!wasEnabled && m_current.enabled.value)
            m_events.enable.emit();
        else if (m_current.enabled.value && m_current.enabled.isEnablePending && m_current.enabled.isDisablePending)
            m_events.reset.emit();
        else
            m_events.onCommit.emit();

        m_pending.enabled.isEnablePending  = false;
        m_pending.enabled.isDisablePending = false;
    });

    m_resource->setSetSurroundingText([this](CZwpTextInputV3* r, const char* text, int32_t cursor, int32_t anchor) {
        m_pending.surrounding.updated = true;
        m_pending.surrounding.anchor  = anchor;
        m_pending.surrounding.cursor  = cursor;
        m_pending.surrounding.text    = text;
    });

    m_resource->setSetTextChangeCause([this](CZwpTextInputV3* r, zwpTextInputV3ChangeCause cause) { m_pending.cause = cause; });

    m_resource->setSetContentType([this](CZwpTextInputV3* r, zwpTextInputV3ContentHint hint, zwpTextInputV3ContentPurpose purpose) {
        m_pending.contentType.updated = true;
        m_pending.contentType.hint    = hint;
        m_pending.contentType.purpose = purpose;
    });

    m_resource->setSetCursorRectangle([this](CZwpTextInputV3* r, int32_t x, int32_t y, int32_t w, int32_t h) {
        m_pending.box.updated   = true;
        m_pending.box.cursorBox = {x, y, w, h};
    });

    m_resource->setEnable([this](CZwpTextInputV3* r) {
        m_pending.reset();
        m_pending.enabled.value           = true;
        m_pending.enabled.isEnablePending = true;
    });

    m_resource->setDisable([this](CZwpTextInputV3* r) {
        m_pending.enabled.value            = false;
        m_pending.enabled.isDisablePending = true;
    });
}

CTextInputV3::~CTextInputV3() {
    m_events.destroy.emit();
}

void CTextInputV3::enter(SP<CWLSurfaceResource> surf) {
    m_resource->sendEnter(surf->getResource()->resource());
}

void CTextInputV3::leave(SP<CWLSurfaceResource> surf) {
    m_resource->sendLeave(surf->getResource()->resource());
}

void CTextInputV3::preeditString(const std::string& text, int32_t cursorBegin, int32_t cursorEnd) {
    m_resource->sendPreeditString(text.c_str(), cursorBegin, cursorEnd);
}

void CTextInputV3::commitString(const std::string& text) {
    m_resource->sendCommitString(text.c_str());
}

void CTextInputV3::deleteSurroundingText(uint32_t beforeLength, uint32_t afterLength) {
    m_resource->sendDeleteSurroundingText(beforeLength, afterLength);
}

void CTextInputV3::sendDone() {
    m_resource->sendDone(m_serial);
}

bool CTextInputV3::good() {
    return m_resource->resource();
}

wl_client* CTextInputV3::client() {
    return wl_resource_get_client(m_resource->resource());
}

CTextInputV3Protocol::CTextInputV3Protocol(const wl_interface* iface, const int& ver, const std::string& name) : IWaylandProtocol(iface, ver, name) {
    ;
}

void CTextInputV3Protocol::bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id) {
    const auto RESOURCE = m_managers.emplace_back(makeUnique<CZwpTextInputManagerV3>(client, ver, id)).get();
    RESOURCE->setOnDestroy([this](CZwpTextInputManagerV3* p) { this->onManagerResourceDestroy(p->resource()); });

    RESOURCE->setDestroy([this](CZwpTextInputManagerV3* pMgr) { this->onManagerResourceDestroy(pMgr->resource()); });
    RESOURCE->setGetTextInput([this](CZwpTextInputManagerV3* pMgr, uint32_t id, wl_resource* seat) { this->onGetTextInput(pMgr, id, seat); });
}

void CTextInputV3Protocol::onManagerResourceDestroy(wl_resource* res) {
    std::erase_if(m_managers, [&](const auto& other) { return other->resource() == res; });
}

void CTextInputV3Protocol::destroyTextInput(CTextInputV3* input) {
    std::erase_if(m_textInputs, [&](const auto& other) { return other.get() == input; });
}

void CTextInputV3Protocol::onGetTextInput(CZwpTextInputManagerV3* pMgr, uint32_t id, wl_resource* seat) {
    const auto CLIENT   = pMgr->client();
    const auto RESOURCE = m_textInputs.emplace_back(makeShared<CTextInputV3>(makeShared<CZwpTextInputV3>(CLIENT, pMgr->version(), id)));

    if UNLIKELY (!RESOURCE->good()) {
        pMgr->noMemory();
        m_textInputs.pop_back();
        LOGM(ERR, "Failed to create a tiv3 resource");
        return;
    }

    m_events.newTextInput.emit(WP<CTextInputV3>(RESOURCE));
}