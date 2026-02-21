#include "WorkspaceAlgoMatcher.hpp"

#include "../../config/ConfigValue.hpp"
#include "../../config/ConfigManager.hpp"

#include "../algorithm/Algorithm.hpp"
#include "../space/Space.hpp"

#include "../algorithm/floating/default/DefaultFloatingAlgorithm.hpp"
#include "../algorithm/tiled/dwindle/DwindleAlgorithm.hpp"
#include "../algorithm/tiled/master/MasterAlgorithm.hpp"
#include "../algorithm/tiled/scrolling/ScrollingAlgorithm.hpp"
#include "../algorithm/tiled/monocle/MonocleAlgorithm.hpp"

#include "../../Compositor.hpp"

using namespace Layout;
using namespace Layout::Supplementary;

constexpr const char*            DEFAULT_FLOATING_ALGO = "default";
constexpr const char*            DEFAULT_TILED_ALGO    = "dwindle";

const UP<CWorkspaceAlgoMatcher>& Supplementary::algoMatcher() {
    static UP<CWorkspaceAlgoMatcher> m = makeUnique<CWorkspaceAlgoMatcher>();
    return m;
}

CWorkspaceAlgoMatcher::CWorkspaceAlgoMatcher() {
    m_tiledAlgos = {
        {"dwindle", [] { return makeUnique<Tiled::CDwindleAlgorithm>(); }},
        {"master", [] { return makeUnique<Tiled::CMasterAlgorithm>(); }},
        {"scrolling", [] { return makeUnique<Tiled::CScrollingAlgorithm>(); }},
        {"monocle", [] { return makeUnique<Tiled::CMonocleAlgorithm>(); }},
    };

    m_floatingAlgos = {
        {"default", [] { return makeUnique<Floating::CDefaultFloatingAlgorithm>(); }},
    };

    m_algoNames = {
        {&typeid(Tiled::CDwindleAlgorithm), "dwindle"},
        {&typeid(Tiled::CMasterAlgorithm), "master"},
        {&typeid(Tiled::CScrollingAlgorithm), "scrolling"},
        {&typeid(Tiled::CMonocleAlgorithm), "monocle"},
        {&typeid(Floating::CDefaultFloatingAlgorithm), "default"},
    };
}

bool CWorkspaceAlgoMatcher::registerTiledAlgo(const std::string& name, const std::type_info* typeInfo, std::function<UP<ITiledAlgorithm>()>&& factory) {
    if (m_tiledAlgos.contains(name) || m_floatingAlgos.contains(name))
        return false;

    m_tiledAlgos.emplace(name, std::move(factory));
    m_algoNames.emplace(typeInfo, name);

    updateWorkspaceLayouts();

    return true;
}

bool CWorkspaceAlgoMatcher::registerFloatingAlgo(const std::string& name, const std::type_info* typeInfo, std::function<UP<IFloatingAlgorithm>()>&& factory) {
    if (m_tiledAlgos.contains(name) || m_floatingAlgos.contains(name))
        return false;

    m_floatingAlgos.emplace(name, std::move(factory));
    m_algoNames.emplace(typeInfo, name);

    updateWorkspaceLayouts();

    return true;
}

bool CWorkspaceAlgoMatcher::unregisterAlgo(const std::string& name) {
    if (!m_tiledAlgos.contains(name) && !m_floatingAlgos.contains(name))
        return false;

    std::erase_if(m_algoNames, [&name](const auto& e) { return e.second == name; });

    if (m_floatingAlgos.contains(name))
        m_floatingAlgos.erase(name);
    else
        m_tiledAlgos.erase(name);

    // this is needed here to avoid situations where a plugin unloads and we still have a UP
    // to a plugin layout
    updateWorkspaceLayouts();

    return true;
}

UP<ITiledAlgorithm> CWorkspaceAlgoMatcher::algoForNameTiled(const std::string& s) {
    if (m_tiledAlgos.contains(s))
        return m_tiledAlgos.at(s)();
    return m_tiledAlgos.at(DEFAULT_TILED_ALGO)();
}

UP<IFloatingAlgorithm> CWorkspaceAlgoMatcher::algoForNameFloat(const std::string& s) {
    if (m_floatingAlgos.contains(s))
        return m_floatingAlgos.at(s)();
    return m_floatingAlgos.at(DEFAULT_FLOATING_ALGO)();
}

std::string CWorkspaceAlgoMatcher::tiledAlgoForWorkspace(const PHLWORKSPACE& w) {
    static auto PLAYOUT = CConfigValue<Hyprlang::STRING>("general:layout");

    auto        rule = g_pConfigManager->getWorkspaceRuleFor(w);
    return rule.layout.value_or(*PLAYOUT);
}

SP<CAlgorithm> CWorkspaceAlgoMatcher::createAlgorithmForWorkspace(PHLWORKSPACE w) {
    return CAlgorithm::create(algoForNameTiled(tiledAlgoForWorkspace(w)), makeUnique<Floating::CDefaultFloatingAlgorithm>(), w->m_space);
}

void CWorkspaceAlgoMatcher::updateWorkspaceLayouts() {
    // TODO: make this ID-based, string comparison is slow
    for (const auto& ws : g_pCompositor->getWorkspaces()) {
        if (!ws)
            continue;

        const auto& TILED_ALGO = ws->m_space->algorithm()->tiledAlgo();

        if (!TILED_ALGO)
            continue;

        const auto LAYOUT_TO_USE = tiledAlgoForWorkspace(ws.lock());

        if (m_algoNames.contains(&typeid(*TILED_ALGO.get())) && m_algoNames.at(&typeid(*TILED_ALGO.get())) == LAYOUT_TO_USE)
            continue;

        // needs a switchup
        ws->m_space->algorithm()->updateTiledAlgo(algoForNameTiled(LAYOUT_TO_USE));
    }
}

std::string CWorkspaceAlgoMatcher::getNameForTiledAlgo(const std::type_info* type) {
    if (m_algoNames.contains(type))
        return m_algoNames.at(type);
    return "unknown";
}
