#include "HookSystemManager.hpp"

CHookSystemManager::CHookSystemManager() {
    ; //
}

// returns the pointer to the function
HOOK_CALLBACK_FN* CHookSystemManager::hookDynamic(const std::string& event, HOOK_CALLBACK_FN fn) {
    const auto PVEC = getVecForEvent(event);
    const auto PFN  = &m_lCallbackFunctions.emplace_back(fn);
    PVEC->emplace_back(PFN);
    return PFN;
}

void CHookSystemManager::hookStatic(const std::string& event, HOOK_CALLBACK_FN* fn) {
    const auto PVEC = getVecForEvent(event);
    PVEC->emplace_back(fn);
}

void CHookSystemManager::unhook(HOOK_CALLBACK_FN* fn) {
    std::erase_if(m_lCallbackFunctions, [&](const auto& other) { return &other == fn; });
    for (auto& [k, v] : m_lpRegisteredHooks) {
        std::erase_if(v, [&](const auto& other) { return other == fn; });
    }
}

void CHookSystemManager::emit(const std::vector<HOOK_CALLBACK_FN*>* callbacks, std::any data) {
    if (callbacks->empty())
        return;

    for (auto& cb : *callbacks)
        (*cb)(cb, data);
}

std::vector<HOOK_CALLBACK_FN*>* CHookSystemManager::getVecForEvent(const std::string& event) {
    auto IT = std::find_if(m_lpRegisteredHooks.begin(), m_lpRegisteredHooks.end(), [&](const auto& other) { return other.first == event; });

    if (IT != m_lpRegisteredHooks.end())
        return &IT->second;

    Debug::log(LOG, "[hookSystem] New hook event registered: %s", event.c_str());

    return &m_lpRegisteredHooks.emplace_back(std::make_pair<>(event, std::vector<HOOK_CALLBACK_FN*>{})).second;
}