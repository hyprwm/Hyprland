#pragma once

#include <string>
#include <vector>
#include <cstddef>
#include <unordered_set>
#include "../helpers/memory/Memory.hpp"

#define HANDLE void*
#if defined(__aarch64__)
// up to 5 displaced instructions, each expanding to at most 16B of code + 8B literal, plus the jump back
#define HOOK_TRAMPOLINE_MAX_SIZE 192
#else
#define HOOK_TRAMPOLINE_MAX_SIZE 32
#endif

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
    void*                      m_source               = nullptr;
    void*                      m_launchTrampolineAddr = nullptr;
    void*                      m_landTrampolineAddr   = nullptr;
    void*                      m_destination          = nullptr;
    size_t                     m_hookLen              = 0;
    HANDLE                     m_owner                = nullptr;
    bool                       m_active               = false;

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

    bool              hookAArch64();

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

    std::vector<SAllocatedPage>  m_pages;
    std::unordered_set<uint64_t> m_activeHooks;

    friend class CFunctionHook;
};

inline UP<CHookSystem> g_pFunctionHookSystem;