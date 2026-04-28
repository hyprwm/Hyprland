#pragma once

#include "Rule.hpp"

#include <expected>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace Desktop::Rule {
    template <typename TEffect, eRuleType RULE_TYPE>
    class CRuleWithEffects : public IRule {
      public:
        using effectType  = TEffect;
        using storageType = TEffect::storageType;
        using valueType   = TEffect::valueType;

        eRuleType type() override {
            return RULE_TYPE;
        }

        std::expected<void, std::string> addEffect(storageType e, const std::string& result) {
            auto parsed = parseEffect(e, result);
            if (!parsed)
                return std::unexpected(parsed.error());

            m_effects.emplace_back(TEffect{.key = e, .raw = result, .value = std::move(*parsed)});
            m_effectSet.emplace(e);

            return {};
        }

        const std::vector<TEffect>& effects() {
            return m_effects;
        }

        const std::unordered_set<storageType>& effectsSet() {
            return m_effectSet;
        }

      protected:
        CRuleWithEffects(const std::string& name = "") : IRule(name) {}

        virtual std::expected<valueType, std::string> parseEffect(storageType e, const std::string& result) = 0;

        std::expected<void, std::string>              addParsedEffect(storageType e, valueType value, std::string raw = {}) {
            m_effects.emplace_back(TEffect{.key = e, .raw = std::move(raw), .value = std::move(value)});
            m_effectSet.emplace(e);

            return {};
        }

      private:
        std::vector<TEffect>            m_effects;
        std::unordered_set<storageType> m_effectSet;
    };
}
