#include "FrogColorManagement.hpp"

CFrogColorManager::CFrogColorManager(SP<CFrogColorManagementFactoryV1> resource_) : resource(resource_) {
    if (!good())
        return;

    resource->setDestroy([](CFrogColorManagementFactoryV1* r) { LOGM(TRACE, "Destroy frog_color_management at {:x} (generated default)", (uintptr_t)r); });
    resource->setOnDestroy([this](CFrogColorManagementFactoryV1* r) { PROTO::frogColorManagement->destroyResource(this); });

    resource->setGetColorManagedSurface([](CFrogColorManagementFactoryV1* r, wl_resource* surface, uint32_t id) {
        LOGM(TRACE, "Get surface for id={}, surface={}", id, (uintptr_t)surface);
        auto SURF = CWLSurfaceResource::fromResource(surface);

        if (!SURF) {
            LOGM(ERR, "No surface for resource {}", (uintptr_t)surface);
            r->error(-1, "Invalid surface (2)");
            return;
        }

        const auto RESOURCE = PROTO::frogColorManagement->m_vSurfaces.emplace_back(
            makeShared<CFrogColorManagementSurface>(makeShared<CFrogColorManagedSurface>(r->client(), r->version(), id), SURF));
        if (!RESOURCE->good()) {
            r->noMemory();
            PROTO::frogColorManagement->m_vSurfaces.pop_back();
            return;
        }

        RESOURCE->self = RESOURCE;

        SURF->frogColorManagement = RESOURCE;
    });
}

bool CFrogColorManager::good() {
    return resource->resource();
}

static const auto BT709 = SImageDescription::SPCPRimaries{.red   = {.x = 0.64 * 50000, .y = 0.33 * 50000},
                                                          .green = {.x = 0.30 * 50000, .y = 0.60 * 50000},
                                                          .blue  = {.x = 0.15 * 50000, .y = 0.06 * 50000},
                                                          .white = {.x = 0.3127 * 50000, .y = 0.3290 * 50000}};

static const auto BT2020 = SImageDescription::SPCPRimaries{.red   = {.x = 0.708 * 50000, .y = 0.292 * 50000},
                                                           .green = {.x = 0.170 * 50000, .y = 0.797 * 50000},
                                                           .blue  = {.x = 0.131 * 50000, .y = 0.046 * 50000},
                                                           .white = {.x = 0.3127 * 50000, .y = 0.3290 * 50000}};

