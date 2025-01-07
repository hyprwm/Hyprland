#pragma once

#include <hyprutils/animation/AnimatedVariable.hpp>

#include "Color.hpp"
#include "../defines.hpp"
#include "../desktop/DesktopTypes.hpp"

enum eAVarDamagePolicy : int8_t {
    AVARDAMAGE_NONE   = -1,
    AVARDAMAGE_ENTIRE = 0,
    AVARDAMAGE_BORDER,
    AVARDAMAGE_SHADOW
};

enum eAnimatedVarType : int8_t {
    AVARTYPE_INVALID = -1,
    AVARTYPE_FLOAT,
    AVARTYPE_VECTOR,
    AVARTYPE_COLOR
};

// Utility to bind a type with its corresponding eAnimatedVarType
template <class T>
// NOLINTNEXTLINE(readability-identifier-naming)
struct STypeToAnimatedVarType_t {
    static constexpr eAnimatedVarType value = AVARTYPE_INVALID;
};

template <>
struct STypeToAnimatedVarType_t<float> {
    static constexpr eAnimatedVarType value = AVARTYPE_FLOAT;
};

template <>
struct STypeToAnimatedVarType_t<Vector2D> {
    static constexpr eAnimatedVarType value = AVARTYPE_VECTOR;
};

template <>
struct STypeToAnimatedVarType_t<CHyprColor> {
    static constexpr eAnimatedVarType value = AVARTYPE_COLOR;
};

template <class T>
inline constexpr eAnimatedVarType typeToeAnimatedVarType = STypeToAnimatedVarType_t<T>::value;

// Utility to define a concept as a list of possible type
template <class T, class... U>
concept OneOf = (... or std::same_as<T, U>);

// Concept to describe which type can be placed into CAnimatedVariable
// This is mainly to get better errors if we put a type that's not supported
// Otherwise template errors are ugly
template <class T>
concept Animable = OneOf<T, Vector2D, float, CHyprColor>;

struct SAnimationContext {
    PHLWINDOWREF      pWindow;
    PHLWORKSPACEREF   pWorkspace;
    PHLLSREF          pLayer;

    eAVarDamagePolicy eDamagePolicy = AVARDAMAGE_NONE;
};

template <Animable VarType>
using CAnimatedVariable = Hyprutils::Animation::CGenericAnimatedVariable<VarType, SAnimationContext>;

template <Animable VarType>
using PHLANIMVAR = SP<CAnimatedVariable<VarType>>;

template <Animable VarType>
using PHLANIMVARREF = WP<CAnimatedVariable<VarType>>;
