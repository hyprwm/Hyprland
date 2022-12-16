#pragma once

#include "../includes.hpp"
#include <functional>

class CHyprWLListener {
  public:
    CHyprWLListener(wl_signal*, std::function<void(void*, void*)>, void* owner);
    CHyprWLListener();
    ~CHyprWLListener();

    CHyprWLListener(const CHyprWLListener&) = delete;
    CHyprWLListener(CHyprWLListener&&)      = delete;
    CHyprWLListener& operator=(const CHyprWLListener&) = delete;
    CHyprWLListener& operator=(CHyprWLListener&&) = delete;

    void             initCallback(wl_signal*, std::function<void(void*, void*)>, void* owner, std::string author = "");

    void             removeCallback();

    bool             isConnected();

    struct SWrapper {
        wl_listener      m_sListener;
        CHyprWLListener* m_pSelf;
    };

    void emit(void*);

  private:
    SWrapper                          m_swWrapper;

    void*                             m_pOwner = nullptr;

    std::function<void(void*, void*)> m_pCallback = nullptr;

    std::string                       m_szAuthor = "";
};