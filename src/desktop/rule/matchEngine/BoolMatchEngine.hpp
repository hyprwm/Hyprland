#pragma once

#include "MatchEngine.hpp"

namespace Desktop::Rule {
    class CBoolMatchEngine : public IMatchEngine {
      public:
        CBoolMatchEngine(const std::string&);
        virtual ~CBoolMatchEngine() = default;

        virtual bool match(bool other);

      private:
        bool m_value = false;
    };
}