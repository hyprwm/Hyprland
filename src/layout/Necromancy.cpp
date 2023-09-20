
#include "Necromancy.hpp"
#include "../managers/XWaylandManager.hpp"
#include "Compositor.hpp"
#include <ranges>

necromancy_error::necromancy_error(const std::string& msg) : std::runtime_error(msg) {}
necromancy_error::necromancy_error(const char* msg) : std::runtime_error(msg) {}

void necromancy_error::notify(const float duration) const {
    g_pHyprNotificationOverlay->addNotification(what(), CColor(0), duration, ICON_ERROR);
}

necromancy_error& necromancy_error::log() {
    Debug::log(ERR, "{}", what());
    return *this;
}

namespace Necromancy {
    template <>
    void dump(std::ostream& os, const std::string& data) {
        dump(os, data.length());
        os.write(data.c_str(), data.length());
    }

    template <>
    void dump(std::ostream& os, const Vector2D& data) {
        dump(os, data.x);
        dump(os, data.y);
    }

    template <>
    void load(std::istream& is, std::string& data) {
        size_t len;
        load(is, len);
        data.resize(len);
        is.read(data.data(), len);
    }

    template <>
    void load(std::istream& is, Vector2D& data) {
        load(is, data.x);
        load(is, data.y);
    }

    bool isWindowSavable(CWindow* pWindow) {
        return pWindow->m_bIsMapped && pWindow->m_iWorkspaceID != -1 && pWindow->m_iWorkspaceID != STRAYS_WORKSPACE_ID;
    }

    void createHeader(std::ostream& os) {
        os.write(SIGNATURE.c_str(), SIGNATURE.length());
        dump(os, VERSION);
    }

    bool validateHeader(std::istream& is) {
        std::string signature(SIGNATURE.length(), ' ');
        int         version = 0;

        is.read(signature.data(), SIGNATURE.length());
        load(is, version);
        return signature == SIGNATURE && version == VERSION;
    }

    void saveLayout(std::string location) {
        static const auto PSAVE_FILE_PATH = &g_pConfigManager->getConfigValuePtr("misc:layout_save_file")->strValue;
        location                          = absolutePath(location.empty() ? *PSAVE_FILE_PATH : location, g_pConfigManager->getDataDir());

        std::ofstream ofs{location, std::ios::binary};
        if (!ofs.is_open())
            throw necromancy_error(std::format("necromancy: Cannot write layout file: {}", location)).log();

        createHeader(ofs);

        std::string layoutName = g_pLayoutManager->getCurrentLayout()->getLayoutName();
        dump(ofs, layoutName);

        g_pLayoutManager->getCurrentLayout()->save(ofs);
    }

    void restoreLayout(std::string location) {
        static const auto PSAVE_FILE_PATH = &g_pConfigManager->getConfigValuePtr("misc:layout_save_file")->strValue;
        location                          = absolutePath(location.empty() ? *PSAVE_FILE_PATH : location, g_pConfigManager->getDataDir());

        std::ifstream ifs{location, std::ios::binary};
        if (!ifs.is_open())
            throw necromancy_error(std::format("necromancy: Cannot read layout file: {}", location)).log();

        if (!validateHeader(ifs))
            throw necromancy_error(std::format("necromancy: Layout file invalid: {}", location)).log();

        std::string layoutName;
        load(ifs, layoutName);

        if (const auto currentLayoutName = g_pLayoutManager->getCurrentLayout()->getLayoutName(); layoutName != currentLayoutName) {
            Debug::log(LOG, "Layout snapshot varies from the current one, wants: {}, current: {} switching", layoutName, currentLayoutName);
            g_pLayoutManager->switchToLayout(layoutName);
        }

        g_pLayoutManager->getCurrentLayout()->restore(ifs);
        g_pCompositor->updateAllWindowsAnimatedDecorationValues();
        // TODO: probably restore focus and cursor position when appropriate (follow_mouse=1)
        g_pInputManager->simulateMouseMovement();
    }
}

