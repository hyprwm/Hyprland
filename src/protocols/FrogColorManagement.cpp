#include "FrogColorManagement.hpp"
#include "color-management-v1.hpp"
#include "macros.hpp"
#include "protocols/ColorManagement.hpp"
#include "protocols/core/Subcompositor.hpp"
#include "protocols/types/ColorManagement.hpp"

using namespace NColorManagement;

static wpColorManagerV1TransferFunction getWPTransferFunction(frogColorManagedSurfaceTransferFunction tf) {
    switch (tf) {
        case FROG_COLOR_MANAGED_SURFACE_TRANSFER_FUNCTION_UNDEFINED: return WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_BT1886;
        case FROG_COLOR_MANAGED_SURFACE_TRANSFER_FUNCTION_SRGB: return WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_SRGB;
        case FROG_COLOR_MANAGED_SURFACE_TRANSFER_FUNCTION_GAMMA_22: return WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_GAMMA22;
        case FROG_COLOR_MANAGED_SURFACE_TRANSFER_FUNCTION_ST2084_PQ: return WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_ST2084_PQ;
        case FROG_COLOR_MANAGED_SURFACE_TRANSFER_FUNCTION_SCRGB_LINEAR: return WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_EXT_LINEAR;
        default: UNREACHABLE();
    }
}

static wpColorManagerV1Primaries getWPPrimaries(frogColorManagedSurfacePrimaries primaries) {
    return static_cast<wpColorManagerV1Primaries>(primaries + 1);
}

CFrogColorManager::CFrogColorManager(SP<CFrogColorManagementFactoryV1> resource_) : m_resource(resource_) {
    if UNLIKELY (!good())
        return;

    m_resource->setDestroy([](CFrogColorManagementFactoryV1* r) { LOGM(TRACE, "Destroy frog_color_management at {:x} (generated default)", (uintptr_t)r); });
    m_resource->setOnDestroy([this](CFrogColorManagementFactoryV1* r) { PROTO::frogColorManagement->destroyResource(this); });

    m_resource->setGetColorManagedSurface([](CFrogColorManagementFactoryV1* r, wl_resource* surface, uint32_t id) {
        LOGM(TRACE, "Get surface for id={}, surface={}", id, (uintptr_t)surface);
        auto SURF = CWLSurfaceResource::fromResource(surface);

        if (!SURF) {
            LOGM(ERR, "No surface for resource {}", (uintptr_t)surface);
            r->error(-1, "Invalid surface (2)");
            return;
        }

        const auto RESOURCE =
            PROTO::frogColorManagement->m_surfaces.emplace_back(makeShared<CFrogColorManagementSurface>(makeShared<CFrogColorManagedSurface>(r->client(), r->version(), id), SURF));
        if UNLIKELY (!RESOURCE->good()) {
            r->noMemory();
            PROTO::frogColorManagement->m_surfaces.pop_back();
            return;
        }

        RESOURCE->m_self = RESOURCE;
    });
}

bool CFrogColorManager::good() {
    return m_resource->resource();
}

