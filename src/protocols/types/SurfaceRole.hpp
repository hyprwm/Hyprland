#pragma once

enum eSurfaceRole : uint8_t {
    SURFACE_ROLE_UNASSIGNED = 0,
    SURFACE_ROLE_XDG_SHELL,
    SURFACE_ROLE_LAYER_SHELL,
    SURFACE_ROLE_EASTER_EGG,
    SURFACE_ROLE_SUBSURFACE,
    SURFACE_ROLE_CURSOR,
};

class CSurfaceRole {
  public:
    virtual eSurfaceRole role() = 0;
    virtual ~ISurfaceRole()     = default;
};
