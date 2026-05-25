#include "WorkspaceAlgoMatcher.hpp"

#include "../../config/ConfigValue.hpp"
#include "../../config/shared/workspace/WorkspaceRuleManager.hpp"

#include "../algorithm/Algorithm.hpp"
#include "../algorithm/TiledAlgorithm.hpp"
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
    const auto DWINDLE_ID   = nextAlgoID();
    const auto MASTER_ID    = nextAlgoID();
    const auto SCROLLING_ID = nextAlgoID();
    const auto MONOCLE_ID   = nextAlgoID();
    const auto DEFAULT_ID   = nextAlgoID();

    m_tiledAlgos = {
        {"dwindle", {.id = DWINDLE_ID, .factory = [] { return makeUnique<Tiled::CDwindleAlgorithm>(); }}},
        {"master", {.id = MASTER_ID, .factory = [] { return makeUnique<Tiled::CMasterAlgorithm>(); }}},
        {"scrolling", {.id = SCROLLING_ID, .factory = [] { return makeUnique<Tiled::CScrollingAlgorithm>(); }}},
        {"monocle", {.id = MONOCLE_ID, .factory = [] { return makeUnique<Tiled::CMonocleAlgorithm>(); }}},
    };

    m_floatingAlgos = {
        {"default", {.id = DEFAULT_ID, .factory = [] { return makeUnique<Floating::CDefaultFloatingAlgorithm>(); }}},
    };

    m_algoNames = {
        {&typeid(Tiled::CDwindleAlgorithm), "dwindle"},
        {&typeid(Tiled::CMasterAlgorithm), "master"},
        {&typeid(Tiled::CScrollingAlgorithm), "scrolling"},
        {&typeid(Tiled::CMonocleAlgorithm), "monocle"},
        {&typeid(Floating::CDefaultFloatingAlgorithm), "default"},
    };

    m_algoIDs = {
        {&typeid(Tiled::CDwindleAlgorithm), DWINDLE_ID},
        {&typeid(Tiled::CMasterAlgorithm), MASTER_ID},
        {&typeid(Tiled::CScrollingAlgorithm), SCROLLING_ID},
        {&typeid(Tiled::CMonocleAlgorithm), MONOCLE_ID},
        {&typeid(Floating::CDefaultFloatingAlgorithm), DEFAULT_ID},
    };

    m_nameIDs = {
        {"dwindle", DWINDLE_ID}, {"master", MASTER_ID}, {"scrolling", SCROLLING_ID}, {"monocle", MONOCLE_ID}, {"default", DEFAULT_ID},
    };

    m_idNames = {
        {DWINDLE_ID, "dwindle"}, {MASTER_ID, "master"}, {SCROLLING_ID, "scrolling"}, {MONOCLE_ID, "monocle"}, {DEFAULT_ID, "default"},
    };
}

bool CWorkspaceAlgoMatcher::registerTiledAlgo(const std::string& name, const std::type_info* typeInfo, std::function<UP<ITiledAlgorithm>()>&& factory) {
    if (m_tiledAlgos.contains(name) || m_floatingAlgos.contains(name))
        return false;

    const auto ID = nextAlgoID();

    m_tiledAlgos.emplace(name,
                         STiledAlgoEntry{
                             .id      = ID,
                             .factory = std::move(factory),
                         });

    m_algoNames.emplace(typeInfo, name);
    m_algoIDs.emplace(typeInfo, ID);
    m_nameIDs.emplace(name, ID);
    m_idNames.emplace(ID, name);

    updateWorkspaceLayouts();

    return true;
}

bool CWorkspaceAlgoMatcher::registerFloatingAlgo(const std::string& name, const std::type_info* typeInfo, std::function<UP<IFloatingAlgorithm>()>&& factory) {
    if (m_tiledAlgos.contains(name) || m_floatingAlgos.contains(name))
        return false;

    const auto ID = nextAlgoID();

    m_floatingAlgos.emplace(name,
                            SFloatingAlgoEntry{
                                .id      = ID,
                                .factory = std::move(factory),
                            });

    m_algoNames.emplace(typeInfo, name);
    m_algoIDs.emplace(typeInfo, ID);
    m_nameIDs.emplace(name, ID);
    m_idNames.emplace(ID, name);

    updateWorkspaceLayouts();

    return true;
}

