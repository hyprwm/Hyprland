#include "TextInputV1.hpp"

#include "../Compositor.hpp"

#define TEXT_INPUT_VERSION 1

static void bindManagerInt(wl_client* client, void* data, uint32_t version, uint32_t id) {
    g_pProtocolManager->m_pFractionalScaleProtocolManager->bindManager(client, data, version, id);
}

static void handleDisplayDestroy(struct wl_listener* listener, void* data) {
    g_pProtocolManager->m_pFractionalScaleProtocolManager->displayDestroy();
}

void CTextInputV1ProtocolManager::displayDestroy() {
    wl_global_destroy(m_pGlobal);
}

CTextInputV1ProtocolManager::CTextInputV1ProtocolManager() {
    m_pGlobal = wl_global_create(g_pCompositor->m_sWLDisplay, &zwp_text_input_manager_v1_interface, TEXT_INPUT_VERSION, this, bindManagerInt);

    if (!m_pGlobal) {
        Debug::log(ERR, "TextInputV1Manager could not start!");
        return;
    }

    m_liDisplayDestroy.notify = handleDisplayDestroy;
    wl_display_add_destroy_listener(g_pCompositor->m_sWLDisplay, &m_liDisplayDestroy);

    Debug::log(LOG, "TextInputV1Manager started successfully!");
}

static void createTI(wl_client* client, wl_resource* resource, uint32_t id) {
    g_pProtocolManager->m_pTextInputV1ProtocolManager->createTI(client, resource, id);
}

static const struct zwp_text_input_manager_v1_interface textInputManagerImpl = {
    .create_text_input = &createTI,
};

void CTextInputV1ProtocolManager::bindManager(wl_client* client, void* data, uint32_t version, uint32_t id) {
    const auto RESOURCE = wl_resource_create(client, &zwp_text_input_manager_v1_interface, version, id);
    wl_resource_set_implementation(RESOURCE, &textInputManagerImpl, this, nullptr);

    Debug::log(LOG, "TextInputV1Manager bound successfully!");
}

//

void handleActivate(wl_client* client, wl_resource* resource, wl_resource* seat, wl_resource* surface) {
    g_pProtocolManager->m_pTextInputV1ProtocolManager->handleActivate(client, resource, seat, surface);
}

void handleDeactivate(wl_client* client, wl_resource* resource, wl_resource* seat) {
    g_pProtocolManager->m_pTextInputV1ProtocolManager->handleDeactivate(client, resource, seat);
}

void handleShowInputPanel(wl_client* client, wl_resource* resource) {
    g_pProtocolManager->m_pTextInputV1ProtocolManager->handleShowInputPanel(client, resource);
}

void handleHideInputPanel(wl_client* client, wl_resource* resource) {
    g_pProtocolManager->m_pTextInputV1ProtocolManager->handleHideInputPanel(client, resource);
}

void handleReset(wl_client* client, wl_resource* resource) {
    g_pProtocolManager->m_pTextInputV1ProtocolManager->handleReset(client, resource);
}

void handleSetSurroundingText(wl_client* client, wl_resource* resource, const char* text, uint32_t cursor, uint32_t anchor) {
    g_pProtocolManager->m_pTextInputV1ProtocolManager->handleSetSurroundingText(client, resource, text, cursor, anchor);
}

void handleSetContentType(wl_client* client, wl_resource* resource, uint32_t hint, uint32_t purpose) {
    g_pProtocolManager->m_pTextInputV1ProtocolManager->handleSetContentType(client, resource, hint, purpose);
}

void handleSetCursorRectangle(wl_client* client, wl_resource* resource, int32_t x, int32_t y, int32_t width, int32_t height) {
    g_pProtocolManager->m_pTextInputV1ProtocolManager->handleSetCursorRectangle(client, resource, x, y, width, height);
}

void handleSetPreferredLanguage(wl_client* client, wl_resource* resource, const char* language) {
    g_pProtocolManager->m_pTextInputV1ProtocolManager->handleSetPreferredLanguage(client, resource, language);
}

void handleCommitState(wl_client* client, wl_resource* resource, uint32_t serial) {
    g_pProtocolManager->m_pTextInputV1ProtocolManager->handleCommitState(client, resource, serial);
}

void handleInvokeAction(wl_client* client, wl_resource* resource, uint32_t button, uint32_t index) {
    g_pProtocolManager->m_pTextInputV1ProtocolManager->handleInvokeAction(client, resource, button, index);
}

static const struct zwp_text_input_v1_interface textInputImpl = {
    .activate               = &handleActivate,
    .deactivate             = &handleDeactivate,
    .show_input_panel       = &handleShowInputPanel,
    .hide_input_panel       = &handleHideInputPanel,
    .reset                  = &handleReset,
    .set_surrounding_text   = &handleSetSurroundingText,
    .set_content_type       = &handleSetContentType,
    .set_cursor_rectangle   = &handleSetCursorRectangle,
    .set_preferred_language = &handleSetPreferredLanguage,
    .commit_state           = &handleCommitState,
    .invoke_action          = &handleInvokeAction,
};

