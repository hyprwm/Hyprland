#include "HookSystemManager.hpp"

#include "../plugins/PluginSystem.hpp"

CHookSystemManager::CHookSystemManager() {
    ; //
}

// returns the pointer to the function
HOOK_CALLBACK_FN* CHookSystemManager::hookDynamic(const std::string& event, HOOK_CALLBACK_FN fn, HANDLE handle) {
    const auto PVEC = getVecForEvent(event);
    const auto PFN  = &m_lCallbackFunctions.emplace_back(fn);
    PVEC->emplace_back(SCallbackFNPtr{PFN, handle});
    return PFN;
}

void CHookSystemManager::hookStatic(const std::string& event, HOOK_CALLBACK_FN* fn, HANDLE handle) {
    const auto PVEC = getVecForEvent(event);
    PVEC->emplace_back(SCallbackFNPtr{fn, handle});
}

void CHookSystemManager::unhook(HOOK_CALLBACK_FN* fn) {
    std::erase_if(m_lCallbackFunctions, [&](const auto& other) { return &other == fn; });
    for (auto& [k, v] : m_lpRegisteredHooks) {
        std::erase_if(v, [&](const auto& other) { return other.fn == fn; });
    }
}

void CHookSystemManager::emit(const std::vector<SCallbackFNPtr>* callbacks, std::any data) {
    if (callbacks->empty())
        return;

    std::vector<HANDLE> faultyHandles;

    for (auto& cb : *callbacks) {

        m_bCurrentEventPlugin = false;

        if (!cb.handle) {
            // we don't guard hl hooks
            (*cb.fn)(cb.fn, data);
            continue;
        }

        m_bCurrentEventPlugin = true;

        if (std::find(faultyHandles.begin(), faultyHandles.end(), cb.handle) != faultyHandles.end())
            continue;

        try {
            if (!setjmp(m_jbHookFaultJumpBuf))
                (*cb.fn)(cb.fn, data);
            else {
                // this module crashed.
                throw std::exception();
            }
        } catch (std::exception& e) {
            // TODO: this works only once...?
            faultyHandles.push_back(cb.handle);
            Debug::log(ERR, " [hookSystem] Hook from plugin %lx caused a SIGSEGV, queueing for unloading.", cb.handle);
        }
    }

    if (!faultyHandles.empty()) {
        for (auto& h : faultyHandles)
            g_pPluginSystem->unloadPlugin(g_pPluginSystem->getPluginByHandle(h), true);
    }
}

std::vector<SCallbackFNPtr>* CHookSystemManager::getVecForEvent(const std::string& event) {
    auto IT = std::find_if(m_lpRegisteredHooks.begin(), m_lpRegisteredHooks.end(), [&](const auto& other) { return other.first == event; });

    if (IT != m_lpRegisteredHooks.end())
        return &IT->second;

    Debug::log(LOG, " [hookSystem] New hook event registered: %s", event.c_str());

    return &m_lpRegisteredHooks.emplace_back(std::make_pair<>(event, std::vector<SCallbackFNPtr>{})).second;
}