#pragma once

#include "../RuleWithEffects.hpp"
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
        using storageType = CWindowRuleEffectContainer::storageType;
        using valueType   = WindowRuleEffectValue;

        CWindowRuleEffectContainer::storageType key = WINDOW_RULE_EFFECT_NONE;
        std::string                             raw;
        WindowRuleEffectValue                   value;
    };

    class CWindowRule : public CRuleWithEffects<SWindowRuleEffect, RULE_TYPE_WINDOW> {
      private:
        using Base        = CRuleWithEffects<SWindowRuleEffect, RULE_TYPE_WINDOW>;
        using storageType = Base::storageType;

      public:
        CWindowRule(const std::string& name = "");
        virtual ~CWindowRule() = default;

        CWindowRule(const CWindowRule&) = default;
        CWindowRule(CWindowRule&)       = default;
        CWindowRule(CWindowRule&&)      = default;

        static std::expected<SP<CWindowRule>, std::string> buildFromExecString(std::string&&);

        bool                                               matches(PHLWINDOW w, bool allowEnvLookup = false);

      private:
        std::expected<WindowRuleEffectValue, std::string> parseEffect(storageType e, const std::string& result) override;
    };
};
