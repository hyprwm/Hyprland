#pragma once

#include "../helpers/signal/Signal.hpp"
#include "../helpers/memory/Memory.hpp"
#include "../helpers/WLListener.hpp"
#include "../macros.hpp"

#include "XDataSource.hpp"

#include <xcb/xcb.h>
#include <xcb/xcb_errors.h>
#include <xcb/composite.h>
#include <xcb/xfixes.h>
#include <xcb/res.h>

struct wl_event_source;
class CXWaylandSurfaceResource;
struct SXSelection;

struct SXTransfer {
    ~SXTransfer();

    SXSelection&                  selection;
    bool                          out = true;

    bool                          incremental   = false;
    bool                          flushOnDelete = false;
    bool                          propertySet   = false;

    int                           wlFD        = -1;
    wl_event_source*              eventSource = nullptr;

    std::vector<uint8_t>          data;

    xcb_selection_request_event_t request;

    int                           propertyStart;
    xcb_get_property_reply_t*     propertyReply;
    xcb_window_t                  incomingWindow;

    bool                          getIncomingSelectionProp(bool erase);
};

struct SXSelection {
    xcb_window_t     window    = 0;
    xcb_window_t     owner     = 0;
    xcb_timestamp_t  timestamp = 0;
    SP<CXDataSource> dataSource;

    void             onSelection();
    bool             sendData(xcb_selection_request_event_t* e, std::string mime);
    int              onRead(int fd, uint32_t mask);

    struct {
        CHyprSignalListener setSelection;
    } listeners;

    std::unique_ptr<SXTransfer> transfer;
};

class CXWM {
  public:
    CXWM();
    ~CXWM();

    int onEvent(int fd, uint32_t mask);

  private:
    void                 setCursor(unsigned char* pixData, uint32_t stride, const Vector2D& size, const Vector2D& hotspot);

    void                 gatherResources();
    void                 getVisual();
    void                 getRenderFormat();
    void                 createWMWindow();
    void                 initSelection();

    void                 onNewSurface(SP<CWLSurfaceResource> surf);
    void                 onNewResource(SP<CXWaylandSurfaceResource> resource);

    void                 setActiveWindow(xcb_window_t window);
    void                 sendState(SP<CXWaylandSurface> surf);
    void                 focusWindow(SP<CXWaylandSurface> surf);
    void                 activateSurface(SP<CXWaylandSurface> surf, bool activate);
    bool                 isWMWindow(xcb_window_t w);

    void                 sendWMMessage(SP<CXWaylandSurface> surf, xcb_client_message_data_t* data, uint32_t mask);

    SP<CXWaylandSurface> windowForXID(xcb_window_t wid);

    void                 readWindowData(SP<CXWaylandSurface> surf);
    void                 associate(SP<CXWaylandSurface> surf, SP<CWLSurfaceResource> wlSurf);
    void                 dissociate(SP<CXWaylandSurface> surf);

    void                 updateClientList();

    // event handlers
    void        handleCreate(xcb_create_notify_event_t* e);
    void        handleDestroy(xcb_destroy_notify_event_t* e);
    void        handleConfigure(xcb_configure_request_event_t* e);
    void        handleConfigureNotify(xcb_configure_notify_event_t* e);
    void        handleMapRequest(xcb_map_request_event_t* e);
    void        handleMapNotify(xcb_map_notify_event_t* e);
    void        handleUnmapNotify(xcb_unmap_notify_event_t* e);
    void        handlePropertyNotify(xcb_property_notify_event_t* e);
    void        handleClientMessage(xcb_client_message_event_t* e);
    void        handleFocusIn(xcb_focus_in_event_t* e);
    void        handleError(xcb_value_error_t* e);

    bool        handleSelectionEvent(xcb_generic_event_t* e);
    void        handleSelectionNotify(xcb_selection_notify_event_t* e);
    bool        handleSelectionPropertyNotify(xcb_property_notify_event_t* e);
    void        handleSelectionRequest(xcb_selection_request_event_t* e);
    bool        handleSelectionXFixesNotify(xcb_xfixes_selection_notify_event_t* e);

    void        selectionSendNotify(xcb_selection_request_event_t* e, bool success);
    xcb_atom_t  mimeToAtom(const std::string& mime);
    std::string mimeFromAtom(xcb_atom_t atom);
    void        setClipboardToWayland(SXSelection& sel);
    void        getTransferData(SXSelection& sel);
    void        readProp(SP<CXWaylandSurface> XSURF, uint32_t atom, xcb_get_property_reply_t* reply);

    //
    xcb_connection_t*                         connection = nullptr;
    xcb_errors_context_t*                     errors     = nullptr;
    xcb_screen_t*                             screen     = nullptr;

    xcb_window_t                              wmWindow;

    wl_event_source*                          eventSource = nullptr;

    const xcb_query_extension_reply_t*        xfixes      = nullptr;
    const xcb_query_extension_reply_t*        xres        = nullptr;
    int                                       xfixesMajor = 0;

    xcb_visualid_t                            visual_id;
    xcb_colormap_t                            colormap;
    uint32_t                                  cursorXID = 0;

    xcb_render_pictformat_t                   render_format_id;

    std::vector<WP<CXWaylandSurfaceResource>> shellResources;
    std::vector<SP<CXWaylandSurface>>         surfaces;
    std::vector<WP<CXWaylandSurface>>         mappedSurfaces;         // ordered by map time
    std::vector<WP<CXWaylandSurface>>         mappedSurfacesStacking; // ordered by stacking

    WP<CXWaylandSurface>                      focusedSurface;
    uint64_t                                  lastFocusSeq = 0;

    SXSelection                               clipboard;

    struct {
        CHyprSignalListener newWLSurface;
        CHyprSignalListener newXShellSurface;
    } listeners;

    friend class CXWaylandSurface;
    friend class CXWayland;
    friend class CXDataSource;
    friend struct SXSelection;
    friend struct SXTransfer;
};
