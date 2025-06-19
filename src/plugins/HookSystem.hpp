#pragma once

#include <string>
#include <vector>
#include <cstddef>
#include "../helpers/memory/Memory.hpp"

#define HANDLE                   void*
#define HOOK_TRAMPOLINE_MAX_SIZE 64

class CFunctionHook {
  public:
    CFunctionHook(HANDLE owner, void* source, void* destination);
    ~CFunctionHook();

    bool hook();
    bool unhook();

    CFunctionHook(const CFunctionHook&)            = delete;
    CFunctionHook(CFunctionHook&&)                 = delete;
    CFunctionHook& operator=(const CFunctionHook&) = delete;
    CFunctionHook& operator=(CFunctionHook&&)      = delete;

    void*          m_original = nullptr;

  private:
    void*                      m_source         = nullptr;
    void*                      m_trampolineAddr = nullptr;
    void*                      m_destination    = nullptr;
    size_t                     m_hookLen        = 0;
    size_t                     m_trampoLen      = 0;
    HANDLE                     m_owner          = nullptr;
    bool                       m_active         = false;

    std::vector<unsigned char> m_originalBytes;

    struct SInstructionProbe {
        size_t              len      = 0;
        std::string         assembly = "";
        std::vector<size_t> insSizes;
    };

    struct SAssembly {
        std::vector<char> bytes;
    };

    SInstructionProbe probeMinimumJumpSize(void* start, size_t min);
    SInstructionProbe getInstructionLenAt(void* start);

    SAssembly         fixInstructionProbeRIPCalls(const SInstructionProbe& probe);

    friend class CHookSystem;
};

class CHookSystem {
  public:
    CFunctionHook* initHook(HANDLE handle, void* source, void* destination);
    bool           removeHook(CFunctionHook* hook);

    void           removeAllHooksFrom(HANDLE handle);

  private:
    std::vector<UP<CFunctionHook>> m_hooks;

    uint64_t                       getAddressForTrampo();

    struct SAllocatedPage {
        uint64_t addr = 0;
        uint64_t len  = 0;
        uint64_t used = 0;
    };

    std::vector<SAllocatedPage> m_pages;

    friend class CFunctionHook;
};

inline UP<CHookSystem> g_pFunctionHookSystem;