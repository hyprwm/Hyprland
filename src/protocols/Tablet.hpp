#pragma once

#include <vector>
#include <cstdint>
#include "WaylandProtocol.hpp"
#include "tablet-v2.hpp"
#include "../helpers/math/Math.hpp"
#include <aquamarine/input/Input.hpp>

class CTablet;
class CTabletTool;
class CTabletPad;
class CEventLoopTimer;
class CTabletSeat;
class CWLSurfaceResource;

class CTabletPadStripV2Resource {
  public:
    CTabletPadStripV2Resource(SP<CZwpTabletPadStripV2> resource_, uint32_t id);

    bool     good();

    uint32_t m_id = 0;

  private:
    SP<CZwpTabletPadStripV2> m_resource;

    friend class CTabletSeat;
    friend class CTabletPadGroupV2Resource;
    friend class CTabletV2Protocol;
};

class CTabletPadRingV2Resource {
  public:
    CTabletPadRingV2Resource(SP<CZwpTabletPadRingV2> resource_, uint32_t id);

    bool     good();

    uint32_t m_id = 0;

  private:
    SP<CZwpTabletPadRingV2> m_resource;

    friend class CTabletSeat;
    friend class CTabletPadGroupV2Resource;
    friend class CTabletV2Protocol;
};

class CTabletPadGroupV2Resource {
  public:
    CTabletPadGroupV2Resource(SP<CZwpTabletPadGroupV2> resource_, size_t idx);

    bool   good();
    void   sendData(SP<CTabletPad> pad, SP<Aquamarine::ITabletPad::STabletPadGroup> group);

    size_t m_idx = 0;

  private:
    SP<CZwpTabletPadGroupV2> m_resource;

    friend class CTabletSeat;
    friend class CTabletPadV2Resource;
    friend class CTabletV2Protocol;
};

class CTabletPadV2Resource {
  public:
    CTabletPadV2Resource(SP<CZwpTabletPadV2> resource_, SP<CTabletPad> pad_, SP<CTabletSeat> seat_);

    bool                                       good();
    void                                       sendData();

    std::vector<WP<CTabletPadGroupV2Resource>> m_groups;

    WP<CTabletPad>                             m_pad;
    WP<CTabletSeat>                            m_seat;

    bool                                       m_inert = false; // removed was sent

  private:
    SP<CZwpTabletPadV2> m_resource;

    void                createGroup(SP<Aquamarine::ITabletPad::STabletPadGroup> group, size_t idx);

    friend class CTabletSeat;
    friend class CTabletV2Protocol;
};

class CTabletV2Resource {
  public:
    CTabletV2Resource(SP<CZwpTabletV2> resource_, SP<CTablet> tablet_, SP<CTabletSeat> seat_);

    bool            good();
    void            sendData();

    WP<CTablet>     m_tablet;
    WP<CTabletSeat> m_seat;

    bool            m_inert = false; // removed was sent

  private:
    SP<CZwpTabletV2> m_resource;

    friend class CTabletSeat;
    friend class CTabletV2Protocol;
};

class CTabletToolV2Resource {
  public:
    CTabletToolV2Resource(SP<CZwpTabletToolV2> resource_, SP<CTabletTool> tool_, SP<CTabletSeat> seat_);
    ~CTabletToolV2Resource();

    bool                   good();
    void                   sendData();
    void                   queueFrame();
    void                   sendFrame(bool removeSource = true);

    bool                   m_current = false;
    WP<CWLSurfaceResource> m_lastSurf;

    WP<CTabletTool>        m_tool;
    WP<CTabletSeat>        m_seat;
    wl_event_source*       m_frameSource = nullptr;

    bool                   m_inert = false; // removed was sent

  private:
    SP<CZwpTabletToolV2> m_resource;

    friend class CTabletSeat;
    friend class CTabletV2Protocol;
};

class CTabletSeat {
  public:
    CTabletSeat(SP<CZwpTabletSeatV2> resource_);

    bool                                   good();
    void                                   sendData();

    std::vector<WP<CTabletToolV2Resource>> m_tools;
    std::vector<WP<CTabletPadV2Resource>>  m_pads;
    std::vector<WP<CTabletV2Resource>>     m_tablets;

