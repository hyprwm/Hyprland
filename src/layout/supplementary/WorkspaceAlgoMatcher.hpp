#pragma once

#include "../../desktop/DesktopTypes.hpp"

namespace Layout {
    class CAlgorithm;
}

namespace Layout::Supplementary {
    class CWorkspaceAlgoMatcher {
      public:
        CWorkspaceAlgoMatcher()  = default;
        ~CWorkspaceAlgoMatcher() = default;

        SP<CAlgorithm> createAlgorithmForWorkspace(PHLWORKSPACE w);
    };

    const UP<CWorkspaceAlgoMatcher>& algoMatcher();
}