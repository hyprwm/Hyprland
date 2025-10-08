#pragma once

#include "../../helpers/math/Math.hpp"
#include "../WaylandProtocol.hpp"
#include "./Buffer.hpp"

class CTexture;
class CDRMSyncPointState;
class CWLCallbackResource;

struct SSurfaceState {
    union {
        uint16_t all = 0;
        struct {
            bool buffer : 1;
            bool damage : 1;
            bool opaque : 1;
            bool input : 1;
            bool transform : 1;
            bool scale : 1;
            bool offset : 1;
            bool viewport : 1;
            bool acquire : 1;
            bool acked : 1;
            bool frame : 1;
        } bits;
    } updated;

    bool rejected = false;

    // initial values, copied from protocol text
    CHLBufferReference  buffer = {};                                          // The initial surface contents are void
    CRegion             damage, bufferDamage;                                 // The initial value for pending damage is empty
    CRegion             opaque;                                               // The initial value for an opaque region is empty
    CRegion             input     = CBox{{}, {INT32_MAX - 1, INT32_MAX - 1}}; // The initial value for an input region is infinite
    wl_output_transform transform = WL_OUTPUT_TRANSFORM_NORMAL;               // A newly created surface has its buffer transformation set to normal
    int                 scale     = 1;                                        // A newly created surface has its buffer scale set to 1

    // these don't have well defined initial values in the protocol, but these work
    Vector2D size, bufferSize;
    Vector2D offset;

    // for xdg_shell resizing
    Vector2D ackedSize;

    // for wl_surface::frame callbacks.
    std::vector<SP<CWLCallbackResource>> callbacks;

    // viewporter protocol surface state
    struct {
        bool     hasDestination = false;
        bool     hasSource      = false;
        Vector2D destination;
        CBox     source;
    } viewport;
    Vector2D sourceSize();

    // drm syncobj protocol surface state
    CDRMSyncPointState acquire;

    // texture of surface content, used for rendering
    SP<CTexture> texture;
    void         updateSynchronousTexture(SP<CTexture> lastTexture);
    void         updateAsyncSynchronousTexture();

    // helpers
    CRegion accumulateBufferDamage();       // transforms state.damage and merges it into state.bufferDamage
    void    updateFrom(SSurfaceState& ref); // updates this state based on a reference state.
    void    reset();                        // resets pending state after commit
};