CFrogColorManagementSurface::CFrogColorManagementSurface(SP<CFrogColorManagedSurface> resource_, SP<CWLSurfaceResource> surface_) : surface(surface_), resource(resource_) {
    if (!good())
        return;

    pClient = resource->client();

    resource->setDestroy([this](CFrogColorManagedSurface* r) {
        LOGM(TRACE, "Destroy frog cm surface {}", (uintptr_t)surface);
        PROTO::frogColorManagement->destroyResource(this);
    });
    resource->setOnDestroy([this](CFrogColorManagedSurface* r) {
        LOGM(TRACE, "Destroy frog cm surface {}", (uintptr_t)surface);
        PROTO::frogColorManagement->destroyResource(this);
    });

    resource->setSetKnownTransferFunction([this](CFrogColorManagedSurface* r, frogColorManagedSurfaceTransferFunction tf) {
        LOGM(TRACE, "Set frog cm transfer function {}", (uint32_t)tf);
        switch (tf) {
            case FROG_COLOR_MANAGED_SURFACE_TRANSFER_FUNCTION_ST2084_PQ:
                this->settings.transferFunction = XX_COLOR_MANAGER_V4_TRANSFER_FUNCTION_ST2084_PQ;
                break;
                ;
            case FROG_COLOR_MANAGED_SURFACE_TRANSFER_FUNCTION_GAMMA_22:
                if (this->pqIntentSent) {
                    LOGM(TRACE,
                         "FIXME: assuming broken enum value 2 (FROG_COLOR_MANAGED_SURFACE_TRANSFER_FUNCTION_GAMMA_22) referring to eotf value 2 (TRANSFER_FUNCTION_ST2084_PQ)");
                    this->settings.transferFunction = XX_COLOR_MANAGER_V4_TRANSFER_FUNCTION_ST2084_PQ;
                    break;
                };
            case FROG_COLOR_MANAGED_SURFACE_TRANSFER_FUNCTION_UNDEFINED:
            case FROG_COLOR_MANAGED_SURFACE_TRANSFER_FUNCTION_SCRGB_LINEAR: LOGM(TRACE, std::format("FIXME: add tf support for {}", (uint32_t)tf));
            case FROG_COLOR_MANAGED_SURFACE_TRANSFER_FUNCTION_SRGB: this->settings.transferFunction = XX_COLOR_MANAGER_V4_TRANSFER_FUNCTION_SRGB;
        }
    });
    resource->setSetKnownContainerColorVolume([this](CFrogColorManagedSurface* r, frogColorManagedSurfacePrimaries primariesName) {
        LOGM(TRACE, "Set frog cm primaries {}", (uint32_t)primariesName);
        switch (primariesName) {
            case FROG_COLOR_MANAGED_SURFACE_PRIMARIES_UNDEFINED:
            case FROG_COLOR_MANAGED_SURFACE_PRIMARIES_REC709: this->settings.masteringPrimaries = BT709; break;
            case FROG_COLOR_MANAGED_SURFACE_PRIMARIES_REC2020: this->settings.masteringPrimaries = BT2020; break;
        }
    });
    resource->setSetRenderIntent([this](CFrogColorManagedSurface* r, frogColorManagedSurfaceRenderIntent intent) {
        LOGM(TRACE, "Set frog cm intent {}", (uint32_t)intent);
        this->pqIntentSent = intent == FROG_COLOR_MANAGED_SURFACE_RENDER_INTENT_PERCEPTUAL;
    });
    resource->setSetHdrMetadata([this](CFrogColorManagedSurface* r, uint32_t r_x, uint32_t r_y, uint32_t g_x, uint32_t g_y, uint32_t b_x, uint32_t b_y, uint32_t w_x, uint32_t w_y,
                                       uint32_t max_lum, uint32_t min_lum, uint32_t cll, uint32_t fall) {
        LOGM(TRACE, "Set frog mastering primaries r:{},{} g:{},{} b:{},{} w:{},{} luminances {} - {} cll {} fall {}", r_x, r_y, g_x, g_y, b_x, b_y, w_x, w_y, min_lum, max_lum, cll,
             fall);
        this->settings.masteringPrimaries =
            SImageDescription::SPCPRimaries{.red = {.x = r_x, .y = r_y}, .green = {.x = g_x, .y = g_y}, .blue = {.x = b_x, .y = b_y}, .white = {.x = w_x, .y = w_y}};
        this->settings.masteringLuminances.min = min_lum;
        this->settings.masteringLuminances.max = max_lum;
        this->settings.maxCLL                  = cll;
        this->settings.maxFALL                 = fall;
        this->settings.drmCoded                = true;
    });
}

bool CFrogColorManagementSurface::good() {
    return resource->resource();
}

wl_client* CFrogColorManagementSurface::client() {
    return pClient;
}

CFrogColorManagementProtocol::CFrogColorManagementProtocol(const wl_interface* iface, const int& ver, const std::string& name) : IWaylandProtocol(iface, ver, name) {
    ;
}

void CFrogColorManagementProtocol::bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id) {
    const auto RESOURCE = m_vManagers.emplace_back(makeShared<CFrogColorManager>(makeShared<CFrogColorManagementFactoryV1>(client, ver, id)));

    if (!RESOURCE->good()) {
        wl_client_post_no_memory(client);
        m_vManagers.pop_back();
        return;
    }

    LOGM(TRACE, "New frog_color_management at {:x}", (uintptr_t)RESOURCE.get());
}

void CFrogColorManagementProtocol::destroyResource(CFrogColorManager* resource) {
    std::erase_if(m_vManagers, [&](const auto& other) { return other.get() == resource; });
}

void CFrogColorManagementProtocol::destroyResource(CFrogColorManagementSurface* resource) {
    std::erase_if(m_vSurfaces, [&](const auto& other) { return other.get() == resource; });
}
