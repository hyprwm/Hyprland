#pragma once

#include "../Rule.hpp"
#include "../../DesktopTypes.hpp"
#include "LayerRuleEffectContainer.hpp"

#include <expected>
#include <variant>

namespace Desktop::Rule {
    using LayerRuleEffectValue = std::variant<std::monostate, bool, int64_t, float, std::string>;

    struct SLayerRuleEffect {
        CLayerRuleEffectContainer::storageType key = LAYER_RULE_EFFECT_NONE;
        std::string                            raw;
        LayerRuleEffectValue                   value;
    };

    class CLayerRule : public IRule {
      public:
        using storageType = CLayerRuleEffectContainer::storageType;

        CLayerRule(const std::string& name = "");
        virtual ~CLayerRule() = default;

        virtual eRuleType                    type();

        std::expected<void, std::string>     addEffect(storageType e, const std::string& result);
        const std::vector<SLayerRuleEffect>& effects();

        void                                 setEnabled(bool enable);
        bool                                 isEnabled() const;

        bool                                 matches(PHLLS w);

      private:
        std::vector<SLayerRuleEffect> m_effects;
        bool                          m_enabled = true;
    };
};
