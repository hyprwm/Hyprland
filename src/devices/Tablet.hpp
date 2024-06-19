#pragma once

#include "IHID.hpp"
#include "../helpers/WLListener.hpp"
#include "../macros.hpp"
#include "../helpers/math/Math.hpp"
#include "../helpers/math/Math.hpp"

struct wlr_tablet;
struct wlr_tablet_tool;
struct wlr_tablet_pad;

class CTabletTool;
class CTabletPad;
class CWLSurfaceResource;

/*
    A tablet device
    Tablets don't have an interface for now,
    if there will be a need it's trivial to do.
*/
class CTablet : public IHID {
  public:
    static SP<CTablet> create(wlr_tablet* tablet);
    static SP<CTablet> fromWlr(wlr_tablet* tablet);
    ~CTablet();

    virtual uint32_t getCapabilities();
    virtual eHIDType getType();
    wlr_tablet*      wlr();

    enum eTabletToolAxes {
        HID_TABLET_TOOL_AXIS_X        = (1 << 0),
        HID_TABLET_TOOL_AXIS_Y        = (1 << 1),
        HID_TABLET_TOOL_AXIS_TILT_X   = (1 << 2),
        HID_TABLET_TOOL_AXIS_TILT_Y   = (1 << 3),
        HID_TABLET_TOOL_AXIS_DISTANCE = (1 << 4),
        HID_TABLET_TOOL_AXIS_PRESSURE = (1 << 5),
        HID_TABLET_TOOL_AXIS_ROTATION = (1 << 6),
        HID_TABLET_TOOL_AXIS_SLIDER   = (1 << 7),
        HID_TABLET_TOOL_AXIS_WHEEL    = (1 << 8),
    };

    struct SAxisEvent {
        wlr_tablet_tool* tool;
        SP<CTablet>      tablet;

        uint32_t         timeMs      = 0;
        uint32_t         updatedAxes = 0; // eTabletToolAxes
        Vector2D         axis;
        Vector2D         axisDelta;
        Vector2D         tilt;
        double           pressure   = 0;
        double           distance   = 0;
        double           rotation   = 0;
        double           slider     = 0;
        double           wheelDelta = 0;
    };

    struct SProximityEvent {
        wlr_tablet_tool* tool;
        SP<CTablet>      tablet;

        uint32_t         timeMs = 0;
        Vector2D         proximity;
        bool             in = false;
    };

    struct STipEvent {
        wlr_tablet_tool* tool;
        SP<CTablet>      tablet;

        uint32_t         timeMs = 0;
        Vector2D         tip;
        bool             in = false;
    };

    struct SButtonEvent {
        wlr_tablet_tool* tool;
        SP<CTablet>      tablet;

        uint32_t         timeMs = 0;
        uint32_t         button;
        bool             down = false;
    };

    struct {
        CSignal axis;
        CSignal proximity;
        CSignal tip;
        CSignal button;
    } tabletEvents;

    WP<CTablet> self;

    bool        relativeInput = false;
    std::string hlName        = "";
    std::string boundOutput   = "";
    CBox        activeArea;
    CBox        boundBox; // output-local

  private:
    CTablet(wlr_tablet* tablet);

    void        disconnectCallbacks();

    wlr_tablet* tablet = nullptr;

    DYNLISTENER(destroy);
    DYNLISTENER(axis);
    DYNLISTENER(proximity);
    DYNLISTENER(tip);
    DYNLISTENER(button);
};

class CTabletPad : public IHID {
  public:
    static SP<CTabletPad> create(wlr_tablet_pad* pad);
    ~CTabletPad();

    virtual uint32_t getCapabilities();
    virtual eHIDType getType();
    wlr_tablet_pad*  wlr();

    struct SButtonEvent {
        uint32_t timeMs = 0;
        uint32_t button = 0;
        bool     down   = false;
        uint32_t mode   = 0;
        uint32_t group  = 0;
    };

    struct SRingEvent {
        uint32_t timeMs   = 0;
        bool     finger   = false;
        uint32_t ring     = 0;
        double   position = 0;
        uint32_t mode     = 0;
    };

    struct SStripEvent {
        uint32_t timeMs   = 0;
        bool     finger   = false;
        uint32_t strip    = 0;
        double   position = 0;
        uint32_t mode     = 0;
    };

    struct {
        CSignal button;
        CSignal ring;
        CSignal strip;
        CSignal attach;
    } padEvents;

    WP<CTabletPad>  self;
    WP<CTabletTool> parent;

    std::string     hlName;

  private:
    CTabletPad(wlr_tablet_pad* pad);

    void            disconnectCallbacks();

    wlr_tablet_pad* pad = nullptr;

    DYNLISTENER(destroy);
    DYNLISTENER(ring);
    DYNLISTENER(strip);
    DYNLISTENER(button);
    DYNLISTENER(attach);
};

class CTabletTool : public IHID {
  public:
    static SP<CTabletTool> create(wlr_tablet_tool* tool);
    static SP<CTabletTool> fromWlr(wlr_tablet_tool* tool);
    ~CTabletTool();

    enum eTabletToolType {
        HID_TABLET_TOOL_TYPE_PEN = 1,
        HID_TABLET_TOOL_TYPE_ERASER,
        HID_TABLET_TOOL_TYPE_BRUSH,
        HID_TABLET_TOOL_TYPE_PENCIL,
        HID_TABLET_TOOL_TYPE_AIRBRUSH,
        HID_TABLET_TOOL_TYPE_MOUSE,
        HID_TABLET_TOOL_TYPE_LENS,
        HID_TABLET_TOOL_TYPE_TOTEM,
    };

    enum eTabletToolCapabilities {
        HID_TABLET_TOOL_CAPABILITY_TILT     = (1 << 0),
        HID_TABLET_TOOL_CAPABILITY_PRESSURE = (1 << 1),
        HID_TABLET_TOOL_CAPABILITY_DISTANCE = (1 << 2),
        HID_TABLET_TOOL_CAPABILITY_ROTATION = (1 << 3),
        HID_TABLET_TOOL_CAPABILITY_SLIDER   = (1 << 4),
        HID_TABLET_TOOL_CAPABILITY_WHEEL    = (1 << 5),
    };

    virtual uint32_t       getCapabilities();
    wlr_tablet_tool*       wlr();
    virtual eHIDType       getType();
    SP<CWLSurfaceResource> getSurface();
    void                   setSurface(SP<CWLSurfaceResource>);

    WP<CTabletTool>        self;
    Vector2D               tilt;
    bool                   active           = false; // true if in proximity
    uint32_t               toolCapabilities = 0;

    bool                   isDown = false;
    std::vector<uint32_t>  buttonsDown;
    Vector2D               absolutePos; // last known absolute position.

    std::string            hlName;

  private:
    CTabletTool(wlr_tablet_tool* tool);

    void                   disconnectCallbacks();

    WP<CWLSurfaceResource> pSurface;

    wlr_tablet_tool*       tool = nullptr;

    DYNLISTENER(destroy);

    struct {
        CHyprSignalListener destroySurface;
    } listeners;
};