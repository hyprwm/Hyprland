#pragma once

#include "../../DesktopTypes.hpp"

class CTagKeeper;

namespace Desktop::Rule {
    enum eRuleMatchEngine : uint8_t {
        RULE_MATCH_ENGINE_REGEX = 0,
        RULE_MATCH_ENGINE_BOOL,
        RULE_MATCH_ENGINE_INT,
        RULE_MATCH_ENGINE_WORKSPACE,
        RULE_MATCH_ENGINE_TAG,
    };

    class IMatchEngine {
      public:
        virtual ~IMatchEngine() = default;
        virtual bool match(const std::string&);
        virtual bool match(bool);
        virtual bool match(int);
        virtual bool match(PHLWORKSPACE);
        virtual bool match(const CTagKeeper& keeper);

      protected:
        IMatchEngine() = default;
    };
};