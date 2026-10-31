#pragma once

#include "MatchEngine.hpp"
#include <string>
#include <vector>

namespace Desktop::Rule {
    class CTagMatchEngine : public IMatchEngine {
      public:
        CTagMatchEngine(const std::string& tag);
        virtual ~CTagMatchEngine() = default;

        virtual bool match(const CTagKeeper& keeper);

      private:
        std::vector<std::string> m_tags;
    };
}
