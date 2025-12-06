#pragma once

#include "../Rule.hpp"
#include "../../DesktopTypes.hpp"
#include "LayerRuleEffectContainer.hpp"

namespace Desktop::Rule {
    class CLayerRule : public IRule {
      public:
        using storageType = CLayerRuleEffectContainer::storageType;

        CLayerRule(const std::string& name = "");
        virtual ~CLayerRule() = default;

        virtual eRuleType                                       type();

        void                                                    addEffect(storageType e, const std::string& result);
        const std::vector<std::pair<storageType, std::string>>& effects();

        bool                                                    matches(PHLLS w);

      private:
        std::vector<std::pair<storageType, std::string>> m_effects;
    };
};