bool CWorkspaceAlgoMatcher::unregisterAlgo(const std::string& name) {
    if (!m_tiledAlgos.contains(name) && !m_floatingAlgos.contains(name))
        return false;

    std::erase_if(m_algoIDs, [this, &name](const auto& e) {
        const auto NAME = m_algoNames.find(e.first);
        return NAME != m_algoNames.end() && NAME->second == name;
    });

    std::erase_if(m_algoNames, [&name](const auto& e) { return e.second == name; });

    m_nameIDs.erase(name);

    if (m_floatingAlgos.contains(name))
        m_floatingAlgos.erase(name);
    else
        m_tiledAlgos.erase(name);

    if (const auto ID = getIDForName(name); ID.has_value())
        m_idNames.erase(*ID);

    // this is needed here to avoid situations where a plugin unloads and we still have a UP
    // to a plugin layout
    updateWorkspaceLayouts();

    return true;
}

UP<ITiledAlgorithm> CWorkspaceAlgoMatcher::algoForNameTiled(const std::string& s) const {
    if (m_tiledAlgos.contains(s))
        return m_tiledAlgos.at(s).factory();

    return m_tiledAlgos.at(DEFAULT_TILED_ALGO).factory();
}

UP<IFloatingAlgorithm> CWorkspaceAlgoMatcher::algoForNameFloat(const std::string& s) const {
    if (m_floatingAlgos.contains(s))
        return m_floatingAlgos.at(s).factory();

    return m_floatingAlgos.at(DEFAULT_FLOATING_ALGO).factory();
}

std::string CWorkspaceAlgoMatcher::tiledAlgoForWorkspace(const PHLWORKSPACE& w) {
    static auto PLAYOUT = CConfigValue<Config::STRING>("general:layout");

    auto        rule = Config::workspaceRuleMgr()->getWorkspaceRuleFor(w);
    return rule && rule->m_layout.has_value() ? rule->m_layout.value() : *PLAYOUT;
}

CWorkspaceAlgoMatcher::AlgoID CWorkspaceAlgoMatcher::nextAlgoID() {
    return m_nextAlgoID++;
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

        const auto LAYOUT_TO_USE = tiledAlgoForWorkspace(ws.lock());

        const auto CURRENT_LAYOUT_ID = getIDForTiledAlgo(TILED_ALGO.get());
        auto       TARGET_LAYOUT_ID  = getIDForName(LAYOUT_TO_USE);

        if (!TARGET_LAYOUT_ID.has_value())
            TARGET_LAYOUT_ID = getIDForName(DEFAULT_TILED_ALGO);

        if (CURRENT_LAYOUT_ID.has_value() && TARGET_LAYOUT_ID.has_value() && *CURRENT_LAYOUT_ID == *TARGET_LAYOUT_ID)
            continue;

        // needs a switchup
        ws->m_space->algorithm()->updateTiledAlgo(algoForNameTiled(LAYOUT_TO_USE));
    }
}

std::optional<CWorkspaceAlgoMatcher::AlgoID> CWorkspaceAlgoMatcher::getIDForName(const std::string& name) {
    if (m_nameIDs.contains(name))
        return m_nameIDs.at(name);

    return std::nullopt;
}

std::optional<CWorkspaceAlgoMatcher::AlgoID> CWorkspaceAlgoMatcher::getIDForTiledAlgo(const ITiledAlgorithm* algo) {
    if (!algo)
        return std::nullopt;

    if (const auto NAME = algo->layoutName(); NAME.has_value())
        return getIDForName(*NAME);

    return getIDForTiledAlgo(&typeid(*algo));
}

std::optional<CWorkspaceAlgoMatcher::AlgoID> CWorkspaceAlgoMatcher::getIDForTiledAlgo(const std::type_info* type) {
    if (m_algoIDs.contains(type))
        return m_algoIDs.at(type);

    return std::nullopt;
}

std::string CWorkspaceAlgoMatcher::getNameForID(const AlgoID id) {
    if (m_idNames.contains(id))
        return m_idNames.at(id);

    return "unknown";
}
