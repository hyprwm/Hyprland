#pragma once

#include "MatchEngine.hpp"

namespace Desktop::Rule {
    class CTagMatchEngine : public IMatchEngine {
      public:
        CTagMatchEngine(const std::string& tag);
        virtual ~CTagMatchEngine() = default;

        virtual bool match(const CTagKeeper& keeper);

      private:
        std::string m_tag;
    };
}