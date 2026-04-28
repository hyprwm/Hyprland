#pragma once

#include "../Rule.hpp"
#include "../../DesktopTypes.hpp"
#include "../../types/OverridableVar.hpp"
#include "WindowRuleEffectContainer.hpp"
#include "../../../config/shared/complex/ComplexDataTypes.hpp"
#include "../../../helpers/math/Math.hpp"

#include <expected>
#include <unordered_set>
#include <variant>

namespace Desktop::Rule {
    constexpr const char* EXEC_RULE_ENV_NAME = "HL_EXEC_RULE_TOKEN";

    struct SFullscreenStateRule {
        int                internal = 0;
        std::optional<int> client;
    };

    struct SOpacityRule {
        Types::SAlphaValue alpha;
        Types::SAlphaValue alphaInactive;
        Types::SAlphaValue alphaFullscreen;
    };

    struct SBorderColorRule {
        Config::CGradientValueData                active;
        std::optional<Config::CGradientValueData> inactive;
    };

    using WindowRuleEffectValue = std::variant<std::monostate, bool, int64_t, float, std::string, std::vector<std::string>, SFullscreenStateRule, SOpacityRule, SBorderColorRule>;

    struct SWindowRuleEffect {
        CWindowRuleEffectContainer::storageType key = WINDOW_RULE_EFFECT_NONE;
        std::string                             raw;
        WindowRuleEffectValue                   value;
    };

    class CWindowRule : public IRule {
      private:
        using storageType = CWindowRuleEffectContainer::storageType;

      public:
        CWindowRule(const std::string& name = "");
        virtual ~CWindowRule() = default;

        CWindowRule(const CWindowRule&) = default;
        CWindowRule(CWindowRule&)       = default;
        CWindowRule(CWindowRule&&)      = default;

        static std::expected<SP<CWindowRule>, std::string> buildFromExecString(std::string&&);

        virtual eRuleType                                  type();

        std::expected<void, std::string>                   addEffect(storageType e, const std::string& result);
        const std::vector<SWindowRuleEffect>&              effects();
        const std::unordered_set<storageType>&             effectsSet();

        void                                               setEnabled(bool enable);
        bool                                               isEnabled() const;

        bool                                               matches(PHLWINDOW w, bool allowEnvLookup = false);

      private:
        std::vector<SWindowRuleEffect>  m_effects;
        std::unordered_set<storageType> m_effectSet;
        bool                            m_enabled = true;
    };
};
