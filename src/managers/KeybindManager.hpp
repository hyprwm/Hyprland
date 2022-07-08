#pragma once

#include "../defines.hpp"
#include <deque>
#include "../Compositor.hpp"
#include <unordered_map>
#include <functional>

struct SKeybind {
    std::string       key = "";
    int               keycode = -1;
    uint32_t          modmask = 0;
    std::string       handler = "";
    std::string       arg = "";
    bool              locked = false;
    std::string       submap = "";
};

class CKeybindManager {
public:
    CKeybindManager();

    bool                handleKeybinds(const uint32_t&, const xkb_keysym_t&, const int&);
    void                addKeybind(SKeybind);
    void                removeKeybind(uint32_t, const std::string&);
    uint32_t            stringToModMask(std::string);
    void                clearKeybinds();

    std::unordered_map<std::string, std::function<void(std::string)>> m_mDispatchers;

private:
    std::list<SKeybind> m_lKeybinds;

    inline static std::string m_szCurrentSelectedSubmap = "";

    bool                handleInternalKeybinds(xkb_keysym_t);
    bool                handleVT(xkb_keysym_t);

    // -------------- Dispatchers -------------- //
    static void         killActive(std::string);
    static void         spawn(std::string);
    static void         toggleActiveFloating(std::string);
    static void         toggleActivePseudo(std::string);
    static void         changeworkspace(std::string);
    static void         fullscreenActive(std::string);
    static void         moveActiveToWorkspace(std::string);
    static void         moveActiveToWorkspaceSilent(std::string);
    static void         moveFocusTo(std::string);
    static void         moveActiveTo(std::string);
    static void         toggleGroup(std::string);
    static void         changeGroupActive(std::string);
    static void         alterSplitRatio(std::string);
    static void         focusMonitor(std::string);
    static void         toggleSplit(std::string);
    static void         moveCursorToCorner(std::string);
    static void         workspaceOpt(std::string);
    static void         exitHyprland(std::string);
    static void         moveCurrentWorkspaceToMonitor(std::string);
    static void         moveWorkspaceToMonitor(std::string);
    static void         toggleSpecialWorkspace(std::string);
    static void         forceRendererReload(std::string);
    static void         resizeActive(std::string);
    static void         moveActive(std::string);
    static void         circleNext(std::string);
    static void         focusWindow(std::string);
    static void         setSubmap(std::string);

    friend class CCompositor;
};

inline std::unique_ptr<CKeybindManager> g_pKeybindManager;