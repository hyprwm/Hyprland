#pragma once

#include "../defines.hpp"

#include <unordered_map>
#include <any>
#include <list>

// global typedef for hooked functions. Passes itself as a ptr when called, and `data` additionally.
typedef std::function<void(void*, std::any)> HOOK_CALLBACK_FN;

#define EMIT_HOOK_EVENT(name, param)                                                                                                                                               \
    {                                                                                                                                                                              \
        static auto* const PEVENTVEC = g_pHookSystem->getVecForEvent(name);                                                                                                        \
        g_pHookSystem->emit(PEVENTVEC, param);                                                                                                                                     \
    }

class CHookSystemManager {
  public:
    CHookSystemManager();

    // returns the pointer to the function
    HOOK_CALLBACK_FN*               hookDynamic(const std::string& event, HOOK_CALLBACK_FN fn);
    void                            hookStatic(const std::string& event, HOOK_CALLBACK_FN* fn);
    void                            unhook(HOOK_CALLBACK_FN* fn);

    void                            emit(const std::vector<HOOK_CALLBACK_FN*>* callbacks, std::any data = 0);
    std::vector<HOOK_CALLBACK_FN*>* getVecForEvent(const std::string& event);

  private:
    // todo: this is slow. Maybe static ptrs should be somehow allowed. unique ptr for vec?
    std::list<std::pair<std::string, std::vector<HOOK_CALLBACK_FN*>>> m_lpRegisteredHooks;
    std::list<HOOK_CALLBACK_FN>                                       m_lCallbackFunctions;
};

inline std::unique_ptr<CHookSystemManager> g_pHookSystem;