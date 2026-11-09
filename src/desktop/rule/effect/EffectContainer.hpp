#pragma once

#include <string>
#include <vector>
#include <type_traits>
#include <cstdint>
#include <optional>
#include <algorithm>
#include "../../../config/values/types/IValue.hpp"

namespace Desktop::Rule {
    template <typename T>
    class IEffectContainer {
        static_assert(std::is_enum_v<T>);

      protected:
        const std::string DEFAULT_MISSING_KEY = "";

      public:
        // Make sure we're using at least a uint16_t for dynamic registrations to not overflow.
        // 32k should be enough
        using storageType = std::conditional_t<(sizeof(std::underlying_type_t<T>) >= 2), std::underlying_type_t<T>, uint16_t>;

        IEffectContainer(std::vector<std::string>&& defaultKeys) : m_keys(std::move(defaultKeys)), m_originalSize(m_keys.size()) {
            m_luaValues.resize(m_originalSize);
        }
        virtual ~IEffectContainer() = default;

        virtual storageType registerEffect(std::string&& name, SP<Config::Values::IValue> value) {
            if (!value || m_keys.size() >= std::numeric_limits<storageType>::max())
                return 0;
            if (auto it = std::ranges::find(m_keys, name); it != m_keys.end()) {
                const auto IDX = sc<storageType>(it - m_keys.begin());
                if (!isEffectDynamic(IDX))
                    return 0;

                m_luaValues[IDX] = std::move(value);
                return IDX;
            }
            m_keys.emplace_back(std::move(name));
            m_luaValues.emplace_back(std::move(value));
            return m_keys.size() - 1;
        }

        virtual void unregisterEffect(storageType id) {
            if (id >= m_keys.size())
                return;

            m_keys[id] = DEFAULT_MISSING_KEY;
            if (id < m_luaValues.size())
                m_luaValues[id] = nullptr;
        }

        virtual void unregisterEffect(const std::string& name) {
            for (storageType i = 0; i < m_keys.size(); ++i) {
                if (m_keys[i] == name) {
                    unregisterEffect(i);
                    break;
                }
            }
        }

        virtual SP<Config::Values::IValue> getLuaValue(storageType id) const {
            if (id >= m_luaValues.size())
                return nullptr;

            return m_luaValues[id];
        }

        virtual const std::string& get(storageType idx) {
            if (idx >= m_keys.size())
                return DEFAULT_MISSING_KEY;

            return m_keys[idx];
        }

        virtual std::optional<storageType> get(const std::string_view& s) {
            for (storageType i = 0; i < m_keys.size(); ++i) {
                if (m_keys[i] == s)
                    return i;
            }

            return std::nullopt;
        }

        virtual const std::vector<std::string>& allEffectStrings() {
            return m_keys;
        }

        // whether the effect has been added dynamically as opposed to in the ctor.
        virtual bool isEffectDynamic(storageType i) {
            return i >= m_originalSize;
        }

      protected:
        // Static effects are registered by their derived container during construction.
        // Dynamic effects are registered through registerEffect().
        void registerStaticEffect(storageType id, SP<Config::Values::IValue> value) {
            if (!value || id == 0 || id >= m_originalSize)
                return;

            m_luaValues[id] = std::move(value);
        }

        std::vector<std::string>                m_keys;
        std::vector<SP<Config::Values::IValue>> m_luaValues;
        size_t                                  m_originalSize = 0;
    };
};
