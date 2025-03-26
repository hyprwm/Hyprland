#pragma once

#include "../../helpers/math/Math.hpp"
#include "../WaylandProtocol.hpp"

class CHLBufferReference;
class CTexture;

struct SSurfaceState {
    enum eUpdatedProperties : uint8_t {
        SURFACE_UPDATED_OPAQUE    = 1 << 0,
        SURFACE_UPDATED_INPUT     = 1 << 1,
        SURFACE_UPDATED_DAMAGE    = 1 << 2,
        SURFACE_UPDATED_SCALE     = 1 << 3,
        SURFACE_UPDATED_BUFFER    = 1 << 4,
        SURFACE_UPDATED_OFFSET    = 1 << 5,
        SURFACE_UPDATED_VIEWPORT  = 1 << 6,
        SURFACE_UPDATED_TRANSFORM = 1 << 7,
    };

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
    bool     rejected = false;
    uint8_t  updated  = 0; // eUpdatedProperties. Stores what the last update changed

    Vector2D sourceSize();
    // Translates damage into bufferDamage, clearing damage and returning the updated bufferDamage
    CRegion accumulateBufferDamage();
    void    updateSynchronousTexture(SP<CTexture> lastTexture);
    void    reset();
    // updates this state from a reference state. Mutates the reference state. If a new buffer is committed,
    // reference state gets its damage and buffer cleared.
    void updateFrom(SSurfaceState& ref);
};
