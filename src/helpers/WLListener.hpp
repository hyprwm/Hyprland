#pragma once

#include "../includes.hpp"
#include <functional>

class CHyprWLListener {
public:
    CHyprWLListener(wl_signal*, std::function<void(void*, void*)>, void* owner);
    CHyprWLListener();
    ~CHyprWLListener();

    void initCallback(wl_signal*, std::function<void(void*, void*)>, void* owner, std::string author = "");

    void removeCallback();

    bool isConnected();

    wl_listener m_sListener;

    void emit(void*);

private:
    bool            m_bIsConnected = false;

    void*           m_pOwner = nullptr;

    std::function<void(void*, void*)> m_pCallback = nullptr;

    std::string     m_szAuthor = "";
};