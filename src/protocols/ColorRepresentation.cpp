#include "ColorRepresentation.hpp"
#include "core/Compositor.hpp"
#include "../helpers/Format.hpp"

#include <algorithm>
#include <array>

using namespace NColorManagement;

// what EGL import hints can express
static constexpr std::array SUPPORTED_COEFFICIENTS = {
    WP_COLOR_REPRESENTATION_SURFACE_V1_COEFFICIENTS_BT601,
    WP_COLOR_REPRESENTATION_SURFACE_V1_COEFFICIENTS_BT709,
    WP_COLOR_REPRESENTATION_SURFACE_V1_COEFFICIENTS_BT2020,
};

static constexpr std::array SUPPORTED_RANGES = {
    WP_COLOR_REPRESENTATION_SURFACE_V1_RANGE_FULL,
    WP_COLOR_REPRESENTATION_SURFACE_V1_RANGE_LIMITED,
};

static_assert(sc<uint32_t>(CM_CHROMA_LOCATION_TYPE_0) == WP_COLOR_REPRESENTATION_SURFACE_V1_CHROMA_LOCATION_TYPE_0);
static_assert(sc<uint32_t>(CM_CHROMA_LOCATION_TYPE_5) == WP_COLOR_REPRESENTATION_SURFACE_V1_CHROMA_LOCATION_TYPE_5);

static eYCbCrCoefficients coefficientsFromWP(wpColorRepresentationSurfaceV1Coefficients coefficients) {
    switch (coefficients) {
        case WP_COLOR_REPRESENTATION_SURFACE_V1_COEFFICIENTS_BT601: return CM_YCBCR_COEFFICIENTS_BT601;
        case WP_COLOR_REPRESENTATION_SURFACE_V1_COEFFICIENTS_BT709: return CM_YCBCR_COEFFICIENTS_BT709;
        case WP_COLOR_REPRESENTATION_SURFACE_V1_COEFFICIENTS_BT2020: return CM_YCBCR_COEFFICIENTS_BT2020;
        default: return CM_YCBCR_COEFFICIENTS_UNSET;
    }
}

static eYCbCrRange rangeFromWP(wpColorRepresentationSurfaceV1Range range) {
    switch (range) {
        case WP_COLOR_REPRESENTATION_SURFACE_V1_RANGE_FULL: return CM_YCBCR_RANGE_FULL;
        case WP_COLOR_REPRESENTATION_SURFACE_V1_RANGE_LIMITED: return CM_YCBCR_RANGE_LIMITED;
        default: return CM_YCBCR_RANGE_UNSET;
    }
}

static bool isYUVBuffer(const CHLBufferReference& buffer) {
    return buffer->type() == Aquamarine::BUFFER_TYPE_DMABUF && NFormatUtils::isFormatYUV(buffer->dmabuf().format);
}

CColorRepresentationSurface::CColorRepresentationSurface(UP<CWpColorRepresentationSurfaceV1>&& resource, SP<CWLSurfaceResource> surface) :
    m_surface(surface), m_resource(std::move(resource)) {
    if UNLIKELY (!good())
        return;

    m_resource->setDestroy([this](CWpColorRepresentationSurfaceV1* r) { destroy(); });
    m_resource->setOnDestroy([this](CWpColorRepresentationSurfaceV1* r) { destroy(); });

    m_resource->setSetAlphaMode([this](CWpColorRepresentationSurfaceV1* r, wpColorRepresentationSurfaceV1AlphaMode mode) {
        if (!m_surface) {
            r->error(WP_COLOR_REPRESENTATION_SURFACE_V1_ERROR_INERT, "set_alpha_mode on an inert object");
            return;
        }

        // premultiplied electrical is the default, so there's nothing to set
        if (mode != WP_COLOR_REPRESENTATION_SURFACE_V1_ALPHA_MODE_PREMULTIPLIED_ELECTRICAL)
            r->error(WP_COLOR_REPRESENTATION_SURFACE_V1_ERROR_ALPHA_MODE, "unsupported alpha mode");
    });

    m_resource->setSetCoefficientsAndRange(
        [this](CWpColorRepresentationSurfaceV1* r, wpColorRepresentationSurfaceV1Coefficients coefficients, wpColorRepresentationSurfaceV1Range range) {
            if (!m_surface) {
                r->error(WP_COLOR_REPRESENTATION_SURFACE_V1_ERROR_INERT, "set_coefficients_and_range on an inert object");
                return;
            }

            const auto COEFFICIENTS = coefficientsFromWP(coefficients);
            const auto RANGE        = rangeFromWP(range);

            if (COEFFICIENTS == CM_YCBCR_COEFFICIENTS_UNSET || RANGE == CM_YCBCR_RANGE_UNSET) {
                r->error(WP_COLOR_REPRESENTATION_SURFACE_V1_ERROR_COEFFICIENTS, "unsupported coefficients and range");
                return;
            }

            auto representation         = m_surface->m_pending.colorRepresentation;
            representation.coefficients = COEFFICIENTS;
            representation.range        = RANGE;
            setPending(representation);
        });

    m_resource->setSetChromaLocation([this](CWpColorRepresentationSurfaceV1* r, wpColorRepresentationSurfaceV1ChromaLocation location) {
        if (!m_surface) {
            r->error(WP_COLOR_REPRESENTATION_SURFACE_V1_ERROR_INERT, "set_chroma_location on an inert object");
            return;
        }

        if (location < WP_COLOR_REPRESENTATION_SURFACE_V1_CHROMA_LOCATION_TYPE_0 || location > WP_COLOR_REPRESENTATION_SURFACE_V1_CHROMA_LOCATION_TYPE_5) {
            r->error(WP_COLOR_REPRESENTATION_SURFACE_V1_ERROR_CHROMA_LOCATION, "invalid chroma location");
            return;
        }

        auto representation           = m_surface->m_pending.colorRepresentation;
        representation.chromaLocation = sc<eChromaLocation>(location);
        setPending(representation);
    });

    m_listeners.surfacePrecommit = surface->m_events.precommit.listen([this] { onPrecommit(); });
}

