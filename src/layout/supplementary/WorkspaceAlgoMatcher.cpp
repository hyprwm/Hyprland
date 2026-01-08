#include "WorkspaceAlgoMatcher.hpp"

#include "../../config/ConfigValue.hpp"

#include "../algorithm/Algorithm.hpp"
#include "../space/Space.hpp"

#include "../algorithm/floating/default/DefaultFloatingAlgorithm.hpp"
#include "../algorithm/tiled/dwindle/DwindleAlgorithm.hpp"
#include "../algorithm/tiled/master/MasterAlgorithm.hpp"

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
    };

    m_floatingAlgos = {
        {"default", [] { return makeUnique<Floating::CDefaultFloatingAlgorithm>(); }},
    };

    m_algoNames = {
        {&typeid(Tiled::CDwindleAlgorithm), "dwindle"},
        {&typeid(Tiled::CMasterAlgorithm), "master"},
        {&typeid(Floating::CDefaultFloatingAlgorithm), "default"},
    };
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

SP<CAlgorithm> CWorkspaceAlgoMatcher::createAlgorithmForWorkspace(PHLWORKSPACE w) {
    static auto PLAYOUT = CConfigValue<Hyprlang::STRING>("general:layout");

    return CAlgorithm::create(algoForNameTiled(*PLAYOUT), makeUnique<Floating::CDefaultFloatingAlgorithm>(), w->m_space);
}

void CWorkspaceAlgoMatcher::updateWorkspaceLayouts() {
    static auto PLAYOUT = CConfigValue<Hyprlang::STRING>("general:layout");

    // TODO: make this ID-based, string comparison is slow
    for (const auto& ws : g_pCompositor->getWorkspaces()) {
        if (!ws)
            continue;

        const auto& TILED_ALGO = ws->m_space->algorithm()->tiledAlgo();

        if (!TILED_ALGO)
            continue;

        if (m_algoNames.at(&typeid(*TILED_ALGO.get())) == *PLAYOUT)
            continue;

        // needs a switchup
        ws->m_space->algorithm()->updateTiledAlgo(algoForNameTiled(*PLAYOUT));
    }
}
