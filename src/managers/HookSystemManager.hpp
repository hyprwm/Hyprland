#pragma once

#include "../defines.hpp"

#include <unordered_map>
#include <any>
#include <array>
#include <list>

#include <csetjmp>

#include "../plugins/PluginAPI.hpp"

// global typedef for hooked functions. Passes itself as a ptr when called, and `data` additionally.
typedef std::function<void(void*, std::any)> HOOK_CALLBACK_FN;

struct SCallbackFNPtr {
    HOOK_CALLBACK_FN* fn     = nullptr;
    HANDLE            handle = nullptr;
};

#define EMIT_HOOK_EVENT(name, param)                                                                                                                                               \
    {                                                                                                                                                                              \
        static auto* const PEVENTVEC = g_pHookSystem->getVecForEvent(name);                                                                                                        \
        g_pHookSystem->emit(PEVENTVEC, param);                                                                                                                                     \
    }

class CHookSystemManager {
  public:
    CHookSystemManager();

    // returns the pointer to the function
    HOOK_CALLBACK_FN*            hookDynamic(const std::string& event, HOOK_CALLBACK_FN fn, HANDLE handle = nullptr);
    void                         hookStatic(const std::string& event, HOOK_CALLBACK_FN* fn, HANDLE handle = nullptr);
    void                         unhook(HOOK_CALLBACK_FN* fn);

    void                         emit(const std::vector<SCallbackFNPtr>* callbacks, std::any data = 0);
    std::vector<SCallbackFNPtr>* getVecForEvent(const std::string& event);

    bool                         m_bCurrentEventPlugin = false;
    jmp_buf                      m_jbHookFaultJumpBuf;

  private:
    // todo: this is slow. Maybe static ptrs should be somehow allowed. unique ptr for vec?
    std::list<std::pair<std::string, std::vector<SCallbackFNPtr>>> m_lpRegisteredHooks;
    std::list<HOOK_CALLBACK_FN>                                    m_lCallbackFunctions;
};

inline std::unique_ptr<CHookSystemManager> g_pHookSystem;