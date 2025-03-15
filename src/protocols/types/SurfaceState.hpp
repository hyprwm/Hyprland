#pragma once

#include "../../helpers/math/Math.hpp"
#include "../WaylandProtocol.hpp"

class CHLBufferReference;
class CTexture;

struct SSurfaceState {
    CRegion                opaque, input = CBox{{}, {INT32_MAX, INT32_MAX}}, damage, bufferDamage = CBox{{}, {INT32_MAX, INT32_MAX}} /* initial damage */;
    wl_output_transform    transform = WL_OUTPUT_TRANSFORM_NORMAL;
    int                    scale     = 1;
    SP<CHLBufferReference> buffer; // buffer ref will be released once the buffer is no longer locked. For checking if a buffer is attached to this state, check texture.
    SP<CTexture>           texture;
    Vector2D               offset;
    Vector2D               size, bufferSize;
    struct {
        bool     hasDestination = false;
        bool     hasSource      = false;
        Vector2D destination;
        CBox     source;
    } viewport;
    bool rejected  = false;
    bool newBuffer = false;

    //
    void reset() {
        damage.clear();
        bufferDamage.clear();
        transform = WL_OUTPUT_TRANSFORM_NORMAL;
        scale     = 1;
        offset    = {};
        size      = {};
    }
};
