#include "WorkspaceAlgoMatcher.hpp"

#include "../algorithm/Algorithm.hpp"

#include "../algorithm/floating/default/DefaultFloatingAlgorithm.hpp"
#include "../algorithm/tiled/dwindle/DwindleAlgorithm.hpp"

using namespace Layout;
using namespace Layout::Supplementary;

const UP<CWorkspaceAlgoMatcher>& Supplementary::algoMatcher() {
    static UP<CWorkspaceAlgoMatcher> m = makeUnique<CWorkspaceAlgoMatcher>();
    return m;
}

SP<CAlgorithm> CWorkspaceAlgoMatcher::createAlgorithmForWorkspace(PHLWORKSPACE w) {
    return CAlgorithm::create(makeUnique<Tiled::CDwindleAlgorithm>(), makeUnique<Floating::CDefaultFloatingAlgorithm>(), w->m_space);
}
