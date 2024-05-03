#include "Listener.hpp"
#include "Signal.hpp"

CSignalListener::CSignalListener(std::function<void(std::any)> handler) : m_fHandler(handler) {
    ;
}

void CSignalListener::emit(std::any data) {
    if (!m_fHandler)
        return;

    m_fHandler(data);
}

CStaticSignalListener::CStaticSignalListener(std::function<void(void*, std::any)> handler, void* owner) : m_pOwner(owner), m_fHandler(handler) {
    ;
}

void CStaticSignalListener::emit(std::any data) {
    m_fHandler(m_pOwner, data);
}
