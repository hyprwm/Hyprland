#include "TextInputV3.hpp"
#include <algorithm>
#include "core/Compositor.hpp"

void CTextInputV3::SState::reset() {
    cause               = ZWP_TEXT_INPUT_V3_CHANGE_CAUSE_INPUT_METHOD;
    surrounding.updated = false;
    contentType.updated = false;
    box.updated         = false;
}

CTextInputV3::CTextInputV3(SP<CZwpTextInputV3> resource_) : resource(resource_) {
    if (!resource->resource())
        return;

    LOGM(LOG, "New tiv3 at {:016x}", (uintptr_t)this);

    resource->setDestroy([this](CZwpTextInputV3* r) { PROTO::textInputV3->destroyTextInput(this); });
    resource->setOnDestroy([this](CZwpTextInputV3* r) { PROTO::textInputV3->destroyTextInput(this); });

    resource->setCommit([this](CZwpTextInputV3* r) {
        bool wasEnabled = current.enabled;

        current = pending;
        serial++;

        if (wasEnabled && !current.enabled) {
            current.reset();
            events.disable.emit();
        } else if (!wasEnabled && current.enabled)
            events.enable.emit();
        else
            events.onCommit.emit();
    });

    resource->setSetSurroundingText([this](CZwpTextInputV3* r, const char* text, int32_t cursor, int32_t anchor) {
        pending.surrounding.updated = true;
        pending.surrounding.anchor  = anchor;
        pending.surrounding.cursor  = cursor;
        pending.surrounding.text    = text;
    });

    resource->setSetTextChangeCause([this](CZwpTextInputV3* r, zwpTextInputV3ChangeCause cause) { pending.cause = cause; });

    resource->setSetContentType([this](CZwpTextInputV3* r, zwpTextInputV3ContentHint hint, zwpTextInputV3ContentPurpose purpose) {
        pending.contentType.updated = true;
        pending.contentType.hint    = hint;
        pending.contentType.purpose = purpose;
    });

    resource->setSetCursorRectangle([this](CZwpTextInputV3* r, int32_t x, int32_t y, int32_t w, int32_t h) {
        pending.box.updated   = true;
        pending.box.cursorBox = {x, y, w, h};
    });

    resource->setEnable([this](CZwpTextInputV3* r) { pending.enabled = true; });

    resource->setDisable([this](CZwpTextInputV3* r) {
        pending.enabled = false;
        pending.reset();
    });
}

CTextInputV3::~CTextInputV3() {
    events.destroy.emit();
}

void CTextInputV3::enter(SP<CWLSurfaceResource> surf) {
    resource->sendEnter(surf->getResource()->resource());
}

void CTextInputV3::leave(SP<CWLSurfaceResource> surf) {
    resource->sendLeave(surf->getResource()->resource());
}

void CTextInputV3::preeditString(const std::string& text, int32_t cursorBegin, int32_t cursorEnd) {
    resource->sendPreeditString(text.c_str(), cursorBegin, cursorEnd);
}

void CTextInputV3::commitString(const std::string& text) {
    resource->sendCommitString(text.c_str());
}

void CTextInputV3::deleteSurroundingText(uint32_t beforeLength, uint32_t afterLength) {
    resource->sendDeleteSurroundingText(beforeLength, afterLength);
}

void CTextInputV3::sendDone() {
    resource->sendDone(serial);
}

bool CTextInputV3::good() {
    return resource->resource();
}

wl_client* CTextInputV3::client() {
    return wl_resource_get_client(resource->resource());
}

CTextInputV3Protocol::CTextInputV3Protocol(const wl_interface* iface, const int& ver, const std::string& name) : IWaylandProtocol(iface, ver, name) {
    ;
}

void CTextInputV3Protocol::bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id) {
    const auto RESOURCE = m_vManagers.emplace_back(std::make_unique<CZwpTextInputManagerV3>(client, ver, id)).get();
    RESOURCE->setOnDestroy([this](CZwpTextInputManagerV3* p) { this->onManagerResourceDestroy(p->resource()); });

    RESOURCE->setDestroy([this](CZwpTextInputManagerV3* pMgr) { this->onManagerResourceDestroy(pMgr->resource()); });
    RESOURCE->setGetTextInput([this](CZwpTextInputManagerV3* pMgr, uint32_t id, wl_resource* seat) { this->onGetTextInput(pMgr, id, seat); });
}

void CTextInputV3Protocol::onManagerResourceDestroy(wl_resource* res) {
    std::erase_if(m_vManagers, [&](const auto& other) { return other->resource() == res; });
}

void CTextInputV3Protocol::destroyTextInput(CTextInputV3* input) {
    std::erase_if(m_vTextInputs, [&](const auto& other) { return other.get() == input; });
}

void CTextInputV3Protocol::onGetTextInput(CZwpTextInputManagerV3* pMgr, uint32_t id, wl_resource* seat) {
    const auto CLIENT   = pMgr->client();
    const auto RESOURCE = m_vTextInputs.emplace_back(makeShared<CTextInputV3>(makeShared<CZwpTextInputV3>(CLIENT, pMgr->version(), id)));

    if (!RESOURCE->good()) {
        pMgr->noMemory();
        m_vTextInputs.pop_back();
        LOGM(ERR, "Failed to create a tiv3 resource");
        return;
    }

    events.newTextInput.emit(WP<CTextInputV3>(RESOURCE));
}