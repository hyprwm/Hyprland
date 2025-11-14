#pragma once

#include <string>
#include <vector>
#include <type_traits>

namespace Desktop::Rule {
    template <typename T>
    class IEffectContainer {
        static_assert(std::is_enum_v<T>);

      protected:
        const std::string DEFAULT_MISSING_KEY = "";

      public:
        // Make sure we're using at least a uint16_t for dynamic registrations to not overflow.
        // 32k should be enough
        using storageType = std::conditional_t<(sizeof(std::underlying_type_t<T>) >= 2), std::underlying_type_t<T>, std::uint16_t>;

        IEffectContainer(std::vector<std::string>&& defaultKeys) : m_keys(std::move(defaultKeys)), m_originalSize(m_keys.size()) {
            ;
        }
        virtual ~IEffectContainer() = default;

        virtual storageType registerEffect(std::string&& name) {
            if (m_keys.size() >= std::numeric_limits<storageType>::max())
                return 0;
            if (auto it = std::ranges::find(m_keys, name); it != m_keys.end())
                return it - m_keys.begin();
            m_keys.emplace_back(std::move(name));
            return m_keys.size() - 1;
        }

        virtual void unregisterEffect(storageType id) {
            if (id >= m_keys.size())
                return;

            m_keys[id] = DEFAULT_MISSING_KEY;
        }

        virtual void unregisterEffect(const std::string& name) {
            for (auto& key : m_keys) {
                if (key == name) {
                    key = DEFAULT_MISSING_KEY;
                    break;
                }
            }
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
        std::vector<std::string> m_keys;
        size_t                   m_originalSize = 0;
    };
};