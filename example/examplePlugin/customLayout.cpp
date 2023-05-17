#include "customLayout.hpp"
#include <hyprland/src/Compositor.hpp>
#include "globals.hpp"

void CHyprCustomLayout::onWindowCreatedTiling(CWindow* pWindow) {
    const auto PMONITOR = g_pCompositor->getMonitorFromID(pWindow->m_iMonitorID);
    const auto SIZE     = PMONITOR->vecSize;

    // these are used for focus and move calculations, and are *required* to touch for moving focus to work properly.
    pWindow->m_vPosition = Vector2D{(SIZE.x / 2.0) * (m_vWindowData.size() % 2), (SIZE.y / 2.0) * (int)(m_vWindowData.size() > 1)};
    pWindow->m_vSize     = SIZE / 2.0;

    // this is the actual pos and size of the window (where it's rendered)
    pWindow->m_vRealPosition = pWindow->m_vPosition + Vector2D{10, 10};
    pWindow->m_vRealSize     = pWindow->m_vSize - Vector2D{20, 20};

    const auto PDATA = &m_vWindowData.emplace_back();
    PDATA->pWindow   = pWindow;
}

void CHyprCustomLayout::onWindowRemovedTiling(CWindow* pWindow) {
    std::erase_if(m_vWindowData, [&](const auto& other) { return other.pWindow == pWindow; });
}

bool CHyprCustomLayout::isWindowTiled(CWindow* pWindow) {
    return std::find_if(m_vWindowData.begin(), m_vWindowData.end(), [&](const auto& other) { return other.pWindow == pWindow; }) != m_vWindowData.end();
}

void CHyprCustomLayout::recalculateMonitor(const int& eIdleInhibitMode) {
    ; // empty
}

void CHyprCustomLayout::recalculateWindow(CWindow* pWindow) {
    ; // empty
}

void CHyprCustomLayout::resizeActiveWindow(const Vector2D& delta, CWindow* pWindow) {
    ; // empty
}

void CHyprCustomLayout::fullscreenRequestForWindow(CWindow* pWindow, eFullscreenMode mode, bool on) {
    ; // empty
}

std::any CHyprCustomLayout::layoutMessage(SLayoutMessageHeader header, std::string content) {
    return "";
}

SWindowRenderLayoutHints CHyprCustomLayout::requestRenderHints(CWindow* pWindow) {
    return {};
}

void CHyprCustomLayout::switchWindows(CWindow* pWindowA, CWindow* pWindowB) {
    ; // empty
}

void CHyprCustomLayout::alterSplitRatio(CWindow* pWindow, float delta, bool exact) {
    ; // empty
}

std::string CHyprCustomLayout::getLayoutName() {
    return "custom";
}

void CHyprCustomLayout::replaceWindowDataWith(CWindow* from, CWindow* to) {
    ; // empty
}

void CHyprCustomLayout::onEnable() {
    for (auto& w : g_pCompositor->m_vWindows) {
        if (w->isHidden() || !w->m_bIsMapped || w->m_bFadingOut || w->m_bIsFloating)
            continue;

        onWindowCreatedTiling(w.get());
    }
}

void CHyprCustomLayout::onDisable() {
    m_vWindowData.clear();
}