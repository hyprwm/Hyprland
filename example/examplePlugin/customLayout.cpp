#include "customLayout.hpp"
#include "../../src/Compositor.hpp"

void CHyprCustomLayout::onWindowCreatedTiling(CWindow* pWindow) {
    const auto PMONITOR      = g_pCompositor->getMonitorFromID(pWindow->m_iMonitorID);
    pWindow->m_vRealPosition = PMONITOR->vecSize / 2.f - Vector2D{250, 250};
    pWindow->m_vRealSize     = Vector2D{250, 250};

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