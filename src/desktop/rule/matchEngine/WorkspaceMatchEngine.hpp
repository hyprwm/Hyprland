#pragma once

#include "MatchEngine.hpp"

namespace Desktop::Rule {
    class CWorkspaceMatchEngine : public IMatchEngine {
      public:
        CWorkspaceMatchEngine(const std::string&);
        virtual ~CWorkspaceMatchEngine() = default;

        virtual bool match(PHLWORKSPACE ws);

      private:
        std::string m_value = "";
    };
}