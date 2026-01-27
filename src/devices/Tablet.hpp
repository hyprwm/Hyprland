#pragma once

#include "IHID.hpp"
#include "../macros.hpp"
#include "../helpers/math/Math.hpp"

AQUAMARINE_FORWARD(ITablet);
AQUAMARINE_FORWARD(ITabletTool);
AQUAMARINE_FORWARD(ITabletPad);

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
    static SP<CTablet> create(SP<Aquamarine::ITablet> tablet);
    ~CTablet();

    virtual uint32_t        getCapabilities();
    virtual eHIDType        getType();
    SP<Aquamarine::ITablet> aq();

    enum eTabletToolAxes : uint16_t {
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
        SP<Aquamarine::ITabletTool> tool;
        SP<CTablet>                 tablet;

        uint32_t                    timeMs      = 0;
        uint32_t                    updatedAxes = 0; // eTabletToolAxes
        Vector2D                    axis;
        Vector2D                    axisDelta;
        Vector2D                    tilt;
        double                      pressure   = 0;
        double                      distance   = 0;
        double                      rotation   = 0;
        double                      slider     = 0;
        double                      wheelDelta = 0;
    };

    struct SProximityEvent {
        SP<Aquamarine::ITabletTool> tool;
        SP<CTablet>                 tablet;

        uint32_t                    timeMs = 0;
        Vector2D                    proximity;
        bool                        in = false;
    };

    struct STipEvent {
        SP<Aquamarine::ITabletTool> tool;
        SP<CTablet>                 tablet;

        uint32_t                    timeMs = 0;
        Vector2D                    tip;
        bool                        in = false;
    };

    struct SButtonEvent {
        SP<Aquamarine::ITabletTool> tool;
        SP<CTablet>                 tablet;

        uint32_t                    timeMs = 0;
        uint32_t                    button;
        bool                        down = false;
    };

    struct {
        CSignalT<SAxisEvent>      axis;
        CSignalT<SProximityEvent> proximity;
        CSignalT<STipEvent>       tip;
        CSignalT<SButtonEvent>    button;
    } m_tabletEvents;

    WP<CTablet> m_self;

    bool        m_relativeInput = false;
    bool        m_absolutePos   = false;
    std::string m_boundOutput   = "";
    CBox        m_activeArea;
    CBox        m_boundBox;

  private:
    CTablet(SP<Aquamarine::ITablet> tablet);

    WP<Aquamarine::ITablet> m_tablet;

    struct {
        CHyprSignalListener destroy;
        CHyprSignalListener axis;
        CHyprSignalListener proximity;
        CHyprSignalListener tip;
        CHyprSignalListener button;
    } m_listeners;
};

class CTabletPad : public IHID {
  public:
    static SP<CTabletPad> create(SP<Aquamarine::ITabletPad> pad);
    ~CTabletPad();

    virtual uint32_t           getCapabilities();
    virtual eHIDType           getType();
    SP<Aquamarine::ITabletPad> aq();

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
        CSignalT<SButtonEvent>    button;
        CSignalT<SRingEvent>      ring;
        CSignalT<SStripEvent>     strip;
        CSignalT<SP<CTabletTool>> attach;
    } m_padEvents;

    WP<CTabletPad>  m_self;
    WP<CTabletTool> m_parent;

  private:
    CTabletPad(SP<Aquamarine::ITabletPad> pad);

    WP<Aquamarine::ITabletPad> m_pad;

    struct {
        CHyprSignalListener destroy;
        CHyprSignalListener ring;
        CHyprSignalListener strip;
        CHyprSignalListener button;
        CHyprSignalListener attach;
    } m_listeners;
};

class CTabletTool : public IHID {
  public:
    static SP<CTabletTool> create(SP<Aquamarine::ITabletTool> tool);
    ~CTabletTool();

    enum eTabletToolType : uint8_t {
        HID_TABLET_TOOL_TYPE_PEN = 1,
        HID_TABLET_TOOL_TYPE_ERASER,
        HID_TABLET_TOOL_TYPE_BRUSH,
        HID_TABLET_TOOL_TYPE_PENCIL,
        HID_TABLET_TOOL_TYPE_AIRBRUSH,
        HID_TABLET_TOOL_TYPE_MOUSE,
        HID_TABLET_TOOL_TYPE_LENS,
        HID_TABLET_TOOL_TYPE_TOTEM,
    };

    enum eTabletToolCapabilities : uint8_t {
        HID_TABLET_TOOL_CAPABILITY_TILT     = (1 << 0),
        HID_TABLET_TOOL_CAPABILITY_PRESSURE = (1 << 1),
        HID_TABLET_TOOL_CAPABILITY_DISTANCE = (1 << 2),
        HID_TABLET_TOOL_CAPABILITY_ROTATION = (1 << 3),
        HID_TABLET_TOOL_CAPABILITY_SLIDER   = (1 << 4),
        HID_TABLET_TOOL_CAPABILITY_WHEEL    = (1 << 5),
    };

    virtual uint32_t            getCapabilities();
    SP<Aquamarine::ITabletTool> aq();
    virtual eHIDType            getType();
    SP<CWLSurfaceResource>      getSurface();
    void                        setSurface(SP<CWLSurfaceResource>);

    WP<CTabletTool>             m_self;
    Vector2D                    m_tilt;
    bool                        m_active           = false; // true if in proximity
    uint32_t                    m_toolCapabilities = 0;

    bool                        m_isDown = false;
    std::vector<uint32_t>       m_buttonsDown;
    Vector2D                    m_absolutePos; // last known absolute position.

  private:
    CTabletTool(SP<Aquamarine::ITabletTool> tool);

    WP<CWLSurfaceResource>      m_surface;
    WP<Aquamarine::ITabletTool> m_tool;

    struct {
        CHyprSignalListener destroySurface;
        CHyprSignalListener destroyTool;
    } m_listeners;
};
