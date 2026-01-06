#include "WorkspaceAlgoMatcher.hpp"

#include "../../config/ConfigValue.hpp"

#include "../algorithm/Algorithm.hpp"

#include "../algorithm/floating/default/DefaultFloatingAlgorithm.hpp"
#include "../algorithm/tiled/dwindle/DwindleAlgorithm.hpp"
#include "../algorithm/tiled/master/MasterAlgorithm.hpp"

using namespace Layout;
using namespace Layout::Supplementary;

const UP<CWorkspaceAlgoMatcher>& Supplementary::algoMatcher() {
    static UP<CWorkspaceAlgoMatcher> m = makeUnique<CWorkspaceAlgoMatcher>();
    return m;
}

SP<CAlgorithm> CWorkspaceAlgoMatcher::createAlgorithmForWorkspace(PHLWORKSPACE w) {
    static auto         PLAYOUT = CConfigValue<Hyprlang::STRING>("general:layout");

    UP<ITiledAlgorithm> algo = nullptr;

    if (*PLAYOUT == std::string_view{"master"})
        algo = makeUnique<Tiled::CMasterAlgorithm>();
    else
        algo = makeUnique<Tiled::CDwindleAlgorithm>();

    return CAlgorithm::create(std::move(algo), makeUnique<Floating::CDefaultFloatingAlgorithm>(), w->m_space);
}