    void                                   sendTool(SP<CTabletTool> tool);
    void                                   sendPad(SP<CTabletPad> pad);
    void                                   sendTablet(SP<CTablet> tablet);

  private:
    SP<CZwpTabletSeatV2> m_resource;
    WP<CTabletSeat>      m_self;

    friend class CTabletV2Protocol;
};

class CTabletV2Protocol : public IWaylandProtocol {
  public:
    CTabletV2Protocol(const wl_interface* iface, const int& ver, const std::string& name);

    virtual void bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id);

    void         registerDevice(SP<CTablet> tablet);
    void         registerDevice(SP<CTabletTool> tool);
    void         registerDevice(SP<CTabletPad> pad);

    void         unregisterDevice(SP<CTablet> tablet);
    void         unregisterDevice(SP<CTabletTool> tool);
    void         unregisterDevice(SP<CTabletPad> pad);

    void         recheckRegisteredDevices();

    // Tablet tool events
    void pressure(SP<CTabletTool> tool, double value);
    void distance(SP<CTabletTool> tool, double value);
    void rotation(SP<CTabletTool> tool, double value);
    void slider(SP<CTabletTool> tool, double value);
    void wheel(SP<CTabletTool> tool, double value);
    void tilt(SP<CTabletTool> tool, const Vector2D& value);
    void up(SP<CTabletTool> tool);
    void down(SP<CTabletTool> tool);
    void proximityIn(SP<CTabletTool> tool, SP<CTablet> tablet, SP<CWLSurfaceResource> surf);
    void proximityOut(SP<CTabletTool> tool);
    void buttonTool(SP<CTabletTool> tool, uint32_t button, uint32_t state);
    void motion(SP<CTabletTool> tool, const Vector2D& value);

    // Tablet pad events
    void mode(SP<CTabletPad> pad, uint32_t group, uint32_t mode, uint32_t timeMs);
    void buttonPad(SP<CTabletPad> pad, uint32_t button, uint32_t timeMs, uint32_t state);
    void strip(SP<CTabletPad> pad, uint32_t strip, double position, bool finger, uint32_t timeMs);
    void ring(SP<CTabletPad> pad, uint32_t ring, double position, bool finger, uint32_t timeMs);

  private:
    void onManagerResourceDestroy(wl_resource* res);
    void destroyResource(CTabletSeat* resource);
    void destroyResource(CTabletToolV2Resource* resource);
    void destroyResource(CTabletV2Resource* resource);
    void destroyResource(CTabletPadV2Resource* resource);
    void destroyResource(CTabletPadGroupV2Resource* resource);
    void destroyResource(CTabletPadRingV2Resource* resource);
    void destroyResource(CTabletPadStripV2Resource* resource);
    void onGetSeat(CZwpTabletManagerV2* pMgr, uint32_t id, wl_resource* seat);

    //
    std::vector<UP<CZwpTabletManagerV2>>       m_managers;
    std::vector<SP<CTabletSeat>>               m_seats;
    std::vector<SP<CTabletToolV2Resource>>     m_tools;
    std::vector<SP<CTabletV2Resource>>         m_tablets;
    std::vector<SP<CTabletPadV2Resource>>      m_pads;
    std::vector<SP<CTabletPadGroupV2Resource>> m_groups;
    std::vector<SP<CTabletPadRingV2Resource>>  m_rings;
    std::vector<SP<CTabletPadStripV2Resource>> m_strips;

    // registered
    std::vector<WP<CTablet>>     m_tabletDevices;
    std::vector<WP<CTabletTool>> m_toolDevices;
    std::vector<WP<CTabletPad>>  m_padDevices;

    // FIXME: rings and strips are broken, I don't understand how this shit works.
    // It's 2am.
    SP<CTabletPadRingV2Resource>  ringForID(SP<CTabletPad> pad, uint32_t id);
    SP<CTabletPadStripV2Resource> stripForID(SP<CTabletPad> pad, uint32_t id);

    friend class CTabletSeat;
    friend class CTabletToolV2Resource;
    friend class CTabletV2Resource;
    friend class CTabletPadV2Resource;
    friend class CTabletPadGroupV2Resource;
    friend class CTabletPadRingV2Resource;
    friend class CTabletPadStripV2Resource;
};

namespace PROTO {
    inline UP<CTabletV2Protocol> tablet;
};
