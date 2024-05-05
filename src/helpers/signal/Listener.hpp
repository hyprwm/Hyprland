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

class CStaticSignalListener {
  public:
    CStaticSignalListener(std::function<void(void*, std::any)> handler, void* owner);

    CStaticSignalListener(CStaticSignalListener&&)       = delete;
    CStaticSignalListener(CStaticSignalListener&)        = delete;
    CStaticSignalListener(const CStaticSignalListener&)  = delete;
    CStaticSignalListener(const CStaticSignalListener&&) = delete;

    void emit(std::any data);

  private:
    void*                                m_pOwner = nullptr;
    std::function<void(void*, std::any)> m_fHandler;
};