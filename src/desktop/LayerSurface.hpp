#pragma once

#include <string>
#include "../defines.hpp"
#include "WLSurface.hpp"
#include "../helpers/AnimatedVariable.hpp"

class CLayerShellResource;

class CLayerSurface {
  public:
    static PHLLS create(SP<CLayerShellResource>);

  private:
    CLayerSurface(SP<CLayerShellResource>);

  public:
    ~CLayerSurface();

    void                    applyRules();
    void                    startAnimation(bool in, bool instant = false);
    bool                    isFadedOut();
    int                     popupsCount();

    PHLANIMVAR<Vector2D>    realPosition;
    PHLANIMVAR<Vector2D>    realSize;
    PHLANIMVAR<float>       alpha;

    WP<CLayerShellResource> layerSurface;
    wl_list                 link;

    // the header providing the enum type cannot be imported here
    int                        interactivity = 0;

    SP<CWLSurface>             surface;

    bool                       mapped = false;
    uint32_t                   layer  = 0;

    PHLMONITORREF              monitor;

    bool                       fadingOut     = false;
    bool                       readyToDelete = false;
    bool                       noProcess     = false;
    bool                       noAnimations  = false;

    bool                       forceBlur                   = false;
    bool                       forceBlurPopups             = false;
    int64_t                    xray                        = -1;
    bool                       ignoreAlpha                 = false;
    float                      ignoreAlphaValue            = 0.f;
    bool                       dimAround                   = false;
    int64_t                    order                       = 0;
    bool                       aboveLockscreen             = false;
    int64_t                    aboveLockscreenInteractable = false;

    std::optional<std::string> animationStyle;

    PHLLSREF                   self;

    CBox                       geometry = {0, 0, 0, 0};
    Vector2D                   position;
    std::string                szNamespace = "";
    UP<CPopup>                 popupHead;

    pid_t                      getPID();

    void                       onDestroy();
    void                       onMap();
    void                       onUnmap();
    void                       onCommit();
    MONITORID                  monitorID();

  private:
    struct {
        CHyprSignalListener destroy;
        CHyprSignalListener map;
        CHyprSignalListener unmap;
        CHyprSignalListener commit;
    } listeners;

    void registerCallbacks();

    // For the list lookup
    bool operator==(const CLayerSurface& rhs) const {
        return layerSurface == rhs.layerSurface && monitor == rhs.monitor;
    }
};
