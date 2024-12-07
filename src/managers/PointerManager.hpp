#pragma once

#include "../devices/IPointer.hpp"
#include "../devices/ITouch.hpp"
#include "../devices/Tablet.hpp"
#include "../helpers/math/Math.hpp"
#include "../desktop/WLSurface.hpp"
#include "../helpers/sync/SyncTimeline.hpp"
#include <tuple>

class CMonitor;
class IHID;
class CTexture;

AQUAMARINE_FORWARD(IBuffer);

/*
    The naming here is a bit confusing.
    CPointerManager manages the _position_ and _displaying_ of the cursor,
    but the CCursorManager _only_ manages the actual image (texture) and size
    of the cursor.
*/

class CPointerManager {
  public:
    CPointerManager();

    void attachPointer(SP<IPointer> pointer);
    void attachTouch(SP<ITouch> touch);
    void attachTablet(SP<CTablet> tablet);

    void detachPointer(SP<IPointer> pointer);
    void detachTouch(SP<ITouch> touch);
    void detachTablet(SP<CTablet> tablet);

    // only clamps to the layout.
    void warpTo(const Vector2D& logical);
    void move(const Vector2D& deltaLogical);
    void warpAbsolute(Vector2D abs, SP<IHID> dev);

    void setCursorBuffer(SP<Aquamarine::IBuffer> buf, const Vector2D& hotspot, const float& scale);
    void setCursorSurface(SP<CWLSurface> buf, const Vector2D& hotspot);
    void resetCursorImage(bool apply = true);

    void lockSoftwareForMonitor(PHLMONITOR pMonitor);
    void unlockSoftwareForMonitor(PHLMONITOR pMonitor);
    void lockSoftwareAll();
    void unlockSoftwareAll();
    bool softwareLockedFor(PHLMONITOR pMonitor);

    void renderSoftwareCursorsFor(PHLMONITOR pMonitor, timespec* now, CRegion& damage /* logical */, std::optional<Vector2D> overridePos = {} /* monitor-local */);

    // this is needed e.g. during screensharing where
    // the software cursors aren't locked during the cursor move, but they
    // are rendered later.
    void damageCursor(PHLMONITOR pMonitor);

    //
    Vector2D position();
    Vector2D cursorSizeLogical();
    void     storeMovement(uint64_t time, const Vector2D& delta, const Vector2D& deltaUnaccel);
    void     setStoredMovement(uint64_t time, const Vector2D& delta, const Vector2D& deltaUnaccel);
    void     sendStoredMovement();

    void     recheckEnteredOutputs();

  private:
    void recheckPointerPosition();
    void onMonitorLayoutChange();
    void onMonitorDisconnect();
    void updateCursorBackend();
    void onCursorMoved();
    bool hasCursor();
    void damageIfSoftware();

    // closest valid point to a given one
    Vector2D closestValid(const Vector2D& pos);

    // returns the thing in device coordinates. Is NOT offset by the hotspot, relies on set_cursor with hotspot.
    Vector2D getCursorPosForMonitor(PHLMONITOR pMonitor);
    // returns the thing in logical coordinates of the monitor
    CBox getCursorBoxLogicalForMonitor(PHLMONITOR pMonitor);
    // returns the thing in global coords
    CBox         getCursorBoxGlobal();

    Vector2D     transformedHotspot(PHLMONITOR pMonitor);

    SP<CTexture> getCurrentCursorTexture();

    struct SPointerListener {
        CHyprSignalListener destroy;
        CHyprSignalListener motion;
        CHyprSignalListener motionAbsolute;
        CHyprSignalListener button;
        CHyprSignalListener axis;
        CHyprSignalListener frame;

        CHyprSignalListener swipeBegin;
        CHyprSignalListener swipeEnd;
        CHyprSignalListener swipeUpdate;

        CHyprSignalListener pinchBegin;
        CHyprSignalListener pinchEnd;
        CHyprSignalListener pinchUpdate;

        CHyprSignalListener holdBegin;
        CHyprSignalListener holdEnd;

        WP<IPointer>        pointer;
    };
    std::vector<SP<SPointerListener>> pointerListeners;

    struct STouchListener {
        CHyprSignalListener destroy;
        CHyprSignalListener down;
        CHyprSignalListener up;
        CHyprSignalListener motion;
        CHyprSignalListener cancel;
        CHyprSignalListener frame;

        WP<ITouch>          touch;
    };
    std::vector<SP<STouchListener>> touchListeners;

    struct STabletListener {
        CHyprSignalListener destroy;
        CHyprSignalListener axis;
        CHyprSignalListener proximity;
        CHyprSignalListener tip;
        CHyprSignalListener button;

        WP<CTablet>         tablet;
    };
    std::vector<SP<STabletListener>> tabletListeners;

    struct {
        std::vector<CBox> monitorBoxes;
    } currentMonitorLayout;

    struct {
        SP<Aquamarine::IBuffer> pBuffer;
        SP<CTexture>            bufferTex;
        WP<CWLSurface>          surface;

        Vector2D                hotspot;
        Vector2D                size;
        float                   scale = 1.F;

        CHyprSignalListener     destroySurface;
        CHyprSignalListener     commitSurface;
        SP<CSyncTimeline>       waitTimeline = nullptr;
        uint64_t                waitPoint    = 0;
    } currentCursorImage; // TODO: support various sizes per-output so we can have pixel-perfect cursors

    Vector2D pointerPos = {0, 0};

    uint64_t storedTime    = 0;
    Vector2D storedDelta   = {0, 0};
    Vector2D storedUnaccel = {0, 0};

    struct SMonitorPointerState {
        SMonitorPointerState(const PHLMONITOR& m) : monitor(m) {}
        ~SMonitorPointerState() = default;

        PHLMONITORREF           monitor;

        int                     softwareLocks  = 0;
        bool                    hardwareFailed = false;
        CBox                    box; // logical
        bool                    entered        = false;
        bool                    hwApplied      = false;
        bool                    cursorRendered = false;

        SP<Aquamarine::IBuffer> cursorFrontBuffer;
    };

    std::vector<SP<SMonitorPointerState>> monitorStates;
    SP<SMonitorPointerState>              stateFor(PHLMONITOR mon);
    bool                                  attemptHardwareCursor(SP<SMonitorPointerState> state);
    SP<Aquamarine::IBuffer>               renderHWCursorBuffer(SP<SMonitorPointerState> state, SP<CTexture> texture);
    bool                                  setHWCursorBuffer(SP<SMonitorPointerState> state, SP<Aquamarine::IBuffer> buf);

    struct {
        SP<HOOK_CALLBACK_FN> monitorAdded;
        SP<HOOK_CALLBACK_FN> monitorPreRender;
    } hooks;
};

inline UP<CPointerManager> g_pPointerManager;
