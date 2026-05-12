#pragma once

#include <array>
#include <cstddef>

#include "../../helpers/AnimatedVariable.hpp"

namespace Desktop::Types {

    template <typename T>
    struct SAddOperation {
        static constexpr T identity() {
            return T{};
        }

        static T apply(const T& lhs, const T& rhs) {
            return lhs + rhs;
        }
    };

    template <typename T>
    struct SMultiplyOperation {
        static constexpr T identity() {
            return T{1};
        }

        static T apply(const T& lhs, const T& rhs) {
            return lhs * rhs;
        }
    };

    template <Animable VarType, typename Key, size_t Count, typename Operation = SMultiplyOperation<VarType>>
    class CMultiAnimatedVariableContainer {
      public:
        using value_type     = VarType;
        using key_type       = Key;
        using operation_type = Operation;

        static constexpr size_t size() {
            return Count;
        }

        PHLANIMVAR<VarType>& get(const Key key) {
            return m_vars.at(index(key));
        }

        const PHLANIMVAR<VarType>& get(const Key key) const {
            return m_vars.at(index(key));
        }

        PHLANIMVAR<VarType>& operator[](const Key key) {
            return get(key);
        }

        const PHLANIMVAR<VarType>& operator[](const Key key) const {
            return get(key);
        }

        PHLANIMVAR<VarType>& raw(const size_t i) {
            return m_vars.at(i);
        }

        const PHLANIMVAR<VarType>& raw(const size_t i) const {
            return m_vars.at(i);
        }

        std::array<PHLANIMVAR<VarType>, Count>& all() {
            return m_vars;
        }

        const std::array<PHLANIMVAR<VarType>, Count>& all() const {
            return m_vars;
        }

        template <typename Fn>
        void forEach(Fn&& fn) {
            for (size_t i = 0; i < Count; ++i)
                fn(static_cast<Key>(i), m_vars.at(i));
        }

        template <typename Fn>
        void forEach(Fn&& fn) const {
            for (size_t i = 0; i < Count; ++i)
                fn(static_cast<Key>(i), m_vars.at(i));
        }

        VarType getTotal() const {
            return accumulate([](const auto& var) { return var->value(); });
        }

        VarType getTotalGoal() const {
            return accumulate([](const auto& var) { return var->goal(); });
        }

        VarType getTotalBegun() const {
            return accumulate([](const auto& var) { return var->begun(); });
        }

        VarType getTotalWithout(const Key key) const {
            return accumulateExcept(key, [](const auto& var) { return var->value(); });
        }

        VarType getTotalGoalWithout(const Key key) const {
            return accumulateExcept(key, [](const auto& var) { return var->goal(); });
        }

        VarType getTotalBegunWithout(const Key key) const {
            return accumulateExcept(key, [](const auto& var) { return var->begun(); });
        }

        VarType value() const {
            return getTotal();
        }

        VarType goal() const {
            return getTotalGoal();
        }

        bool isBeingAnimated() const {
            for (const auto& var : m_vars) {
                if (var && var->isBeingAnimated())
                    return true;
            }

            return false;
        }

        bool initialized() const {
            for (const auto& var : m_vars) {
                if (!var)
                    return false;
            }

            return true;
        }

        void warp(const bool endCallback = true) {
            for (auto& var : m_vars) {
                if (var)
                    var->warp(endCallback);
            }
        }

      private:
        static constexpr size_t index(const Key key) {
            return static_cast<size_t>(key);
        }

        template <typename Getter>
        VarType accumulate(Getter&& getter) const {
            VarType result = Operation::identity();

            for (const auto& var : m_vars) {
                if (!var)
                    continue;

                result = Operation::apply(result, getter(var));
            }

            return result;
        }

        template <typename Getter>
        VarType accumulateExcept(const Key key, Getter&& getter) const {
            VarType      result = Operation::identity();
            const size_t except = index(key);

            for (size_t i = 0; i < Count; ++i) {
                if (i == except || !m_vars.at(i))
                    continue;

                result = Operation::apply(result, getter(m_vars.at(i)));
            }

            return result;
        }

        std::array<PHLANIMVAR<VarType>, Count> m_vars;
    };

    template <Animable VarType, typename Key, size_t Count, typename Operation = SMultiplyOperation<VarType>>
    using CMultiAVarContainer = CMultiAnimatedVariableContainer<VarType, Key, Count, Operation>;

}
