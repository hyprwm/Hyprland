#pragma once

#include "LayerRuleEffectContainer.hpp"
#include "../../DesktopTypes.hpp"
#include "../Rule.hpp"
#include "../../types/OverridableVar.hpp"
#include "../../../helpers/math/Math.hpp"
#include "../../../config/shared/complex/ComplexDataTypes.hpp"

namespace Desktop::Rule {
    class CLayerRule;

    class CLayerRuleApplicator {
      public:
        CLayerRuleApplicator(PHLLS ls);
        ~CLayerRuleApplicator() = default;

        CLayerRuleApplicator(const CLayerRuleApplicator&) = delete;
        CLayerRuleApplicator(CLayerRuleApplicator&)       = delete;
        CLayerRuleApplicator(CLayerRuleApplicator&&)      = delete;

        void propertiesChanged(std::underlying_type_t<eRuleProperty> props);
        void resetProps(std::underlying_type_t<eRuleProperty> props, Types::eOverridePriority prio = Types::PRIORITY_WINDOW_RULE);

        struct SCustomPropContainer {
            CLayerRuleEffectContainer::storageType idx      = LAYER_RULE_EFFECT_NONE;
            std::underlying_type_t<eRuleProperty>  propMask = RULE_PROP_NONE;
            std::string                            effect;
        };

        // This struct holds props that were dynamically registered. Plugins may read this.
        struct {
            std::unordered_map<CLayerRuleEffectContainer::storageType, UP<SCustomPropContainer>> props;
        } m_otherProps;

#define COMMA ,
#define DEFINE_PROP(type, name, def)                                                                                                                                               \
  private:                                                                                                                                                                         \
    std::pair<Types::COverridableVar<type>, std::underlying_type_t<eRuleProperty>> m_##name = {def, RULE_PROP_NONE};                                                               \
                                                                                                                                                                                   \
  public:                                                                                                                                                                          \
    Types::COverridableVar<type>& name() {                                                                                                                                         \
        return m_##name.first;                                                                                                                                                     \
    }                                                                                                                                                                              \
    void name##Override(const Types::COverridableVar<type>& other) {                                                                                                               \
        m_##name.first = other;                                                                                                                                                    \
    }

        // dynamic props
        DEFINE_PROP(bool, noanim, false)
        DEFINE_PROP(bool, blur, false)
        DEFINE_PROP(bool, blurPopups, false)
        DEFINE_PROP(bool, dimAround, false)
        DEFINE_PROP(bool, xray, false)
        DEFINE_PROP(bool, noScreenShare, false)

        DEFINE_PROP(Config::INTEGER, order, 0)
        DEFINE_PROP(Config::INTEGER, aboveLock, 0)

        DEFINE_PROP(Config::FLOAT, ignoreAlpha, 0.F)

        DEFINE_PROP(std::string, animationStyle, std::string(""))

#undef COMMA
#undef DEFINE_PROP

      private:
        PHLLSREF m_ls;

        void     applyDynamicRule(const SP<CLayerRule>& rule);
    };
};