CFrogColorManagementSurface::CFrogColorManagementSurface(SP<CFrogColorManagedSurface> resource_, SP<CWLSurfaceResource> surface_) : m_surface(surface_), m_resource(resource_) {
    if UNLIKELY (!good())
        return;

    m_client = m_resource->client();

    if (!m_surface->m_colorManagement.valid()) {
        const auto RESOURCE = PROTO::colorManagement->m_surfaces.emplace_back(makeShared<CColorManagementSurface>(surface_));
        if UNLIKELY (!RESOURCE) {
            m_resource->noMemory();
            PROTO::colorManagement->m_surfaces.pop_back();
            return;
        }

        RESOURCE->m_self = RESOURCE;

        m_surface->m_colorManagement = RESOURCE;

        m_resource->setOnDestroy([this](CFrogColorManagedSurface* r) {
            LOGM(TRACE, "Destroy frog cm and xx cm for surface {}", (uintptr_t)m_surface);
            if (m_surface.valid())
                PROTO::colorManagement->destroyResource(m_surface->m_colorManagement.get());
            PROTO::frogColorManagement->destroyResource(this);
        });
    } else
        m_resource->setOnDestroy([this](CFrogColorManagedSurface* r) {
            LOGM(TRACE, "Destroy frog cm surface {}", (uintptr_t)m_surface);
            PROTO::frogColorManagement->destroyResource(this);
        });

    m_resource->setDestroy([this](CFrogColorManagedSurface* r) {
        LOGM(TRACE, "Destroy frog cm surface {}", (uintptr_t)m_surface);
        PROTO::frogColorManagement->destroyResource(this);
    });

    m_resource->setSetKnownTransferFunction([this](CFrogColorManagedSurface* r, frogColorManagedSurfaceTransferFunction tf) {
        LOGM(TRACE, "Set frog cm transfer function {} for {}", (uint32_t)tf, m_surface->id());
        switch (tf) {
            case FROG_COLOR_MANAGED_SURFACE_TRANSFER_FUNCTION_ST2084_PQ:
                m_surface->m_colorManagement->m_imageDescription.transferFunction =
                    convertTransferFunction(getWPTransferFunction(FROG_COLOR_MANAGED_SURFACE_TRANSFER_FUNCTION_ST2084_PQ));
                break;
                ;
            case FROG_COLOR_MANAGED_SURFACE_TRANSFER_FUNCTION_GAMMA_22:
                if (m_pqIntentSent) {
                    LOGM(TRACE,
                         "FIXME: assuming broken enum value 2 (FROG_COLOR_MANAGED_SURFACE_TRANSFER_FUNCTION_GAMMA_22) referring to eotf value 2 (TRANSFER_FUNCTION_ST2084_PQ)");
                    m_surface->m_colorManagement->m_imageDescription.transferFunction =
                        convertTransferFunction(getWPTransferFunction(FROG_COLOR_MANAGED_SURFACE_TRANSFER_FUNCTION_ST2084_PQ));
                    break;
                };
                [[fallthrough]];
            case FROG_COLOR_MANAGED_SURFACE_TRANSFER_FUNCTION_UNDEFINED:
            case FROG_COLOR_MANAGED_SURFACE_TRANSFER_FUNCTION_SCRGB_LINEAR: LOGM(TRACE, "FIXME: add tf support for {}", (uint32_t)tf); [[fallthrough]];
            case FROG_COLOR_MANAGED_SURFACE_TRANSFER_FUNCTION_SRGB:
                m_surface->m_colorManagement->m_imageDescription.transferFunction = convertTransferFunction(getWPTransferFunction(tf));

                m_surface->m_colorManagement->setHasImageDescription(true);
        }
    });
    m_resource->setSetKnownContainerColorVolume([this](CFrogColorManagedSurface* r, frogColorManagedSurfacePrimaries primariesName) {
        LOGM(TRACE, "Set frog cm primaries {}", (uint32_t)primariesName);
        switch (primariesName) {
            case FROG_COLOR_MANAGED_SURFACE_PRIMARIES_UNDEFINED:
            case FROG_COLOR_MANAGED_SURFACE_PRIMARIES_REC709: m_surface->m_colorManagement->m_imageDescription.primaries = NColorPrimaries::BT709; break;
            case FROG_COLOR_MANAGED_SURFACE_PRIMARIES_REC2020: m_surface->m_colorManagement->m_imageDescription.primaries = NColorPrimaries::BT2020; break;
        }
        m_surface->m_colorManagement->m_imageDescription.primariesNamed = convertPrimaries(getWPPrimaries(primariesName));

        m_surface->m_colorManagement->setHasImageDescription(true);
    });
    m_resource->setSetRenderIntent([this](CFrogColorManagedSurface* r, frogColorManagedSurfaceRenderIntent intent) {
        LOGM(TRACE, "Set frog cm intent {}", (uint32_t)intent);
        m_pqIntentSent = intent == FROG_COLOR_MANAGED_SURFACE_RENDER_INTENT_PERCEPTUAL;
        m_surface->m_colorManagement->setHasImageDescription(true);
    });
    m_resource->setSetHdrMetadata([this](CFrogColorManagedSurface* r, uint32_t r_x, uint32_t r_y, uint32_t g_x, uint32_t g_y, uint32_t b_x, uint32_t b_y, uint32_t w_x,
                                         uint32_t w_y, uint32_t max_lum, uint32_t min_lum, uint32_t cll, uint32_t fall) {
        LOGM(TRACE, "Set frog primaries r:{},{} g:{},{} b:{},{} w:{},{} luminances {} - {} cll {} fall {}", r_x, r_y, g_x, g_y, b_x, b_y, w_x, w_y, min_lum, max_lum, cll, fall);
        m_surface->m_colorManagement->m_imageDescription.masteringPrimaries      = SPCPRimaries{.red   = {.x = r_x / 50000.0f, .y = r_y / 50000.0f},
                                                                                                .green = {.x = g_x / 50000.0f, .y = g_y / 50000.0f},
                                                                                                .blue  = {.x = b_x / 50000.0f, .y = b_y / 50000.0f},
                                                                                                .white = {.x = w_x / 50000.0f, .y = w_y / 50000.0f}};
        m_surface->m_colorManagement->m_imageDescription.masteringLuminances.min = min_lum / 10000.0f;
        m_surface->m_colorManagement->m_imageDescription.masteringLuminances.max = max_lum;
        m_surface->m_colorManagement->m_imageDescription.maxCLL                  = cll;
        m_surface->m_colorManagement->m_imageDescription.maxFALL                 = fall;

        m_surface->m_colorManagement->setHasImageDescription(true);
    });
}

bool CFrogColorManagementSurface::good() {
    return m_resource->resource();
}

wl_client* CFrogColorManagementSurface::client() {
    return m_client;
}

CFrogColorManagementProtocol::CFrogColorManagementProtocol(const wl_interface* iface, const int& ver, const std::string& name) : IWaylandProtocol(iface, ver, name) {
    ;
}

void CFrogColorManagementProtocol::bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id) {
    const auto RESOURCE = m_managers.emplace_back(makeShared<CFrogColorManager>(makeShared<CFrogColorManagementFactoryV1>(client, ver, id)));

    if UNLIKELY (!RESOURCE->good()) {
        wl_client_post_no_memory(client);
        m_managers.pop_back();
        return;
    }

    LOGM(TRACE, "New frog_color_management at {:x}", (uintptr_t)RESOURCE.get());
}

void CFrogColorManagementProtocol::destroyResource(CFrogColorManager* resource) {
    std::erase_if(m_managers, [&](const auto& other) { return other.get() == resource; });
}

void CFrogColorManagementProtocol::destroyResource(CFrogColorManagementSurface* resource) {
    std::erase_if(m_surfaces, [&](const auto& other) { return other.get() == resource; });
}