/* Window states */
SWindowState::SWindowState(CWindow* pWindow) {
    self    = pWindow;
    cmdline = getCmdline(pWindow->getPID());

    workspaceId  = pWindow->m_iWorkspaceID;
    monitorId    = pWindow->m_iMonitorID;
    title        = pWindow->m_szTitle;
    class_       = g_pXWaylandManager->getAppIDClass(pWindow);
    initialTitle = pWindow->m_szInitialTitle;
    initialClass = pWindow->m_szInitialClass;

    isFloating       = pWindow->m_bIsFloating;
    isPinned         = pWindow->m_bPinned;
    isFullscreen     = pWindow->m_bIsFullscreen;
    isFakeFullscreen = pWindow->m_bFakeFullscreenState;
    isPseudotiled    = pWindow->m_bIsPseudotiled;
    isHidden         = pWindow->isHidden();

    realPosition = pWindow->m_vRealPosition.goalv();
    realSize     = pWindow->m_vRealSize.goalv();

    groupData.next   = (uintptr_t)pWindow->m_sGroupData.pNextWindow;
    groupData.head   = pWindow->m_sGroupData.head;
    groupData.locked = pWindow->m_sGroupData.locked;
}

void SWindowState::apply(const auto&& callback) {
    // clang-format off
    std::apply([&](auto&&... data) { (callback(data), ...); }, std::tie(
        cmdline,
        workspaceId,
        monitorId,
        title,
        class_,
        initialTitle,
        initialClass,
        isFloating,
        isPinned,
        isFullscreen,
        isFakeFullscreen,
        isPseudotiled,
        isHidden,
        realPosition,
        realSize,
        groupData.next,
        groupData.head,
        groupData.locked
    ));
    // clang-format on
}

void SWindowState::applyToWindow() {
    if (!self) {
        Debug::log(LOG, "necromancy: not applying this soulless SWindowState\n  {}", *this);
        return;
    }
    if (!self->m_bIsMapped) {
        Debug::log(LOG, "necromancy: {} is invalid", self);
        return;
    }
    auto layout = g_pLayoutManager->getCurrentLayout();
    self->setHidden(isHidden);
    // workspace
    self->moveToWorkspace(workspaceId); // sets m_iWorkspaceID = workspaceId
    self->m_iMonitorID = monitorId;
    self->updateToplevel();

    // fake fullscreen
    g_pXWaylandManager->setWindowFullscreen(self, isFakeFullscreen);
    // fullscreen
    if (!isHidden && isFullscreen != self->m_bIsFullscreen) {
        // it's only possible for visible window to have fullscreen status
        auto* pWorkspace = g_pCompositor->getWorkspaceByID(workspaceId);
        layout->fullscreenRequestForWindow(self, pWorkspace->m_efFullscreenMode, isFullscreen);
        g_pXWaylandManager->setWindowFullscreen(self, self->m_bIsFullscreen);

        for (auto& w : g_pCompositor->m_vWindows) {
            if (w->m_iWorkspaceID == workspaceId && !w->m_bIsFullscreen && !w->m_bFadingOut && !w->m_bPinned)
                w->m_bCreatedOverFullscreen = false;
        }
        g_pCompositor->updateFullscreenFadeOnWorkspace(pWorkspace);
        g_pXWaylandManager->setWindowSize(self, self->m_vRealSize.goalv(), true);
        g_pCompositor->forceReportSizesToWindowsOnWorkspace(workspaceId);
        g_pInputManager->recheckIdleInhibitorStatus();
        g_pHyprRenderer->setWindowScanoutMode(self);
        g_pConfigManager->ensureVRR(g_pCompositor->getMonitorFromID(monitorId));
    }

    // pseudo
    if (isPseudotiled && !self->m_bIsFullscreen) {
        self->m_bIsPseudotiled = true;
        self->m_vPosition      = realPosition;
        self->m_vSize          = realSize;
        layout->recalculateWindow(self);
    }

    // floating
    if (isFloating && !isHidden) {
        g_pCompositor->changeWindowZOrder(self, true);
        g_pHyprRenderer->damageMonitor(g_pCompositor->getMonitorFromID(monitorId));
        restoreSize();
    }
    self->m_bIsFloating = isFloating;
    self->m_bPinned     = isPinned;

    self->updateDynamicRules();

    if (isPseudotiled && !self->m_bIsFullscreen)
        restoreSize();
}

void SWindowState::restoreSize() {
    self->m_vRealPosition = realPosition;
    self->m_vRealSize     = realSize;
    self->updateSpecialRenderData();
    self->updateToplevel();
}

