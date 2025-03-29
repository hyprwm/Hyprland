#pragma once

#include "../../helpers/math/Math.hpp"
#include "../WaylandProtocol.hpp"
#include "./Buffer.hpp"

class CTexture;
class CDRMSyncPointState;

struct SSurfaceState {
    enum eUpdatedProperties : uint16_t {
        SURFACE_UPDATED_BUFFER    = 1 << 0,
        SURFACE_UPDATED_DAMAGE    = 1 << 1,
        SURFACE_UPDATED_OPAQUE    = 1 << 2,
        SURFACE_UPDATED_INPUT     = 1 << 3,
        SURFACE_UPDATED_TRANSFORM = 1 << 4,
        SURFACE_UPDATED_SCALE     = 1 << 5,
        SURFACE_UPDATED_OFFSET    = 1 << 6,
        SURFACE_UPDATED_VIEWPORT  = 1 << 7,
        SURFACE_UPDATED_ACQUIRE   = 1 << 8,
    };

    uint16_t updated  = 0; // eUpdatedProperties. Stores pending state updates
    bool     ready    = false;
    bool     rejected = false;

    // initial values, copied from protocol text
    CHLBufferReference  buffer = {};                                  // The initial surface contents are void
    CRegion             damage, bufferDamage;                         // The initial value for pending damage is empty
    CRegion             opaque;                                       // The initial value for an opaque region is empty
    CRegion             input     = CBox{{}, {INT32_MAX, INT32_MAX}}; // The initial value for an input region is infinite
    wl_output_transform transform = WL_OUTPUT_TRANSFORM_NORMAL;       // A newly created surface has its buffer transformation set to normal
    int                 scale     = 1;                                // A newly created surface has its buffer scale set to 1

    // these don't have well defined initial values in the protocol, but these work
    Vector2D size, bufferSize;
    Vector2D offset;

    // viewporter protocol surface state
    struct {
        bool     hasDestination = false;
        bool     hasSource      = false;
        Vector2D destination;
        CBox     source;
    } viewport;
    Vector2D sourceSize();

    // drm syncobj protocol surface state
    UP<CDRMSyncPointState> acquire;

    // texture of surface content, used for rendering
    SP<CTexture> texture;
    void         updateSynchronousTexture(SP<CTexture> lastTexture);

    // helpers
    CRegion accumulateBufferDamage();       // transforms damage and merges it into bufferDamage
    void    updateFrom(SSurfaceState& ref); // updates this state based on a reference state.
    void    reset();                        // resets pending state after commit
};
