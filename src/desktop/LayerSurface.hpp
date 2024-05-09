#pragma once

#include <string>
#include "../defines.hpp"
#include "WLSurface.hpp"
#include "../helpers/AnimatedVariable.hpp"
#include "wlr-layer-shell-unstable-v1-protocol.h"

struct SLayerRule {
    std::string targetNamespace = "";
    std::string rule            = "";
};

class CLayerSurface {
  public:
    static PHLLS create(wlr_layer_surface_v1*);

  private:
    CLayerSurface();

  public:
    ~CLayerSurface();

    void                        applyRules();
    void                        startAnimation(bool in, bool instant = false);
    bool                        isFadedOut();
    int                         popupsCount();

    CAnimatedVariable<Vector2D> realPosition;
    CAnimatedVariable<Vector2D> realSize;
    CAnimatedVariable<float>    alpha;

    wlr_layer_surface_v1*       layerSurface;
    wl_list                     link;

    bool                        keyboardExclusive = false;

    CWLSurface                  surface;

    bool                        mapped = false;

    int                         monitorID = -1;

    bool                        fadingOut     = false;
    bool                        readyToDelete = false;
    bool                        noProcess     = false;
    bool                        noAnimations  = false;

    bool                        forceBlur        = false;
    bool                        forceBlurPopups  = false;
    int                         xray             = -1;
    bool                        ignoreAlpha      = false;
    float                       ignoreAlphaValue = 0.f;
    bool                        dimAround        = false;

    std::optional<std::string>  animationStyle;

    zwlr_layer_shell_v1_layer   layer;

    PHLLSREF                    self;

    CBox                        geometry = {0, 0, 0, 0};
    Vector2D                    position;
    std::string                 szNamespace = "";

    void                        onDestroy();
    void                        onMap();
    void                        onUnmap();
    void                        onCommit();

  private:
    std::unique_ptr<CPopup> popupHead;

    DYNLISTENER(destroyLayerSurface);
    DYNLISTENER(mapLayerSurface);
    DYNLISTENER(unmapLayerSurface);
    DYNLISTENER(commitLayerSurface);

    void registerCallbacks();

    // For the list lookup
    bool operator==(const CLayerSurface& rhs) const {
        return layerSurface == rhs.layerSurface && monitorID == rhs.monitorID;
    }
};