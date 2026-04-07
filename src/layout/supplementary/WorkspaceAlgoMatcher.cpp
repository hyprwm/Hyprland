#include "WorkspaceAlgoMatcher.hpp"

#include "../../config/ConfigValue.hpp"
#include "../../config/shared/workspace/WorkspaceRuleManager.hpp"

#include "../algorithm/Algorithm.hpp"
#include "../space/Space.hpp"

#include "../algorithm/floating/default/DefaultFloatingAlgorithm.hpp"
#include "../algorithm/tiled/dwindle/DwindleAlgorithm.hpp"
#include "../algorithm/tiled/master/MasterAlgorithm.hpp"
#include "../algorithm/tiled/scrolling/ScrollingAlgorithm.hpp"
#include "../algorithm/tiled/monocle/MonocleAlgorithm.hpp"

#include "../../Compositor.hpp"

static LayoutID layoutIDFromString(const std::string& s) {
    if (s == "dwindle")
        return LayoutID::DWINDLE;
    if (s == "master")
        return LayoutID::MASTER;
    if (s == "scrolling")
        return LayoutID::SCROLLING;
    if (s == "monocle")
        return LayoutID::MONOCLE;
    if (s == "floating")
        return LayoutID::FLOATING;
    return LayoutID::UNKNOWN;
}

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
    m_algoIDs = {
        {&typeid(Tiled::CDwindleAlgorithm), LayoutID::DWINDLE},
        {&typeid(Tiled::CMasterAlgorithm), LayoutID::MASTER},
        {&typeid(Tiled::CScrollingAlgorithm), LayoutID::SCROLLING},
        {&typeid(Tiled::CMonocleAlgorithm), LayoutID::MONOCLE},
        {&typeid(Floating::CDefaultFloatingAlgorithm), LayoutID::FLOATING},
    };
}

bool CWorkspaceAlgoMatcher::registerTiledAlgo(const std::string& name, const std::type_info* typeInfo, std::function<UP<ITiledAlgorithm>()>&& factory) {
    if (m_tiledAlgos.contains(name) || m_floatingAlgos.contains(name))
        return false;

    m_tiledAlgos.emplace(name, std::move(factory));
    m_algoNames.emplace(typeInfo, name);
    m_algoIDs.emplace(typeInfo, layoutIDFromString(name));

    updateWorkspaceLayouts();

    return true;
}

bool CWorkspaceAlgoMatcher::registerFloatingAlgo(const std::string& name, const std::type_info* typeInfo, std::function<UP<IFloatingAlgorithm>()>&& factory) {
    if (m_tiledAlgos.contains(name) || m_floatingAlgos.contains(name))
        return false;

    m_floatingAlgos.emplace(name, std::move(factory));
    m_algoNames.emplace(typeInfo, name);
    m_algoIDs.emplace(typeInfo, layoutIDFromString(name));

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

    auto        rule = Config::workspaceRuleMgr()->getWorkspaceRuleFor(w);
    return rule && rule->m_layout.has_value() ? rule->m_layout.value() : *PLAYOUT;
}

SP<CAlgorithm> CWorkspaceAlgoMatcher::createAlgorithmForWorkspace(PHLWORKSPACE w) {
    return CAlgorithm::create(algoForNameTiled(tiledAlgoForWorkspace(w)), makeUnique<Floating::CDefaultFloatingAlgorithm>(), w->m_space);
}

void CWorkspaceAlgoMatcher::updateWorkspaceLayouts() {

    for (const auto& ws : g_pCompositor->getWorkspaces()) {
        if (!ws)
            continue;

        const auto& TILED_ALGO = ws->m_space->algorithm()->tiledAlgo();

        if (!TILED_ALGO)
            continue;

        const auto layoutStr = tiledAlgoForWorkspace(ws.lock());
        const auto layoutID  = layoutIDFromString(layoutStr);

        const auto type = &typeid(*TILED_ALGO.get());

        const auto it = m_algoIDs.find(type);
        if (it != m_algoIDs.end()) {
            if (layoutID != LayoutID::UNKNOWN && it->second == layoutID)
                continue;

            // fallback for unknown layouts (plugins)
            if (layoutID == LayoutID::UNKNOWN && m_algoNames.contains(type) && m_algoNames.at(type) == layoutStr)
                continue;
        }

        // needs a switchup
        ws->m_space->algorithm()->updateTiledAlgo(algoForNameTiled(layoutStr));
    }
}

std::string CWorkspaceAlgoMatcher::getNameForTiledAlgo(const std::type_info* type) {
    if (m_algoNames.contains(type))
        return m_algoNames.at(type);
    return "unknown";
}
