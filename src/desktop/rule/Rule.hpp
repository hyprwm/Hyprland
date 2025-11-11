#pragma once

#include "matchEngine/MatchEngine.hpp"

#include "../../helpers/memory/Memory.hpp"
#include "../../helpers/time/Time.hpp"
#include <vector>
#include <unordered_map>
#include <cstdint>
#include <optional>

namespace Desktop::Rule {
    enum eRuleProperty : uint32_t {
        RULE_PROP_NONE                     = 0,
        RULE_PROP_CLASS                    = (1 << 0),
        RULE_PROP_TITLE                    = (1 << 1),
        RULE_PROP_INITIAL_CLASS            = (1 << 2),
        RULE_PROP_INITIAL_TITLE            = (1 << 3),
        RULE_PROP_FLOATING                 = (1 << 4),
        RULE_PROP_TAG                      = (1 << 5),
        RULE_PROP_XWAYLAND                 = (1 << 6),
        RULE_PROP_FULLSCREEN               = (1 << 7),
        RULE_PROP_PINNED                   = (1 << 8),
        RULE_PROP_FOCUS                    = (1 << 9),
        RULE_PROP_GROUP                    = (1 << 10),
        RULE_PROP_MODAL                    = (1 << 11),
        RULE_PROP_FULLSCREENSTATE_INTERNAL = (1 << 12),
        RULE_PROP_FULLSCREENSTATE_CLIENT   = (1 << 13),
        RULE_PROP_ON_WORKSPACE             = (1 << 14),
        RULE_PROP_CONTENT                  = (1 << 15),
        RULE_PROP_XDG_TAG                  = (1 << 16),
        RULE_PROP_NAMESPACE                = (1 << 17),
        RULE_PROP_EXEC_TOKEN               = (1 << 18),

        RULE_PROP_ALL = std::numeric_limits<std::underlying_type_t<eRuleProperty>>::max(),
    };

    enum eRuleType : uint8_t {
        RULE_TYPE_WINDOW = 0,
        RULE_TYPE_LAYER,
    };

    std::optional<eRuleProperty>    matchPropFromString(const std::string& s);
    const std::vector<std::string>& allMatchPropStrings();

    class IRule {
      public:
        virtual ~IRule() = default;

        virtual eRuleType                             type() = 0;
        virtual std::underlying_type_t<eRuleProperty> getPropertiesMask();

        void                                          registerMatch(eRuleProperty, const std::string&);
        void                                          markAsExecRule(const std::string& token, bool persistent = false);
        bool                                          isExecRule();
        bool                                          isExecPersistent();
        bool                                          execExpired();
        const std::string&                            execToken();

        const std::string&                            name();

      protected:
        IRule(const std::string& name = "");

        bool matches(eRuleProperty, const std::string& s);
        bool matches(eRuleProperty, bool b);
        bool has(eRuleProperty);

        //
        std::unordered_map<eRuleProperty, UP<IMatchEngine>> m_matchEngines;

      private:
        std::underlying_type_t<eRuleProperty> m_mask = 0;
        std::string                           m_name = "";

        struct {
            bool            isExecRule       = false;
            bool            isExecPersistent = false;
            std::string     token;
            Time::steady_tp expiresAt;
        } m_execData;
    };
}