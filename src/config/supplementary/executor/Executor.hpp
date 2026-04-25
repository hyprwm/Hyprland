#pragma once

#include <string>
#include <vector>
#include <optional>
#include <cstdint>
#include "../../../helpers/signal/Signal.hpp"
#include "../../../helpers/memory/Memory.hpp"
#include "../../../desktop/DesktopTypes.hpp"

namespace Config::Supplementary {
    struct SExecRequest {
        std::string exec      = "";
        bool        withRules = false;
    };

    class CExecutor {
      public:
        CExecutor();
        ~CExecutor() = default;

        void                    addExecOnce(const SExecRequest& cmd);
        void                    addExecShutdown(const SExecRequest& cmd);

        std::optional<uint64_t> spawn(const std::string& args);
        std::optional<uint64_t> spawnRaw(const std::string& args);

        std::optional<uint64_t> spawnRawProc(const std::string&, PHLWORKSPACE pInitialWorkspace = nullptr, const std::string& execRuleToken = "");
        std::optional<uint64_t> spawnWithRules(std::string, PHLWORKSPACE pInitialWorkspace = nullptr);

      private:
        std::vector<SExecRequest> m_execOnce, m_execShutdown;

        struct {
            CHyprSignalListener init;
            CHyprSignalListener shutdown;
        } m_listeners;

        bool m_firstExecDispatched = false;
    };

    UP<CExecutor>& executor();
};