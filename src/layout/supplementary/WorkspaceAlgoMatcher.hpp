#pragma once

#include "../../desktop/DesktopTypes.hpp"

#include <map>
#include <functional>
#include <string>
#include <optional>
#include <cstdint>

namespace Layout {
    class CAlgorithm;
    class ITiledAlgorithm;
    class IFloatingAlgorithm;
}

namespace Layout::Supplementary {
    class CWorkspaceAlgoMatcher {
      public:
        using AlgoID = uint32_t;

        CWorkspaceAlgoMatcher();
        ~CWorkspaceAlgoMatcher() = default;

        SP<CAlgorithm>        createAlgorithmForWorkspace(const PHLWORKSPACE& w);
        void                  updateWorkspaceLayouts();

        std::string           getNameForTiledAlgo(const ITiledAlgorithm* algo);
        std::string           getNameForTiledAlgo(const std::type_info* type);

        std::optional<AlgoID> getIDForTiledAlgo(const ITiledAlgorithm* algo);
        std::optional<AlgoID> getIDForTiledAlgo(const std::type_info* type);
        std::optional<AlgoID> getIDForName(const std::string& name);

        // these fns can fail due to name collisions
        bool registerTiledAlgo(const std::string& name, const std::type_info* typeInfo, std::function<UP<ITiledAlgorithm>()>&& factory);
        bool registerFloatingAlgo(const std::string& name, const std::type_info* typeInfo, std::function<UP<IFloatingAlgorithm>()>&& factory);

        // this fn fails if the algo isn't registered
        bool unregisterAlgo(const std::string& name);

      private:
        struct STiledAlgoEntry {
            AlgoID                               id = 0;
            std::function<UP<ITiledAlgorithm>()> factory;
        };

        struct SFloatingAlgoEntry {
            AlgoID                                  id = 0;
            std::function<UP<IFloatingAlgorithm>()> factory;
        };

        UP<ITiledAlgorithm>                          algoForNameTiled(const std::string& s) const;
        UP<IFloatingAlgorithm>                       algoForNameFloat(const std::string& s) const;

        std::string                                  tiledAlgoForWorkspace(const PHLWORKSPACE&);

        AlgoID                                       nextAlgoID();

        std::map<std::string, STiledAlgoEntry>       m_tiledAlgos;
        std::map<std::string, SFloatingAlgoEntry>    m_floatingAlgos;

        std::map<const std::type_info*, std::string> m_algoNames;
        std::map<const std::type_info*, AlgoID>      m_algoIDs;
        std::map<std::string, AlgoID>                m_nameIDs;

        AlgoID                                       m_nextAlgoID = 1;
    };

    const UP<CWorkspaceAlgoMatcher>& algoMatcher();
}