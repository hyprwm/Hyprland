#pragma once

#include <string>
#include <vector>
#include <optional>
#include <cstdint>
#include "../../../helpers/signal/Signal.hpp"
#include "../../../helpers/memory/Memory.hpp"
#include "../../../desktop/DesktopTypes.hpp"
#include "../../../desktop/rule/windowRule/WindowRule.hpp"

namespace Config::Supplementary {
    struct SExecRequest {
        std::string exec      = "";
        bool        withRules = false;

        // if this rule is passed, don't put any match: on it, executor will do it
        // for you
        // also dont set withRules
        SP<Desktop::Rule::CWindowRule> rule;
    };

    class CExecutor {
      public:
        CExecutor();
        ~CExecutor() = default;

        void                    addExecOnce(SExecRequest&& cmd);
        void                    addExecShutdown(SExecRequest&& cmd);

        std::optional<uint64_t> spawn(const std::string& args);
        std::optional<uint64_t> spawn(const SExecRequest& args);
        std::optional<uint64_t> spawnRaw(const std::string& args);

        std::optional<uint64_t> spawnRawProc(const std::string&, PHLWORKSPACE pInitialWorkspace = nullptr, const std::string& execRuleToken = "");
        std::optional<uint64_t> spawnWithRules(std::string, PHLWORKSPACE pInitialWorkspace = nullptr);

      private:
        std::vector<SExecRequest> m_execOnce, m_execShutdown;

        void                      applyRuleToProc(SP<Desktop::Rule::CWindowRule> rule, int64_t pid, const std::string& token);

        struct {
            CHyprSignalListener init;
            CHyprSignalListener shutdown;
        } m_listeners;

        bool m_firstExecDispatched = false;
    };

    UP<CExecutor>& executor();
};