#include "HookSystemManager.hpp"

#include "../plugins/PluginSystem.hpp"

CHookSystemManager::CHookSystemManager() {
    ; //
}

// returns the pointer to the function
SP<HOOK_CALLBACK_FN> CHookSystemManager::hookDynamic(const std::string& event, HOOK_CALLBACK_FN fn, HANDLE handle) {
    SP<HOOK_CALLBACK_FN> hookFN = makeShared<HOOK_CALLBACK_FN>(fn);
    m_mRegisteredHooks[event].emplace_back(SCallbackFNPtr{.fn = hookFN, .handle = handle});
    return hookFN;
}

void CHookSystemManager::unhook(SP<HOOK_CALLBACK_FN> fn) {
    for (auto& [k, v] : m_mRegisteredHooks) {
        std::erase_if(v, [&](const auto& other) {
            SP<HOOK_CALLBACK_FN> fn_ = other.fn.lock();

            return fn_.get() == fn.get();
        });
    }
}

void CHookSystemManager::emit(std::vector<SCallbackFNPtr>* const callbacks, SCallbackInfo& info, std::any data) {
    if (callbacks->empty())
        return;

    std::vector<HANDLE> faultyHandles;
    volatile bool       needsDeadCleanup = false;

    for (auto const& cb : *callbacks) {

        m_bCurrentEventPlugin = false;

        if (!cb.handle) {
            // we don't guard hl hooks

            if (SP<HOOK_CALLBACK_FN> fn = cb.fn.lock())
                (*fn)(fn.get(), info, data);
            else
                needsDeadCleanup = true;
            continue;
        }

        m_bCurrentEventPlugin = true;

        if (std::find(faultyHandles.begin(), faultyHandles.end(), cb.handle) != faultyHandles.end())
            continue;

        try {
            if (!setjmp(m_jbHookFaultJumpBuf)) {
                if (SP<HOOK_CALLBACK_FN> fn = cb.fn.lock())
                    (*fn)(fn.get(), info, data);
                else
                    needsDeadCleanup = true;
            } else {
                // this module crashed.
                throw std::exception();
            }
        } catch (std::exception& e) {
            // TODO: this works only once...?
            faultyHandles.push_back(cb.handle);
            NDebug::log(ERR, "[hookSystem] Hook from plugin {:x} caused a SIGSEGV, queueing for unloading.", (uintptr_t)cb.handle);
        }
    }

    if (needsDeadCleanup)
        std::erase_if(*callbacks, [](const auto& fn) { return !fn.fn.lock(); });

    if (!faultyHandles.empty()) {
        for (auto const& h : faultyHandles)
            g_pPluginSystem->unloadPlugin(g_pPluginSystem->getPluginByHandle(h), true);
    }
}

std::vector<SCallbackFNPtr>* CHookSystemManager::getVecForEvent(const std::string& event) {
    if (!m_mRegisteredHooks.contains(event))
        NDebug::log(LOG, "[hookSystem] New hook event registered: {}", event);

    return &m_mRegisteredHooks[event];
}
