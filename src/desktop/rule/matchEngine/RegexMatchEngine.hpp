#pragma once

#include "MatchEngine.hpp"
#include "../../../helpers/memory/Memory.hpp"

//NOLINTNEXTLINE
namespace re2 {
    class RE2;
};

namespace Desktop::Rule {
    class CRegexMatchEngine : public IMatchEngine {
      public:
        CRegexMatchEngine(const std::string& regex);
        virtual ~CRegexMatchEngine() = default;

        virtual bool match(const std::string& other);

      private:
        UP<re2::RE2> m_regex;
        bool         m_negative = false;
    };
}