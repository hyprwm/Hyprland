#pragma once

#include <functional>
#include <any>
#include <vector>
#include <memory>

#include "Listener.hpp"

class CSignal {
  public:
    void emit(std::any data = {});

    //
    [[nodiscard("Listener is unregistered when the ptr is lost")]] CHyprSignalListener registerListener(std::function<void(std::any)> handler);

    // this is for static listeners. They die with this signal.
    // TODO: can we somehow rid of the void* data and make it a custom this?
    void registerStaticListener(std::function<void(void*, std::any)> handler, void* owner);

  private:
    std::vector<std::weak_ptr<CSignalListener>>         m_vListeners;
    std::vector<std::unique_ptr<CStaticSignalListener>> m_vStaticListeners;
};
