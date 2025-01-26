#pragma once

#include "../defines.hpp"

#include <unordered_map>
#include <any>
#include <array>
#include <list>

#include <csetjmp>

#define HANDLE void*

// global typedef for hooked functions. Passes itself as a ptr when called, and `data` additionally.

typedef std::function<void(void*, SCallbackInfo& info, std::any data)> HOOK_CALLBACK_FN;

struct SCallbackFNPtr {
    WP<HOOK_CALLBACK_FN> fn;
    HANDLE               handle = nullptr;
};

#define EMIT_HOOK_EVENT(name, param)                                                                                                                                               \
    {                                                                                                                                                                              \
        static auto* const PEVENTVEC = g_pHookSystem->getVecForEvent(name);                                                                                                        \
        SCallbackInfo      info;                                                                                                                                                   \
        g_pHookSystem->emit(PEVENTVEC, info, param);                                                                                                                               \
    }

#define EMIT_HOOK_EVENT_CANCELLABLE(name, param)                                                                                                                                   \
    {                                                                                                                                                                              \
        static auto* const PEVENTVEC = g_pHookSystem->getVecForEvent(name);                                                                                                        \
        SCallbackInfo      info;                                                                                                                                                   \
        g_pHookSystem->emit(PEVENTVEC, info, param);                                                                                                                               \
        if (info.cancelled)                                                                                                                                                        \
            return;                                                                                                                                                                \
    }

class CHookSystemManager {
  public:
    CHookSystemManager();

    // returns the pointer to the function.
    // losing this pointer (letting it get destroyed)
    // will equal to unregistering the callback.
    [[nodiscard("Losing this pointer instantly unregisters the callback")]] SP<HOOK_CALLBACK_FN> hookDynamic(const std::string& event, HOOK_CALLBACK_FN fn,
                                                                                                             HANDLE handle = nullptr);
    void                                                                                         unhook(SP<HOOK_CALLBACK_FN> fn);

    void                         emit(std::vector<SCallbackFNPtr>* const callbacks, SCallbackInfo& info, std::any data = 0);
    std::vector<SCallbackFNPtr>* getVecForEvent(const std::string& event);

    bool                         m_bCurrentEventPlugin = false;
    jmp_buf                      m_jbHookFaultJumpBuf;

  private:
    std::unordered_map<std::string, std::vector<SCallbackFNPtr>> m_mRegisteredHooks;
};

inline UP<CHookSystemManager> g_pHookSystem;
