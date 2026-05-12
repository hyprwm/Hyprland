#pragma once

#include "../RuleWithEffects.hpp"
#include "../../DesktopTypes.hpp"
#include "LayerRuleEffectContainer.hpp"

#include <expected>
#include <variant>

namespace Desktop::Rule {
    using LayerRuleEffectValue = std::variant<std::monostate, bool, int64_t, float, std::string>;

    struct SLayerRuleEffect {
        using storageType = CLayerRuleEffectContainer::storageType;
        using valueType   = LayerRuleEffectValue;

        CLayerRuleEffectContainer::storageType key = LAYER_RULE_EFFECT_NONE;
        std::string                            raw;
        LayerRuleEffectValue                   value;
    };

    class CLayerRule : public CRuleWithEffects<SLayerRuleEffect, RULE_TYPE_LAYER> {
      public:
        using Base        = CRuleWithEffects<SLayerRuleEffect, RULE_TYPE_LAYER>;
        using storageType = Base::storageType;

        CLayerRule(const std::string& name = "");
        virtual ~CLayerRule() = default;

        bool matches(PHLLS w);

      private:
        std::expected<LayerRuleEffectValue, std::string> parseEffect(storageType e, const std::string& result) override;
    };
};
