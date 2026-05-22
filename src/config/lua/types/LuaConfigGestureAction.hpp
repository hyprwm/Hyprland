#pragma once

#define GESTURE_ACTIONS_LIST(X)                                                                                                                                                    \
    X(UNSET)                                                                                                                                                                       \
    X(WORKSPACE)                                                                                                                                                                   \
    X(RESIZE)                                                                                                                                                                      \
    X(MOVE)                                                                                                                                                                        \
    X(SPECIAL)                                                                                                                                                                     \
    X(CLOSE)                                                                                                                                                                       \
    X(FLOAT)                                                                                                                                                                       \
    X(FULLSCREEN)                                                                                                                                                                  \
    X(CURSOR_ZOOM)                                                                                                                                                                 \
    X(SCROLL_MOVE)

enum class eGestureAction : std::uint8_t {
#define AS_ENUM_VARIANT(name) name,
    GESTURE_ACTIONS_LIST(AS_ENUM_VARIANT)
#undef AS_ENUM_VARIANT
    COUNT
};

inline constexpr std::array<const char*, static_cast<size_t>(eGestureAction::COUNT)> GESTURE_ACTION_NAMES = {
#define AS_STRING_LITERAL(name) #name,
    GESTURE_ACTIONS_LIST(AS_STRING_LITERAL)
#undef AS_STRING_LITERAL
};
