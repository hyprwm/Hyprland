#pragma once

#include <string>
#include "../defines.hpp"
#include "WLSurface.hpp"
#include "../helpers/AnimatedVariable.hpp"

struct SLayerRule {
    std::string targetNamespace = "";
    std::string rule            = "";
};

class CLayerShellResource;

class CLayerSurface {
  public:
    static PHLLS create(SP<CLayerShellResource>);

  private:
    CLayerSurface(SP<CLayerShellResource>);

  public:
    ~CLayerSurface();

    void                        applyRules();
    void                        startAnimation(bool in, bool instant = false);
    bool                        isFadedOut();
    int                         popupsCount();

    CAnimatedVariable<Vector2D> realPosition;
    CAnimatedVariable<Vector2D> realSize;
    CAnimatedVariable<float>    alpha;

    WP<CLayerShellResource>     layerSurface;
    wl_list                     link;

    // the header providing the enum type cannot be imported here
    int                        interactivity = 0;

    SP<CWLSurface>             surface;

    bool                       mapped = false;
    uint32_t                   layer  = 0;

    MONITORID                  monitorID = -1;

    bool                       fadingOut     = false;
    bool                       readyToDelete = false;
    bool                       noProcess     = false;
    bool                       noAnimations  = false;

    bool                       forceBlur        = false;
    bool                       forceBlurPopups  = false;
    int64_t                    xray             = -1;
    bool                       ignoreAlpha      = false;
    float                      ignoreAlphaValue = 0.f;
    bool                       dimAround        = false;
    int64_t                    order            = 0;

    std::optional<std::string> animationStyle;

    PHLLSREF                   self;

    CBox                       geometry = {0, 0, 0, 0};
    Vector2D                   position;
    std::string                szNamespace = "";
    std::unique_ptr<CPopup>    popupHead;

    void                       onDestroy();
    void                       onMap();
    void                       onUnmap();
    void                       onCommit();

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
        return layerSurface == rhs.layerSurface && monitorID == rhs.monitorID;
    }
};