void CTextInputV1ProtocolManager::removeTI(STextInputV1* pTI) {
    const auto TI = std::find_if(m_pClients.begin(), m_pClients.end(), [&](const auto& other) { return other->resourceCaller == pTI->resourceCaller; });
    if (TI == m_pClients.end())
        return;

    if ((*TI)->resourceImpl)
        wl_resource_destroy((*TI)->resourceImpl);

    std::erase_if(m_pClients, [&](const auto& other) { return other.get() == pTI; });
}

STextInputV1* tiFromResource(wl_resource* resource) {
    ASSERT(wl_resource_instance_of(resource, &zwp_text_input_v1_interface, &textInputImpl));
    return (STextInputV1*)wl_resource_get_user_data(resource);
}

static void destroyTI(wl_resource* resource) {
    const auto TI = tiFromResource(resource);

    if (TI->resourceImpl) {
        wl_resource_set_user_data(resource, nullptr);
    }

    TI->pTextInput->hyprListener_textInputDestroy.emit(nullptr);

    g_pProtocolManager->m_pTextInputV1ProtocolManager->removeTI(TI);
}

void CTextInputV1ProtocolManager::createTI(wl_client* client, wl_resource* resource, uint32_t id) {
    const auto PTI = m_pClients.emplace_back(std::make_unique<STextInputV1>()).get();
    Debug::log(LOG, "New TI V1 at %x", PTI);

    PTI->client         = client;
    PTI->resourceCaller = resource;
    PTI->resourceImpl   = wl_resource_create(client, &zwp_text_input_v1_interface, TEXT_INPUT_VERSION, id);

    if (!PTI->resourceImpl) {
        Debug::log(ERR, "Could not alloc wl_resource for TIV1");
        removeTI(PTI);
        return;
    }

    wl_resource_set_implementation(PTI->resourceImpl, &textInputImpl, PTI, &destroyTI);
}

void CTextInputV1ProtocolManager::handleActivate(wl_client* client, wl_resource* resource, wl_resource* seat, wl_resource* surface) {
    const auto PTI = tiFromResource(resource);
    PTI->pTextInput->hyprListener_textInputEnable.emit(nullptr);
}

void CTextInputV1ProtocolManager::handleDeactivate(wl_client* client, wl_resource* resource, wl_resource* seat) {
    const auto PTI = tiFromResource(resource);
    PTI->pTextInput->hyprListener_textInputDisable.emit(nullptr);
}

void CTextInputV1ProtocolManager::handleShowInputPanel(wl_client* client, wl_resource* resource) {
    ;
}

void CTextInputV1ProtocolManager::handleHideInputPanel(wl_client* client, wl_resource* resource) {
    ;
}

void CTextInputV1ProtocolManager::handleReset(wl_client* client, wl_resource* resource) {
    const auto PTI                    = tiFromResource(resource);
    PTI->pendingSurrounding.isPending = false;
}

void CTextInputV1ProtocolManager::handleSetSurroundingText(wl_client* client, wl_resource* resource, const char* text, uint32_t cursor, uint32_t anchor) {
    const auto PTI          = tiFromResource(resource);
    PTI->pendingSurrounding = {true, std::string(text), cursor, anchor};
}

void CTextInputV1ProtocolManager::handleSetContentType(wl_client* client, wl_resource* resource, uint32_t hint, uint32_t purpose) {
    const auto PTI          = tiFromResource(resource);
    PTI->pendingContentType = {true, hint == ZWP_TEXT_INPUT_V1_CONTENT_HINT_DEFAULT ? ZWP_TEXT_INPUT_V1_CONTENT_HINT_NONE : hint,
                               purpose > ZWP_TEXT_INPUT_V1_CONTENT_PURPOSE_PASSWORD ? hint + 1 : hint};
}

void CTextInputV1ProtocolManager::handleSetCursorRectangle(wl_client* client, wl_resource* resource, int32_t x, int32_t y, int32_t width, int32_t height) {
    const auto PTI       = tiFromResource(resource);
    PTI->cursorRectangle = wlr_box{x, y, width, height};
}

void CTextInputV1ProtocolManager::handleSetPreferredLanguage(wl_client* client, wl_resource* resource, const char* language) {
    ;
}

void CTextInputV1ProtocolManager::handleCommitState(wl_client* client, wl_resource* resource, uint32_t serial) {
    const auto PTI = tiFromResource(resource);
    PTI->pTextInput->hyprListener_textInputCommit.emit(nullptr);
}

void CTextInputV1ProtocolManager::handleInvokeAction(wl_client* client, wl_resource* resource, uint32_t button, uint32_t index) {
    ;
}