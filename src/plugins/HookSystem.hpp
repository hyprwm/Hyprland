#pragma once

#include <string>
#include <vector>
#include <memory>

class CFunctionHook {
  public:
    CFunctionHook(void* source, void* destination);
    ~CFunctionHook();

    bool hook();
    bool unhook();

    CFunctionHook(const CFunctionHook&) = delete;
    CFunctionHook(CFunctionHook&&)      = delete;
    CFunctionHook& operator=(const CFunctionHook&) = delete;
    CFunctionHook& operator=(CFunctionHook&&) = delete;

    void*          m_pOriginal = nullptr;

  private:
    void*  m_pSource         = nullptr;
    void*  m_pFunctionAddr   = nullptr;
    void*  m_pTrampolineAddr = nullptr;
    void*  m_pDestination    = nullptr;
    size_t m_iHookLen        = 0;
    size_t m_iTrampoLen      = 0;

    bool   m_bActive = false;
};

class CHookSystem {
  public:
    CFunctionHook* initHook(void* source, void* destination);
    bool           removeHook(CFunctionHook* hook);

  private:
    std::vector<std::unique_ptr<CFunctionHook>> m_vHooks;
};

inline std::unique_ptr<CHookSystem> g_pFunctionHookSystem;