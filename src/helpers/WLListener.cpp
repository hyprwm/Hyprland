#include "WLListener.hpp"
#include "MiscFunctions.hpp"
#include <string>
#include "../debug/Log.hpp"

void handleWrapped(wl_listener* listener, void* data) {
    CHyprWLListener* pListener = wl_container_of(listener, pListener, m_sListener);

    pListener->emit(data);
}

CHyprWLListener::CHyprWLListener(wl_signal* pSignal, std::function<void(void*, void*)> callback, void* pOwner) {
    initCallback(pSignal, callback, pOwner);
}

CHyprWLListener::CHyprWLListener() {
    ; //
}

CHyprWLListener::~CHyprWLListener() {
    removeCallback();
}

void CHyprWLListener::removeCallback() {
    if (m_bIsConnected) {
        Debug::log(LOG, "Callback %x -> %x, %s removed.", &m_pCallback, &m_pOwner, m_szAuthor.c_str());
        wl_list_remove(&m_sListener.link);
        wl_list_init(&m_sListener.link);
    }

    m_bIsConnected = false;
}

bool CHyprWLListener::isConnected() {
    return m_bIsConnected;
}

void CHyprWLListener::initCallback(wl_signal* pSignal, std::function<void(void*, void*)> callback, void* pOwner, std::string author) {
    if (m_bIsConnected)
        removeCallback();

    m_pOwner = pOwner;
    m_pCallback = callback;
    m_szAuthor = author;

    m_sListener.notify = &handleWrapped;

    m_bIsConnected = true;

    wl_list_init(&m_sListener.link);
    addWLSignal(pSignal, &m_sListener, pOwner, m_szAuthor);
}

void CHyprWLListener::emit(void* data) {
    m_pCallback(m_pOwner, data);
}