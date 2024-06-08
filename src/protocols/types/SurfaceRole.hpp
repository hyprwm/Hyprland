#pragma once

enum eSurfaceRole {
    SURFACE_ROLE_UNASSIGNED = 0,
    SURFACE_ROLE_XDG_SHELL,
    SURFACE_ROLE_LAYER_SHELL,
    SURFACE_ROLE_EASTER_EGG,
    SURFACE_ROLE_SUBSURFACE,
};

class ISurfaceRole {
  public:
    virtual eSurfaceRole role() = 0;
};
