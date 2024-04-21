#pragma once

#include <any>
#include <memory>
#include <functional>

class CSignal;

class CSignalListener {
  public:
    CSignalListener(std::function<void(std::any)> handler);

    CSignalListener(CSignalListener&&)       = delete;
    CSignalListener(CSignalListener&)        = delete;
    CSignalListener(const CSignalListener&)  = delete;
    CSignalListener(const CSignalListener&&) = delete;

    void emit(std::any data);

  private:
    std::function<void(std::any)> m_fHandler;
};

typedef std::shared_ptr<CSignalListener> CHyprSignalListener;
