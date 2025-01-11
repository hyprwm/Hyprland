#include "LayoutManager.hpp"
#include "config/ConfigManager.hpp"
#include <algorithm>
#include <optional>

CLayoutManager::CLayoutManager() : m_vLayouts{{"dwindle", &m_cDwindleLayout}, {"master", &m_cMasterLayout}}, m_LayoutNameToID{{"dwindle", 0}, {"master", 1}} {}

IHyprLayout* CLayoutManager::getCurrentLayout(std::optional<int> monitorID) {
    static auto MON_ID_TO_LAYOUT = g_pConfigManager->getMonitorIDToLayout();
    if (monitorID.has_value() && MON_ID_TO_LAYOUT.contains(monitorID.value()))
        return m_vLayouts[m_LayoutNameToID[MON_ID_TO_LAYOUT.at(monitorID.value())]].second;
    return m_vLayouts[m_iCurrentLayoutID].second;
}

void CLayoutManager::switchToLayout(std::string layout) {
    // TODO disable only for monitor with persistent layout
    for (size_t i = 0; i < m_vLayouts.size(); ++i) {
        if (m_vLayouts[i].first != layout)
            continue;

        if (i == (size_t)m_iCurrentLayoutID)
            return;

        getCurrentLayout()->onDisable();
        m_iCurrentLayoutID = i;
        getCurrentLayout()->onEnable();
        return;
    }

    Debug::log(ERR, "Unknown layout!");
}

bool CLayoutManager::addLayout(const std::string& name, IHyprLayout* layout) {
    if (std::ranges::find_if(m_vLayouts, [&](const auto& other) { return other.first == name || other.second == layout; }) != m_vLayouts.end())
        return false;

    m_vLayouts.emplace_back(std::make_pair<>(name, layout));
    m_LayoutNameToID[name] = m_vLayouts.size() - 1;

    Debug::log(LOG, "Added new layout {} at {:x}", name, (uintptr_t)layout);

    return true;
}

bool CLayoutManager::removeLayout(IHyprLayout* layout) {
    const auto IT = std::ranges::find_if(m_vLayouts, [&](const auto& other) { return other.second == layout; });
    if (IT == m_vLayouts.end() || IT->first == "dwindle" || IT->first == "master")
        return false;

    if (m_iCurrentLayoutID == IT - m_vLayouts.begin())
        switchToLayout("dwindle");

    Debug::log(LOG, "Removed a layout {} at {:x}", IT->first, (uintptr_t)layout);

    std::erase(m_vLayouts, *IT);

    const int LAST_REMOVED = m_LayoutNameToID[IT->first];
    for (const auto& [k, v] : m_LayoutNameToID) {
        if (v > LAST_REMOVED) {
            m_LayoutNameToID[k] = v - 1;
        }
    }
    m_LayoutNameToID.erase(IT->first);

    return true;
}

std::vector<std::string> CLayoutManager::getAllLayoutNames() {
    std::vector<std::string> results(m_vLayouts.size());
    for (size_t i = 0; i < m_vLayouts.size(); ++i)
        results[i] = m_vLayouts[i].first;
    return results;
}
