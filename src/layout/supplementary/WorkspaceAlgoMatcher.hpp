#pragma once

#include "../../desktop/DesktopTypes.hpp"

#include <map>
#include <type_traits>
#include <functional>
#include <string>

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
        std::string    getNameForTiledAlgo(const std::type_info* type);

        // these fns can fail due to name collisions
        bool registerTiledAlgo(const std::string& name, const std::type_info* typeInfo, std::function<UP<ITiledAlgorithm>()>&& factory);
        bool registerFloatingAlgo(const std::string& name, const std::type_info* typeInfo, std::function<UP<IFloatingAlgorithm>()>&& factory);

        // this fn fails if the algo isn't registered
        bool unregisterAlgo(const std::string& name);

      private:
        UP<ITiledAlgorithm>                                            algoForNameTiled(const std::string& s);
        UP<IFloatingAlgorithm>                                         algoForNameFloat(const std::string& s);

        std::string                                                    tiledAlgoForWorkspace(const PHLWORKSPACE&);

        std::map<std::string, std::function<UP<ITiledAlgorithm>()>>    m_tiledAlgos;
        std::map<std::string, std::function<UP<IFloatingAlgorithm>()>> m_floatingAlgos;

        std::map<const std::type_info*, std::string>                   m_algoNames;
    };

    const UP<CWorkspaceAlgoMatcher>& algoMatcher();
}