#pragma once

#include <functional>
#include <any>
#include <vector>
#include <memory>

#include "Listener.hpp"

class CSignal {
  public:
    void emit(std::any data);

    //
    [[nodiscard("Listener is unregistered when the ptr is lost")]] CHyprSignalListener registerListener(std::function<void(std::any)> handler);

  private:
    std::vector<std::weak_ptr<CSignalListener>> m_vListeners;
};
