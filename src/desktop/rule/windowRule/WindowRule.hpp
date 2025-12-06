#pragma once

#include "../Rule.hpp"
#include "../../DesktopTypes.hpp"
#include "WindowRuleEffectContainer.hpp"
#include "../../../helpers/math/Math.hpp"

#include <unordered_set>

namespace Desktop::Rule {
    constexpr const char* EXEC_RULE_ENV_NAME = "HL_EXEC_RULE_TOKEN";

    class CWindowRule : public IRule {
      private:
        using storageType = CWindowRuleEffectContainer::storageType;

      public:
        CWindowRule(const std::string& name = "");
        virtual ~CWindowRule() = default;

        static SP<CWindowRule>                                  buildFromExecString(std::string&&);

        virtual eRuleType                                       type();

        void                                                    addEffect(storageType e, const std::string& result);
        const std::vector<std::pair<storageType, std::string>>& effects();
        const std::unordered_set<storageType>&                  effectsSet();

        bool                                                    matches(PHLWINDOW w, bool allowEnvLookup = false);

      private:
        std::vector<std::pair<storageType, std::string>> m_effects;
        std::unordered_set<storageType>                  m_effectSet;
    };
};
