#pragma once

#include "MatchEngine.hpp"

namespace Desktop::Rule {
    class CIntMatchEngine : public IMatchEngine {
      public:
        CIntMatchEngine(const std::string&);
        virtual ~CIntMatchEngine() = default;

        virtual bool match(int other);

      private:
        int m_value = 0;
    };
}