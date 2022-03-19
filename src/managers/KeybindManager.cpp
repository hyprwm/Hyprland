#include "KeybindManager.hpp"

void CKeybindManager::addKeybind(SKeybind kb) {
    m_dKeybinds.push_back(kb);
}

uint32_t CKeybindManager::stringToModMask(std::string mods) {
    uint32_t modMask = 0;
    if (mods.find("SHIFT") != std::string::npos)
        modMask |= WLR_MODIFIER_SHIFT;
    if (mods.find("CAPS") != std::string::npos)
        modMask |= WLR_MODIFIER_CAPS;
    if (mods.find("CTRL") != std::string::npos || mods.find("CONTROL") != std::string::npos)
        modMask |= WLR_MODIFIER_CTRL;
    if (mods.find("ALT") != std::string::npos)
        modMask |= WLR_MODIFIER_ALT;
    if (mods.find("MOD2") != std::string::npos)
        modMask |= WLR_MODIFIER_MOD2;
    if (mods.find("MOD3") != std::string::npos)
        modMask |= WLR_MODIFIER_MOD3;
    if (mods.find("SUPER") != std::string::npos || mods.find("WIN") != std::string::npos || mods.find("LOGO") != std::string::npos || mods.find("MOD4") != std::string::npos)
        modMask |= WLR_MODIFIER_LOGO;
    if (mods.find("MOD5") != std::string::npos)
        modMask |= WLR_MODIFIER_MOD5;

    return modMask;
}

bool CKeybindManager::handleKeybinds(const uint32_t& modmask, const xkb_keysym_t& key) {
    bool found = false;
    for (auto& k : m_dKeybinds) {
        if (modmask != k.modmask) 
            continue;

        // oMg such performance hit!!11!
        // this little maneouver is gonna cost us 4µs
        const auto KBKEY = xkb_keysym_from_name(k.key.c_str(), XKB_KEYSYM_CASE_INSENSITIVE);

        if (key != KBKEY)
            continue;

        // yes.
        if (k.handler == "exec") { spawn(k.arg); }
        else if (k.handler == "killactive") { killActive(k.arg); }

        found = true;
    }

    return found;
}

// Dispatchers

void CKeybindManager::spawn(std::string args) {
    args = "WAYLAND_DISPLAY=" + std::string(g_pCompositor->m_szWLDisplaySocket) + " DISPLAY=" + std::string(g_pXWaylandManager->m_sWLRXWayland->display_name) + " " + args;
    Debug::log(LOG, "Executing %s", args.c_str());
    if (fork() == 0) {
        execl("/bin/sh", "/bin/sh", "-c", args.c_str(), nullptr);

        _exit(0);
    }
}

void CKeybindManager::killActive(std::string args) {
    if (g_pCompositor->m_pLastFocus && g_pCompositor->windowValidMapped(g_pCompositor->m_pLastFocus))
        g_pXWaylandManager->sendCloseWindow(g_pCompositor->m_pLastFocus);
}

void CKeybindManager::clearKeybinds() {
    m_dKeybinds.clear();
}