bool SWindowState::isValid() {
    return self && (!self->m_bIsX11 && self->m_bMappedX11) && self->m_bIsMapped && self->m_iWorkspaceID != -1;
}

bool SWindowState::matchWindow(CWindow* pWindow) {
    const auto CLASS = g_pXWaylandManager->getAppIDClass(pWindow);
    return (class_ == CLASS && title == pWindow->m_szTitle)                            //
        || (class_ == pWindow->m_szInitialClass && title == pWindow->m_szInitialTitle) //
        || (initialClass == CLASS && initialTitle == pWindow->m_szTitle)               //
        || (initialClass == pWindow->m_szInitialClass && initialTitle == pWindow->m_szInitialTitle);
}

std::unique_ptr<window_state_map_t> SWindowState::collectAll() {
    auto states = std::make_unique<window_state_map_t>();
    for (const auto& w : g_pCompositor->m_vWindows) {
        if (!Necromancy::isWindowSavable(w.get()))
            continue;
        (*states)[(uintptr_t)w.get()] = SWindowState{w.get()};
    }
    return states;
}

void SWindowState::dumpAll(std::ostream& os) {
    const auto states = SWindowState::collectAll();
    Necromancy::dump(os, states->size());
    for (auto& ws : *states | std::views::values)
        ws.marshal(os);
}

std::unique_ptr<window_state_map_t> SWindowState::loadAll(std::istream& is) {
    auto   states = std::make_unique<window_state_map_t>();
    size_t count  = 0;
    Necromancy::load(is, count);
    for (size_t i = 0; i < count; i++) {
        SWindowState ws;
        uintptr_t    key = ws.unmarshal(is);
        (*states)[key]   = ws;
    }
    return states;
}

void SWindowState::marshal(std::ostream& os) {
    Necromancy::dump(os, (uintptr_t)self);
    apply([&]<typename T>(T& data) { Necromancy::dump(os, data); });
}

uintptr_t SWindowState::unmarshal(std::istream& is) {
    uintptr_t key;
    Necromancy::load(is, key);
    apply([&]<typename T>(T& data) { Necromancy::load(is, data); });
    return key;
}

/* Workspace states */
SWorkspaceState::SWorkspaceState(CWorkspace* pWorkspace) {
    id        = pWorkspace->m_iID;
    name      = pWorkspace->m_szName;
    monitorId = pWorkspace->m_iMonitorID;

    prev.id   = pWorkspace->m_sPrevWorkspace.iID;
    prev.name = pWorkspace->m_sPrevWorkspace.name;

    isSpecial       = pWorkspace->m_bIsSpecialWorkspace;
    defaultFloating = pWorkspace->m_bDefaultFloating;
    defaultPseudo   = pWorkspace->m_bDefaultPseudo;
    fullscreenMode  = pWorkspace->m_efFullscreenMode;
    immortal        = pWorkspace->m_bIndestructible;
}

void SWorkspaceState::apply(const auto&& callback) {
    // clang-format off
    std::apply([&](auto&&... data) { (callback(data), ...); }, std::tie(
        id,
        name,
        monitorId,
        prev.id,
        prev.name,
        isSpecial,
        defaultFloating,
        defaultPseudo,
        fullscreenMode,
        immortal
    ));
    // clang-format on
}

void SWorkspaceState::applyToWorkspace() {
    auto pWorkspace = g_pCompositor->getWorkspaceByID(id);
    if (!pWorkspace)
        pWorkspace = g_pCompositor->createNewWorkspace(id, monitorId);
    else
        g_pCompositor->renameWorkspace(id, name == "" ? std::to_string(id) : name);
    pWorkspace->m_sPrevWorkspace      = {prev.id, prev.name};
    pWorkspace->m_bIsSpecialWorkspace = isSpecial;
    pWorkspace->m_bDefaultFloating    = defaultFloating;
    pWorkspace->m_bDefaultPseudo      = defaultPseudo;
    pWorkspace->m_efFullscreenMode    = fullscreenMode;
    pWorkspace->m_bIndestructible     = immortal;
}

void SWorkspaceState::marshal(std::ostream& os) {
    apply([&]<typename T>(T& data) { Necromancy::dump(os, data); });
}

void SWorkspaceState::unmarshal(std::istream& is) {
    apply([&]<typename T>(T& data) { Necromancy::load(is, data); });
}