bool CColorRepresentationSurface::good() {
    return m_resource->resource();
}

void CColorRepresentationSurface::destroy() {
    // unsetting is double-buffered as well
    if (m_surface)
        setPending({});

    PROTO::colorRepresentation->destroySurface(this);
}

void CColorRepresentationSurface::setPending(const SColorRepresentation& representation) {
    m_surface->m_pending.colorRepresentation              = representation;
    m_surface->m_pending.updated.bits.colorRepresentation = true;
}

void CColorRepresentationSurface::onPrecommit() {
    auto& pending = m_surface->m_pending;

    // the current contents look different now, repaint them
    if (pending.updated.bits.colorRepresentation && pending.colorRepresentation != m_surface->m_current.colorRepresentation) {
        pending.bufferDamage.add(CBox{{}, pending.bufferSize});
        pending.updated.bits.damage = true;
    }

    if (pending.colorRepresentation == SColorRepresentation{})
        return;

    const auto& BUFFER = pending.updated.bits.buffer ? pending.buffer : m_surface->m_current.buffer;
    if (!BUFFER || isYUVBuffer(BUFFER))
        return;

    m_resource->error(WP_COLOR_REPRESENTATION_SURFACE_V1_ERROR_PIXEL_FORMAT, "color representation set for a non-YUV buffer");
    pending.rejected = true;
}

CColorRepresentationProtocol::CColorRepresentationProtocol(const wl_interface* iface, const int& ver, const std::string& name) : IWaylandProtocol(iface, ver, name) {
    ;
}

void CColorRepresentationProtocol::bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id) {
    const auto RESOURCE = m_managers.emplace_back(makeUnique<CWpColorRepresentationManagerV1>(client, ver, id)).get();

    if UNLIKELY (!RESOURCE->resource()) {
        wl_client_post_no_memory(client);
        m_managers.pop_back();
        return;
    }

    RESOURCE->setDestroy([this](CWpColorRepresentationManagerV1* manager) { destroyManager(manager); });
    RESOURCE->setOnDestroy([this](CWpColorRepresentationManagerV1* manager) { destroyManager(manager); });
    RESOURCE->setGetSurface(
        [this](CWpColorRepresentationManagerV1* manager, uint32_t id, wl_resource* surface) { getSurface(manager, id, CWLSurfaceResource::fromResource(surface)); });

    RESOURCE->sendSupportedAlphaMode(WP_COLOR_REPRESENTATION_SURFACE_V1_ALPHA_MODE_PREMULTIPLIED_ELECTRICAL);

    for (const auto coefficients : SUPPORTED_COEFFICIENTS) {
        for (const auto range : SUPPORTED_RANGES)
            RESOURCE->sendSupportedCoefficientsAndRanges(coefficients, range);
    }

    RESOURCE->sendDone();
}

void CColorRepresentationProtocol::destroyManager(CWpColorRepresentationManagerV1* manager) {
    std::erase_if(m_managers, [&](const auto& other) { return other.get() == manager; });
}

void CColorRepresentationProtocol::destroySurface(CColorRepresentationSurface* surface) {
    std::erase_if(m_surfaces, [&](const auto& other) { return other.get() == surface; });
}

void CColorRepresentationProtocol::getSurface(CWpColorRepresentationManagerV1* manager, uint32_t id, SP<CWLSurfaceResource> surface) {
    if (std::ranges::any_of(m_surfaces, [&](const auto& other) { return other->m_surface == surface; })) {
        manager->error(WP_COLOR_REPRESENTATION_MANAGER_V1_ERROR_SURFACE_EXISTS, "color representation surface already exists");
        return;
    }

    const auto RESOURCE =
        m_surfaces.emplace_back(makeUnique<CColorRepresentationSurface>(makeUnique<CWpColorRepresentationSurfaceV1>(manager->client(), manager->version(), id), surface)).get();

    if UNLIKELY (!RESOURCE->good()) {
        manager->noMemory();
        m_surfaces.pop_back();
    }
}
