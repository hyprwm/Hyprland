#pragma once

#include "../../desktop/DesktopTypes.hpp"

#include <map>
#include <type_traits>

namespace Layout {
    class CAlgorithm;
    class ITiledAlgorithm;
    class IFloatingAlgorithm;
}

namespace Layout::Supplementary {
    class CWorkspaceAlgoMatcher {
      public:
        CWorkspaceAlgoMatcher();
        ~CWorkspaceAlgoMatcher() = default;

        SP<CAlgorithm> createAlgorithmForWorkspace(PHLWORKSPACE w);

        void           updateWorkspaceLayouts();

      private:
        UP<ITiledAlgorithm>                                            algoForNameTiled(const std::string& s);
        UP<IFloatingAlgorithm>                                         algoForNameFloat(const std::string& s);

        std::map<std::string, std::function<UP<ITiledAlgorithm>()>>    m_tiledAlgos;
        std::map<std::string, std::function<UP<IFloatingAlgorithm>()>> m_floatingAlgos;

        std::map<const std::type_info*, std::string>                   m_algoNames;
    };

    const UP<CWorkspaceAlgoMatcher>& algoMatcher();
}