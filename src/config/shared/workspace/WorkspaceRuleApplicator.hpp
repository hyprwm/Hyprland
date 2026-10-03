#pragma once

#include <sys/types.h>
#include <vector>
#include "desktop/DesktopTypes.hpp"
#include "desktop/types/OverridableVar.hpp"
#include "config/shared/complex/ComplexDataTypes.hpp"

namespace Config {

    enum eWorkspaceRule : u_int32_t {

        WORKSPACE_RULE_PROP_NONE             = 0,
        WORKSPACE_RULE_PROP_ANIMATION        = 1 << 0,
        WORKSPACE_RULE_PROP_BORDER_SIZE      = 1 << 1,
        WORKSPACE_RULE_PROP_DECORATE         = 1 << 2,
        WORKSPACE_RULE_PROP_DEFAULT_NAME     = 1 << 3,
        WORKSPACE_RULE_PROP_GAPS             = 1 << 4,
        WORKSPACE_RULE_PROP_GAPS_FLOAT       = 1 << 5 | WORKSPACE_RULE_PROP_GAPS,
        WORKSPACE_RULE_PROP_GAPS_IN          = 1 << 6 | WORKSPACE_RULE_PROP_GAPS,
        WORKSPACE_RULE_PROP_GAPS_OUT         = 1 << 7 | WORKSPACE_RULE_PROP_GAPS,
        WORKSPACE_RULE_PROP_LAYOUT           = 1 << 8,
        WORKSPACE_RULE_PROP_LAYOUT_OPTS      = 1 << 9,
        WORKSPACE_RULE_PROP_MONITOR          = 1 << 10,
        WORKSPACE_RULE_PROP_DEFAULT          = 1 << 11,
        WORKSPACE_RULE_PROP_BORDER_DISABLE   = 1 << 12 | WORKSPACE_RULE_PROP_BORDER_SIZE,
        WORKSPACE_RULE_PROP_ROUNDING_DISABLE = 1 << 13,
        WORKSPACE_RULE_PROP_SHADOW_DISABLE   = 1 << 14,
        WORKSPACE_RULE_PROP_ON_CREATED_EMPTY = 1 << 15,
        WORKSPACE_RULE_PROP_PERSISTENT       = 1 << 16,

        WORKSPACE_RULE_PROP_ALL = std::numeric_limits<std::underlying_type_t<eWorkspaceRule>>::max(),
    };

    class CWorkspaceRuleApplicator {

      public:
        CWorkspaceRuleApplicator(PHLWORKSPACE w);
        ~CWorkspaceRuleApplicator() = default;

        CWorkspaceRuleApplicator(const CWorkspaceRuleApplicator&)            = delete;
        CWorkspaceRuleApplicator(CWorkspaceRuleApplicator&&)                 = delete;
        CWorkspaceRuleApplicator& operator=(const CWorkspaceRuleApplicator&) = delete;
        CWorkspaceRuleApplicator& operator=(CWorkspaceRuleApplicator&&)      = delete;

        void                      propertiesChanged(std::underlying_type_t<eWorkspaceRule> props);
        void                      resetProps(eWorkspaceRule prop);

#define COMMA ,
#define DEFINE_PROP(type, name, def, flag)                                                                                                                                         \
  private:                                                                                                                                                                         \
    std::pair<Desktop::Types::COverridableVar<type>, std::underlying_type_t<eWorkspaceRule>> m_##name = {def, WORKSPACE_RULE_PROP_NONE};                                           \
                                                                                                                                                                                   \
  public:                                                                                                                                                                          \
    Desktop::Types::COverridableVar<type>& name() {                                                                                                                                \
        return m_##name.first;                                                                                                                                                     \
    }                                                                                                                                                                              \
    void name##Override(const Desktop::Types::COverridableVar<type>& other) {                                                                                                      \
        m_##name.first = other;                                                                                                                                                    \
    }                                                                                                                                                                              \
    eWorkspaceRule name##Prop() {                                                                                                                                                  \
        return flag;                                                                                                                                                               \
    }

        DEFINE_PROP(std::string, animation, std::string(""), WORKSPACE_RULE_PROP_ANIMATION) // ERSTARR TODO - NEED TO HANDLE THESE PROPERLY!! DEFAULT IS NO OVERRIDE

        DEFINE_PROP(Config::INTEGER, borderSize, {std::string("general:border_size") COMMA static_cast<Config::INTEGER>(0) COMMA std::nullopt}, WORKSPACE_RULE_PROP_BORDER_SIZE)
        DEFINE_PROP(bool, decorate, true, WORKSPACE_RULE_PROP_DECORATE)
        DEFINE_PROP(std::string, defaultName, std::string(""), WORKSPACE_RULE_PROP_DEFAULT_NAME)

        DEFINE_PROP(CCssGapData, floatGaps, {std::string("general:float_gaps") COMMA Config::CCssGapData() COMMA std::nullopt},
                    WORKSPACE_RULE_PROP_GAPS_FLOAT) // ERSTARR TODO - max is def not nullopt
        DEFINE_PROP(CCssGapData, gapsIn, {std::string("general:gaps_in") COMMA Config::CCssGapData() COMMA std::nullopt},
                    WORKSPACE_RULE_PROP_GAPS_IN) // ERSTARR TODO - max is def not nullopt
        DEFINE_PROP(CCssGapData, gapsOut, {std::string("general:gaps_out") COMMA Config::CCssGapData() COMMA std::nullopt},
                    WORKSPACE_RULE_PROP_GAPS_OUT) // ERSTARR TODO - max is def not nullopt

        DEFINE_PROP(std::string, layout, {std::string("general:layout")}, WORKSPACE_RULE_PROP_LAYOUT)

        DEFINE_PROP(std::map<std::string COMMA std::string>, layoutOpts, {}, WORKSPACE_RULE_PROP_LAYOUT_OPTS)

        DEFINE_PROP(std::string, monitor, std::string(""), WORKSPACE_RULE_PROP_MONITOR) // ERSTARR TODO - NEED TO HANDLE THESE PROPERLY!! DEFAULT IS NO OVERRIDE
        DEFINE_PROP(bool, isDefault, false, WORKSPACE_RULE_PROP_DEFAULT)
        DEFINE_PROP(bool, noBorder, false, WORKSPACE_RULE_PROP_BORDER_DISABLE)
        DEFINE_PROP(bool, noRounding, false, WORKSPACE_RULE_PROP_ROUNDING_DISABLE)
        DEFINE_PROP(bool, noShadow, false, WORKSPACE_RULE_PROP_SHADOW_DISABLE)
        DEFINE_PROP(std::string, onCreatedEmpty, std::string(""), WORKSPACE_RULE_PROP_ON_CREATED_EMPTY)
        DEFINE_PROP(bool, persistent, false, WORKSPACE_RULE_PROP_PERSISTENT)

#undef COMMA
#undef DEFINE_PROP

      private:
        PHLWORKSPACE m_workspace;
        bool         needsReLayout();
    };

}