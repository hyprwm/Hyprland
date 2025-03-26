#pragma once

#include <vector>
#include "WaylandProtocol.hpp"
#include "hyprland-toplevel-mapping-v1.hpp"

class CToplevelWindowMappingHandle {
    public:
        CToplevelWindowMappingHandle(SP<CHyprlandToplevelWindowMappingHandleV1> resource_);

        bool good();

    private:
        SP<CHyprlandToplevelWindowMappingHandleV1> resource;

    friend class CToplevelMappingManager;
};

class CWindowToplevelMappingHandle {
    public:
        CWindowToplevelMappingHandle(SP<CHyprlandWindowToplevelMappingHandleV1> resource_);

        bool good();

    private:
        SP<CHyprlandWindowToplevelMappingHandleV1> resource;

    friend class CToplevelMappingManager;
};

class CWindowWlrToplevelMappingHandle {
    public:
        CWindowWlrToplevelMappingHandle(SP<CHyprlandWindowWlrToplevelMappingHandleV1> resource_);

        bool good();

    private:
        SP<CHyprlandWindowWlrToplevelMappingHandleV1> resource;

    friend class CToplevelMappingManager;
};

class CToplevelMappingManager {
    public:
        CToplevelMappingManager(SP<CHyprlandToplevelMappingManagerV1> resource_);

        bool good();
    private:
        SP<CHyprlandToplevelMappingManagerV1>   resource;

        std::vector<WP<CToplevelWindowMappingHandle>> toplevelWindowHandles;
        std::vector<WP<CWindowToplevelMappingHandle>> windowToplevelHandles;
        std::vector<WP<CWindowWlrToplevelMappingHandle>> windowWlrToplevelHandles;
};

class CToplevelMappingProtocol : IWaylandProtocol {
    public:
        CToplevelMappingProtocol(const wl_interface* iface, const int& ver, const std::string& name);

        void bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id);
        void destroyHandle(CToplevelWindowMappingHandle* handle);
        void destroyHandle(CWindowToplevelMappingHandle* handle);
        void destroyHandle(CWindowWlrToplevelMappingHandle* handle);

    private:
        void onManagerResourceDestroy(CToplevelMappingManager* mgr);

        std::vector<UP<CToplevelMappingManager>> m_vManagers;
        std::vector<SP<CToplevelWindowMappingHandle>>  m_vToplevelWindowHandles;
        std::vector<SP<CWindowToplevelMappingHandle>>  m_vWindowToplevelHandles;
        std::vector<SP<CWindowWlrToplevelMappingHandle>>  m_vWindowWlrToplevelHandles;

    friend class CToplevelMappingManager;
    friend class CToplevelWindowMappingHandle;
    friend class CWindowToplevelMappingHandle;
    friend class CWindowWlrToplevelMappingHandle;
};

namespace PROTO {
    inline UP<CToplevelMappingProtocol> toplevelMapping;
};