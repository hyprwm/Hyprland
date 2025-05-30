#include "LayoutManager.hpp"

CLayoutManager::CLayoutManager() {
    m_layouts.emplace_back(std::make_pair<>("dwindle", &m_dwindleLayout));
    m_layouts.emplace_back(std::make_pair<>("master", &m_masterLayout));
}

IHyprLayout* CLayoutManager::getCurrentLayout() {
    return m_layouts[m_currentLayoutID].second;
}

void CLayoutManager::switchToLayout(std::string layout) {
    for (size_t i = 0; i < m_layouts.size(); ++i) {
        if (m_layouts[i].first == layout) {
            if (i == (size_t)m_currentLayoutID)
                return;

            getCurrentLayout()->onDisable();
            m_currentLayoutID = i;
            getCurrentLayout()->onEnable();
            return;
        }
    }

    Debug::log(ERR, "Unknown layout!");
}

bool CLayoutManager::addLayout(const std::string& name, IHyprLayout* layout) {
    if (std::ranges::find_if(m_layouts, [&](const auto& other) { return other.first == name || other.second == layout; }) != m_layouts.end())
        return false;

    m_layouts.emplace_back(std::make_pair<>(name, layout));

    Debug::log(LOG, "Added new layout {} at {:x}", name, (uintptr_t)layout);

    return true;
}

bool CLayoutManager::removeLayout(IHyprLayout* layout) {
    const auto IT = std::ranges::find_if(m_layouts, [&](const auto& other) { return other.second == layout; });

    if (IT == m_layouts.end() || IT->first == "dwindle" || IT->first == "master")
        return false;

    if (m_currentLayoutID == IT - m_layouts.begin())
        switchToLayout("dwindle");

    Debug::log(LOG, "Removed a layout {} at {:x}", IT->first, (uintptr_t)layout);

    std::erase(m_layouts, *IT);

    return true;
}

std::vector<std::string> CLayoutManager::getAllLayoutNames() {
    std::vector<std::string> results(m_layouts.size());
    for (size_t i = 0; i < m_layouts.size(); ++i)
        results[i] = m_layouts[i].first;
    return results;
}
