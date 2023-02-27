#pragma once

#include <string>
#include <vector>
#include <memory>

#define HANDLE void*

class CFunctionHook {
  public:
    CFunctionHook(HANDLE owner, void* source, void* destination);
    ~CFunctionHook();

    bool hook();
    bool unhook();

    CFunctionHook(const CFunctionHook&) = delete;
    CFunctionHook(CFunctionHook&&)      = delete;
    CFunctionHook& operator=(const CFunctionHook&) = delete;
    CFunctionHook& operator=(CFunctionHook&&) = delete;

    void*          m_pOriginal = nullptr;

  private:
    void*                                       m_pSource         = nullptr;
    void*                                       m_pFunctionAddr   = nullptr;
    void*                                       m_pTrampolineAddr = nullptr;
    void*                                       m_pDestination    = nullptr;
    size_t                                      m_iHookLen        = 0;
    size_t                                      m_iTrampoLen      = 0;
    HANDLE                                      m_pOwner          = nullptr;
    bool                                        m_bActive         = false;

    std::vector<std::pair<size_t, std::string>> m_vTrampolineRIPUses;

    void*                                       m_pOriginalBytes = nullptr;

    size_t                                      probeMinimumJumpSize(void* start, size_t min);
    size_t                                      getInstructionLenAt(void* start);

    friend class CHookSystem;
};

class CHookSystem {
  public:
    CFunctionHook* initHook(HANDLE handle, void* source, void* destination);
    bool           removeHook(CFunctionHook* hook);

    void           removeAllHooksFrom(HANDLE handle);

  private:
    std::vector<std::unique_ptr<CFunctionHook>> m_vHooks;
};

inline std::unique_ptr<CHookSystem> g_pFunctionHookSystem;