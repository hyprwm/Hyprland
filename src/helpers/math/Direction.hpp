#pragma once

#include <cstdint>

namespace Math {
    enum eDirection : int8_t {
        DIRECTION_DEFAULT = -1,
        DIRECTION_UP,
        DIRECTION_RIGHT,
        DIRECTION_DOWN,
        DIRECTION_LEFT
    };

    inline eDirection fromChar(char x) {
        switch (x) {
            case 'r': return DIRECTION_RIGHT;
            case 'l': return DIRECTION_LEFT;
            case 't':
            case 'u': return DIRECTION_UP;
            case 'b':
            case 'd': return DIRECTION_DOWN;
            default: return DIRECTION_DEFAULT;
        }
    }

    inline const char* toString(eDirection d) {
        switch (d) {
            case DIRECTION_UP: return "up";
            case DIRECTION_DOWN: return "down";
            case DIRECTION_LEFT: return "left";
            case DIRECTION_RIGHT: return "right";
            default: return "default";
        }
    }
};