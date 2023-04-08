#include "TextInputV1.hpp"

#include "../Compositor.hpp"

#define TEXT_INPUT_VERSION 1

static void bindManagerInt(wl_client* client, void* data, uint32_t version, uint32_t id) {
    g_pProtocolManager->m_pTextInputV1ProtocolManager->bindManager(client, data, version, id);
}

static void handleDisplayDestroy(struct wl_listener* listener, void* data) {
    g_pProtocolManager->m_pTextInputV1ProtocolManager->displayDestroy();
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
    .create_text_input = createTI,
};

void CTextInputV1ProtocolManager::bindManager(wl_client* client, void* data, uint32_t version, uint32_t id) {
    const auto RESOURCE = wl_resource_create(client, &zwp_text_input_manager_v1_interface, version, id);
    wl_resource_set_implementation(RESOURCE, &textInputManagerImpl, this, nullptr);

    Debug::log(LOG, "TextInputV1Manager bound successfully!");
}

//

static void handleActivate(wl_client* client, wl_resource* resource, wl_resource* seat, wl_resource* surface) {
    g_pProtocolManager->m_pTextInputV1ProtocolManager->handleActivate(client, resource, seat, surface);
}

static void handleDeactivate(wl_client* client, wl_resource* resource, wl_resource* seat) {
    g_pProtocolManager->m_pTextInputV1ProtocolManager->handleDeactivate(client, resource, seat);
}

static void handleShowInputPanel(wl_client* client, wl_resource* resource) {
    g_pProtocolManager->m_pTextInputV1ProtocolManager->handleShowInputPanel(client, resource);
}

static void handleHideInputPanel(wl_client* client, wl_resource* resource) {
    g_pProtocolManager->m_pTextInputV1ProtocolManager->handleHideInputPanel(client, resource);
}

static void handleReset(wl_client* client, wl_resource* resource) {
    g_pProtocolManager->m_pTextInputV1ProtocolManager->handleReset(client, resource);
}

static void handleSetSurroundingText(wl_client* client, wl_resource* resource, const char* text, uint32_t cursor, uint32_t anchor) {
    g_pProtocolManager->m_pTextInputV1ProtocolManager->handleSetSurroundingText(client, resource, text, cursor, anchor);
}

static void handleSetContentType(wl_client* client, wl_resource* resource, uint32_t hint, uint32_t purpose) {
    g_pProtocolManager->m_pTextInputV1ProtocolManager->handleSetContentType(client, resource, hint, purpose);
}

static void handleSetCursorRectangle(wl_client* client, wl_resource* resource, int32_t x, int32_t y, int32_t width, int32_t height) {
    g_pProtocolManager->m_pTextInputV1ProtocolManager->handleSetCursorRectangle(client, resource, x, y, width, height);
}

static void handleSetPreferredLanguage(wl_client* client, wl_resource* resource, const char* language) {
    g_pProtocolManager->m_pTextInputV1ProtocolManager->handleSetPreferredLanguage(client, resource, language);
}

static void handleCommitState(wl_client* client, wl_resource* resource, uint32_t serial) {
    g_pProtocolManager->m_pTextInputV1ProtocolManager->handleCommitState(client, resource, serial);
}

static void handleInvokeAction(wl_client* client, wl_resource* resource, uint32_t button, uint32_t index) {
    g_pProtocolManager->m_pTextInputV1ProtocolManager->handleInvokeAction(client, resource, button, index);
}

static const struct zwp_text_input_v1_interface textInputImpl = {
    .activate               = handleActivate,
    .deactivate             = handleDeactivate,
    .show_input_panel       = handleShowInputPanel,
    .hide_input_panel       = handleHideInputPanel,
    .reset                  = handleReset,
    .set_surrounding_text   = handleSetSurroundingText,
    .set_content_type       = handleSetContentType,
    .set_cursor_rectangle   = handleSetCursorRectangle,
    .set_preferred_language = handleSetPreferredLanguage,
    .commit_state           = handleCommitState,
    .invoke_action          = handleInvokeAction,
};

void CTextInputV1ProtocolManager::removeTI(STextInputV1* pTI) {
    const auto TI = std::find_if(m_pClients.begin(), m_pClients.end(), [&](const auto& other) { return other->resourceCaller == pTI->resourceCaller; });
    if (TI == m_pClients.end())
        return;

    // if ((*TI)->resourceImpl)
    //  wl_resource_destroy((*TI)->resourceImpl);

    g_pInputManager->m_sIMERelay.removeTextInput((*TI)->pTextInput);

    std::erase_if(m_pClients, [&](const auto& other) { return other.get() == pTI; });
}

STextInputV1* tiFromResource(wl_resource* resource) {
    ASSERT(wl_resource_instance_of(resource, &zwp_text_input_v1_interface, &textInputImpl));
    return (STextInputV1*)wl_resource_get_user_data(resource);
}

static void destroyTI(wl_resource* resource) {
    const auto TI = tiFromResource(resource);

    if (!TI)
        return;

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
    wl_resource_set_user_data(PTI->resourceImpl, PTI);

    wl_signal_init(&PTI->sEnable);
    wl_signal_init(&PTI->sDisable);
    wl_signal_init(&PTI->sDestroy);
    wl_signal_init(&PTI->sCommit);

    g_pInputManager->m_sIMERelay.createNewTextInput(nullptr, PTI);
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
    PTI->pendingContentType.isPending = false;
}

void CTextInputV1ProtocolManager::handleSetSurroundingText(wl_client* client, wl_resource* resource, const char* text, uint32_t cursor, uint32_t anchor) {
    const auto PTI          = tiFromResource(resource);
    PTI->pendingSurrounding = {true, std::string(text), cursor, anchor};
}

void CTextInputV1ProtocolManager::handleSetContentType(wl_client* client, wl_resource* resource, uint32_t hint, uint32_t purpose) {
    const auto PTI          = tiFromResource(resource);
    PTI->pendingContentType = {true, hint == (uint32_t)ZWP_TEXT_INPUT_V1_CONTENT_HINT_DEFAULT ? (uint32_t)ZWP_TEXT_INPUT_V1_CONTENT_HINT_NONE : hint,
                               purpose > (uint32_t)ZWP_TEXT_INPUT_V1_CONTENT_PURPOSE_PASSWORD ? hint + 1 : hint};
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
    PTI->serial    = serial;
    PTI->pTextInput->hyprListener_textInputCommit.emit(nullptr);
}

void CTextInputV1ProtocolManager::handleInvokeAction(wl_client* client, wl_resource* resource, uint32_t button, uint32_t index) {
    ;
}