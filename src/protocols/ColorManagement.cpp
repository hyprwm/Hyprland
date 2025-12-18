#include "ColorManagement.hpp"
#include "Compositor.hpp"
#include "color-management-v1.hpp"
#include "../helpers/Monitor.hpp"
#include "core/Output.hpp"
#include "types/ColorManagement.hpp"
#include <cstdint>
#include <algorithm> 
#include <utility>  

using namespace NColorManagement;

const auto PRIMARIES_SCALE = 1000000.0f;


CColorManager::CColorManager(SP<CWpColorManagerV1> resource) : m_resource(resource) {
    if UNLIKELY (!good())
        return;

    m_resource->sendSupportedFeature(WP_COLOR_MANAGER_V1_FEATURE_PARAMETRIC);
    m_resource->sendSupportedFeature(WP_COLOR_MANAGER_V1_FEATURE_SET_PRIMARIES);
    m_resource->sendSupportedFeature(WP_COLOR_MANAGER_V1_FEATURE_SET_LUMINANCES);
    m_resource->sendSupportedFeature(WP_COLOR_MANAGER_V1_FEATURE_WINDOWS_SCRGB);

    if (PROTO::colorManagement->m_debug) {
        m_resource->sendSupportedFeature(WP_COLOR_MANAGER_V1_FEATURE_ICC_V2_V4);
        m_resource->sendSupportedFeature(WP_COLOR_MANAGER_V1_FEATURE_SET_TF_POWER);
        m_resource->sendSupportedFeature(WP_COLOR_MANAGER_V1_FEATURE_SET_MASTERING_DISPLAY_PRIMARIES);
        m_resource->sendSupportedFeature(WP_COLOR_MANAGER_V1_FEATURE_EXTENDED_TARGET_VOLUME);
    }

    m_resource->sendSupportedPrimariesNamed(WP_COLOR_MANAGER_V1_PRIMARIES_SRGB);
    m_resource->sendSupportedPrimariesNamed(WP_COLOR_MANAGER_V1_PRIMARIES_BT2020);
    m_resource->sendSupportedPrimariesNamed(WP_COLOR_MANAGER_V1_PRIMARIES_PAL_M);
    m_resource->sendSupportedPrimariesNamed(WP_COLOR_MANAGER_V1_PRIMARIES_PAL);
    m_resource->sendSupportedPrimariesNamed(WP_COLOR_MANAGER_V1_PRIMARIES_NTSC);
    m_resource->sendSupportedPrimariesNamed(WP_COLOR_MANAGER_V1_PRIMARIES_GENERIC_FILM);
    m_resource->sendSupportedPrimariesNamed(WP_COLOR_MANAGER_V1_PRIMARIES_DCI_P3);
    m_resource->sendSupportedPrimariesNamed(WP_COLOR_MANAGER_V1_PRIMARIES_DISPLAY_P3);
    m_resource->sendSupportedPrimariesNamed(WP_COLOR_MANAGER_V1_PRIMARIES_ADOBE_RGB);

    if (PROTO::colorManagement->m_debug) {
        m_resource->sendSupportedPrimariesNamed(WP_COLOR_MANAGER_V1_PRIMARIES_CIE1931_XYZ);
    }

    m_resource->sendSupportedTfNamed(WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_SRGB);
    m_resource->sendSupportedTfNamed(WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_GAMMA22);
    m_resource->sendSupportedTfNamed(WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_GAMMA28);
    m_resource->sendSupportedTfNamed(WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_EXT_LINEAR);
    m_resource->sendSupportedTfNamed(WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_ST2084_PQ);
    m_resource->sendSupportedTfNamed(WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_HLG);
    m_resource->sendSupportedTfNamed(WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_BT1886);
    m_resource->sendSupportedTfNamed(WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_ST240);
    m_resource->sendSupportedTfNamed(WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_LOG_100);
    m_resource->sendSupportedTfNamed(WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_LOG_316);
    m_resource->sendSupportedTfNamed(WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_XVYCC);
    m_resource->sendSupportedTfNamed(WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_EXT_SRGB);
    m_resource->sendSupportedTfNamed(WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_ST428);

    m_resource->sendSupportedIntent(WP_COLOR_MANAGER_V1_RENDER_INTENT_PERCEPTUAL);
    if (PROTO::colorManagement->m_debug) {
        m_resource->sendSupportedIntent(WP_COLOR_MANAGER_V1_RENDER_INTENT_RELATIVE);
        m_resource->sendSupportedIntent(WP_COLOR_MANAGER_V1_RENDER_INTENT_SATURATION);
        m_resource->sendSupportedIntent(WP_COLOR_MANAGER_V1_RENDER_INTENT_ABSOLUTE);
        m_resource->sendSupportedIntent(WP_COLOR_MANAGER_V1_RENDER_INTENT_RELATIVE_BPC);
    }

    m_resource->setDestroy([](CWpColorManagerV1* r) { 
        LOGM(TRACE, "Destroy WP_color_manager at {:x} (generated default)", (uintptr_t)r); 
    });

    m_resource->setGetOutput([](CWpColorManagerV1* r, uint32_t id, wl_resource* output) {
        LOGM(TRACE, "Get output for id={}, output={}", id, (uintptr_t)output);
        const auto OUTPUTRESOURCE = CWLOutputResource::fromResource(output);

        const auto RESOURCE = PROTO::colorManagement->m_outputs.emplace_back(
            makeShared<CColorManagementOutput>(makeShared<CWpColorManagementOutputV1>(r->client(), r->version(), id), OUTPUTRESOURCE));

        if UNLIKELY (!RESOURCE->good()) {
            r->noMemory();
            PROTO::colorManagement->m_outputs.pop_back();
            return;
        }
        RESOURCE->m_self = RESOURCE;
    });
    
    m_resource->setGetSurface([](CWpColorManagerV1* r, uint32_t id, wl_resource* surface) {
        LOGM(TRACE, "Get surface for id={}, surface={}", id, (uintptr_t)surface);
        auto SURF = CWLSurfaceResource::fromResource(surface);

        if (!SURF) {
            LOGM(ERR, "No surface for resource {}", (uintptr_t)surface);
            r->error(-1, "Invalid surface (2)");
            return;
        }

        if (SURF->m_colorManagement) {
            r->error(WP_COLOR_MANAGER_V1_ERROR_SURFACE_EXISTS, "CM Surface already exists");
            return;
        }

        const auto RESOURCE =
            PROTO::colorManagement->m_surfaces.emplace_back(makeShared<CColorManagementSurface>(makeShared<CWpColorManagementSurfaceV1>(r->client(), r->version(), id), SURF));
        if UNLIKELY (!RESOURCE->good()) {
            r->noMemory();
            PROTO::colorManagement->m_surfaces.pop_back();
            return;
        }

        RESOURCE->m_self = RESOURCE;

        SURF->m_colorManagement = RESOURCE;
    });
    m_resource->setGetSurfaceFeedback([](CWpColorManagerV1* r, uint32_t id, wl_resource* surface) {
        LOGM(TRACE, "Get feedback surface for id={}, surface={}", id, (uintptr_t)surface);
        auto SURF = CWLSurfaceResource::fromResource(surface);

        if (!SURF) {
            LOGM(ERR, "No surface for resource {}", (uintptr_t)surface);
            r->error(-1, "Invalid surface (2)");
            return;
        }

        const auto RESOURCE = PROTO::colorManagement->m_feedbackSurfaces.emplace_back(
            makeShared<CColorManagementFeedbackSurface>(makeShared<CWpColorManagementSurfaceFeedbackV1>(r->client(), r->version(), id), SURF));

        if UNLIKELY (!RESOURCE->good()) {
            r->noMemory();
            PROTO::colorManagement->m_feedbackSurfaces.pop_back();
            return;
        }

        RESOURCE->m_self = RESOURCE;
    });
    m_resource->setCreateIccCreator([](CWpColorManagerV1* r, uint32_t id) {
        LOGM(WARN, "New ICC creator for id={} (unsupported)", id);
        if (!PROTO::colorManagement->m_debug) {
            r->error(WP_COLOR_MANAGER_V1_ERROR_UNSUPPORTED_FEATURE, "ICC profiles are not supported");
            return;
        }

        const auto RESOURCE =
            PROTO::colorManagement->m_iccCreators.emplace_back(makeShared<CColorManagementIccCreator>(makeShared<CWpImageDescriptionCreatorIccV1>(r->client(), r->version(), id)));

        if UNLIKELY (!RESOURCE->good()) {
            r->noMemory();
            PROTO::colorManagement->m_iccCreators.pop_back();
            return;
        }

        RESOURCE->m_self = RESOURCE;
    });
    m_resource->setCreateParametricCreator([](CWpColorManagerV1* r, uint32_t id) {
        LOGM(TRACE, "New parametric creator for id={}", id);

        const auto RESOURCE = PROTO::colorManagement->m_parametricCreators.emplace_back(
            makeShared<CColorManagementParametricCreator>(makeShared<CWpImageDescriptionCreatorParamsV1>(r->client(), r->version(), id)));

        if UNLIKELY (!RESOURCE->good()) {
            r->noMemory();
            PROTO::colorManagement->m_parametricCreators.pop_back();
            return;
        }

        RESOURCE->m_self = RESOURCE;
    });

    m_resource->setCreateWindowsScrgb([](CWpColorManagerV1* r, uint32_t id) {
        LOGM(WARN, "New Windows scRGB description id={}", id);

        const auto RESOURCE = PROTO::colorManagement->m_imageDescriptions.emplace_back(
            makeShared<CColorManagementImageDescription>(makeShared<CWpImageDescriptionV1>(r->client(), r->version(), id), false));

        if UNLIKELY (!RESOURCE->good()) {
            r->noMemory();
            PROTO::colorManagement->m_imageDescriptions.pop_back();
            return;
        }

        RESOURCE->m_self                          = RESOURCE;
        RESOURCE->m_settings.windowsScRGB         = true;
        RESOURCE->m_settings.primariesNamed       = NColorManagement::CM_PRIMARIES_SRGB;
        RESOURCE->m_settings.primariesNameSet     = true;
        RESOURCE->m_settings.primaries            = NColorPrimaries::BT709;
        RESOURCE->m_settings.transferFunction     = NColorManagement::CM_TRANSFER_FUNCTION_EXT_LINEAR;
        
        // Hardcode valid HDR luminance values for scRGB to satisfy strict clients
        RESOURCE->m_settings.luminances.min       = 0.0001f; 
        RESOURCE->m_settings.luminances.reference = 80.0f;   
        RESOURCE->m_settings.luminances.max       = 600.0f; 

        RESOURCE->m_settings.masteringLuminances.min = 0.0001f;
        RESOURCE->m_settings.masteringLuminances.max = 600.0f;
        // Use BT.709 primaries for mastering volume as a safe default for sRGB container
        RESOURCE->m_settings.masteringPrimaries = NColorPrimaries::BT709; 

        RESOURCE->resource()->sendReady(RESOURCE->m_settings.updateId());
    });

    m_resource->setOnDestroy([this](CWpColorManagerV1* _) { PROTO::colorManagement->destroyResource(this); });

    m_resource->sendDone();
}

bool CColorManager::good() const {
    return m_resource->resource();
}

wl_client* CColorManager::client() const {
    return m_resource->client();
}

CColorManagementOutput::CColorManagementOutput(SP<CWpColorManagementOutputV1> resource, WP<CWLOutputResource> output) : m_resource(resource), m_output(output) {
    if UNLIKELY (!good())
        return;

    m_client = m_resource->client();

    m_resource->setDestroy([this](CWpColorManagementOutputV1* r) { PROTO::colorManagement->destroyResource(this); });
    m_resource->setOnDestroy([this](CWpColorManagementOutputV1* r) { PROTO::colorManagement->destroyResource(this); });

    m_resource->setGetImageDescription([this](CWpColorManagementOutputV1* r, uint32_t id) {
        LOGM(TRACE, "Get image description for output={}, id={}", (uintptr_t)r, id);

        if (m_imageDescription.valid())
            PROTO::colorManagement->destroyResource(m_imageDescription.get());

        const auto RESOURCE = PROTO::colorManagement->m_imageDescriptions.emplace_back(
            makeShared<CColorManagementImageDescription>(makeShared<CWpImageDescriptionV1>(r->client(), r->version(), id), true));

        if UNLIKELY (!RESOURCE->good()) {
            r->noMemory();
            PROTO::colorManagement->m_imageDescriptions.pop_back();
            return;
        }

        RESOURCE->m_self = RESOURCE;
        if (!m_output || !m_output->m_monitor.valid())
            RESOURCE->m_resource->sendFailed(WP_IMAGE_DESCRIPTION_V1_CAUSE_NO_OUTPUT, "No output");
        else {
            RESOURCE->m_settings = m_output->m_monitor->m_imageDescription;
            RESOURCE->m_resource->sendReady(RESOURCE->m_settings.updateId());
        }
    });
}

bool CColorManagementOutput::good() const {
    return m_resource->resource();
}

wl_client* CColorManagementOutput::client() const {
    return m_client;
}

CColorManagementSurface::CColorManagementSurface(SP<CWLSurfaceResource> surface_) : m_surface(surface_) {
    // only for frog cm until wayland cm is adopted
}

CColorManagementSurface::CColorManagementSurface(SP<CWpColorManagementSurfaceV1> resource, SP<CWLSurfaceResource> surface_) : m_surface(surface_), m_resource(resource) {
    if UNLIKELY (!good())
        return;

    m_client = m_resource->client();

    m_resource->setDestroy([this](CWpColorManagementSurfaceV1* r) {
        LOGM(TRACE, "Destroy wp cm surface {}", (uintptr_t)m_surface);
        PROTO::colorManagement->destroyResource(this);
    });
    m_resource->setOnDestroy([this](CWpColorManagementSurfaceV1* r) {
        LOGM(TRACE, "Destroy wp cm surface {}", (uintptr_t)m_surface);
        PROTO::colorManagement->destroyResource(this);
    });

    m_resource->setSetImageDescription([this](CWpColorManagementSurfaceV1* r, wl_resource* image_description, uint32_t render_intent) {
        LOGM(TRACE, "Set image description for surface={}, desc={}, intent={}", (uintptr_t)r, (uintptr_t)image_description, render_intent);

        const auto PO = sc<CWpImageDescriptionV1*>(wl_resource_get_user_data(image_description));
        if (!PO) { // FIXME check validity
            r->error(WP_COLOR_MANAGEMENT_SURFACE_V1_ERROR_IMAGE_DESCRIPTION, "Image description creation failed");
            return;
        }
        if (render_intent != WP_COLOR_MANAGER_V1_RENDER_INTENT_PERCEPTUAL) {
            r->error(WP_COLOR_MANAGEMENT_SURFACE_V1_ERROR_RENDER_INTENT, "Unsupported render intent");
            return;
        }

        const auto imageDescription =
            std::ranges::find_if(PROTO::colorManagement->m_imageDescriptions, [&](const auto& other) { return other->resource()->resource() == image_description; });
        if (imageDescription == PROTO::colorManagement->m_imageDescriptions.end()) {
            r->error(WP_COLOR_MANAGEMENT_SURFACE_V1_ERROR_IMAGE_DESCRIPTION, "Image description not found");
            return;
        }

        setHasImageDescription(true);
        m_imageDescription = imageDescription->get()->m_settings;
    });
    m_resource->setUnsetImageDescription([this](CWpColorManagementSurfaceV1* r) {
        LOGM(TRACE, "Unset image description for surface={}", (uintptr_t)r);
        m_imageDescription = SImageDescription{};
        setHasImageDescription(false);
    });
}

bool CColorManagementSurface::good() const {
    return m_resource && m_resource->resource();
}

wl_client* CColorManagementSurface::client() const {
    return m_client;
}

const SImageDescription& CColorManagementSurface::imageDescription() {
    if (!hasImageDescription())
        LOGM(WARN, "Reading imageDescription while none set. Returns default or empty values");
    return m_imageDescription;
}

bool CColorManagementSurface::hasImageDescription() {
    return m_hasImageDescription;
}

void CColorManagementSurface::setHasImageDescription(bool has) {
    m_hasImageDescription = has;
    m_needsNewMetadata    = true;
}

const hdr_output_metadata& CColorManagementSurface::hdrMetadata() {
    return m_hdrMetadata;
}

void CColorManagementSurface::setHDRMetadata(const hdr_output_metadata& metadata) {
    m_hdrMetadata          = metadata;
    m_lastImageDescription = m_imageDescription;
    m_needsNewMetadata     = false;
}

bool CColorManagementSurface::needsHdrMetadataUpdate() {
    if (!m_needsNewMetadata)
        return false;
    if (m_imageDescription == m_lastImageDescription)
        m_needsNewMetadata = false;
    return m_needsNewMetadata;
}

bool CColorManagementSurface::isHDR() {
    return m_imageDescription.transferFunction == CM_TRANSFER_FUNCTION_ST2084_PQ || m_imageDescription.transferFunction == CM_TRANSFER_FUNCTION_HLG || isWindowsScRGB();
}

bool CColorManagementSurface::isWindowsScRGB() {
    return m_imageDescription.windowsScRGB ||
        // autodetect scRGB, might be incorrect
        (m_imageDescription.primariesNamed == CM_PRIMARIES_SRGB && m_imageDescription.transferFunction == CM_TRANSFER_FUNCTION_EXT_LINEAR);
}

CColorManagementFeedbackSurface::CColorManagementFeedbackSurface(SP<CWpColorManagementSurfaceFeedbackV1> resource, SP<CWLSurfaceResource> surface_) :
    m_surface(surface_), m_resource(resource) {
    if UNLIKELY (!good())
        return;

    m_client = m_resource->client();

    m_resource->setDestroy([this](CWpColorManagementSurfaceFeedbackV1* r) {
        LOGM(TRACE, "Destroy wp cm feedback surface {}", (uintptr_t)m_surface);
        PROTO::colorManagement->destroyResource(this);
    });
    m_resource->setOnDestroy([this](CWpColorManagementSurfaceFeedbackV1* r) {
        LOGM(TRACE, "Destroy wp cm feedback surface {}", (uintptr_t)m_surface);
        PROTO::colorManagement->destroyResource(this);
    });

    m_resource->setGetPreferred([this](CWpColorManagementSurfaceFeedbackV1* r, uint32_t id) {
        LOGM(TRACE, "Get preferred for id {}", id);

        if (m_surface.expired()) {
            r->error(WP_COLOR_MANAGEMENT_SURFACE_FEEDBACK_V1_ERROR_INERT, "Surface is inert");
            return;
        }

        const auto RESOURCE = PROTO::colorManagement->m_imageDescriptions.emplace_back(
            makeShared<CColorManagementImageDescription>(makeShared<CWpImageDescriptionV1>(r->client(), r->version(), id), true));

        if UNLIKELY (!RESOURCE->good()) {
            r->noMemory();
            PROTO::colorManagement->m_imageDescriptions.pop_back();
            return;
        }

        RESOURCE->m_self     = RESOURCE;
        RESOURCE->m_settings = m_surface->getPreferredImageDescription();

        RESOURCE->resource()->sendReady(RESOURCE->m_settings.updateId());
    });

    m_resource->setGetPreferredParametric([this](CWpColorManagementSurfaceFeedbackV1* r, uint32_t id) {
        LOGM(TRACE, "Get preferred for id {}", id);

        if (m_surface.expired()) {
            r->error(WP_COLOR_MANAGEMENT_SURFACE_FEEDBACK_V1_ERROR_INERT, "Surface is inert");
            return;
        }

        const auto RESOURCE = PROTO::colorManagement->m_imageDescriptions.emplace_back(
            makeShared<CColorManagementImageDescription>(makeShared<CWpImageDescriptionV1>(r->client(), r->version(), id), true));

        if UNLIKELY (!RESOURCE->good()) {
            r->noMemory();
            PROTO::colorManagement->m_imageDescriptions.pop_back();
            return;
        }

        RESOURCE->m_self     = RESOURCE;
        RESOURCE->m_settings = m_surface->getPreferredImageDescription();
        m_currentPreferredId = RESOURCE->m_settings.updateId();

        if (!PROTO::colorManagement->m_debug && RESOURCE->m_settings.icc.fd >= 0) {
            LOGM(ERR, "FIXME: parse icc profile");
            r->error(WP_COLOR_MANAGER_V1_ERROR_UNSUPPORTED_FEATURE, "ICC profiles are not supported");
            return;
        }

        RESOURCE->resource()->sendReady(m_currentPreferredId);
    });

    m_listeners.enter = m_surface->m_events.enter.registerListener([this](std::any data) { onPreferredChanged(); });
    m_listeners.leave = m_surface->m_events.leave.registerListener([this](std::any data) { onPreferredChanged(); });
}

void CColorManagementFeedbackSurface::onPreferredChanged() {
    if (m_surface->m_enteredOutputs.size() == 1) {
        const auto newId = m_surface->getPreferredImageDescription().updateId();
        if (m_currentPreferredId != newId)
            m_resource->sendPreferredChanged(newId);
    }
}

bool CColorManagementFeedbackSurface::good() const {
    return m_resource->resource();
}

wl_client* CColorManagementFeedbackSurface::client() const {
    return m_client;
}

CColorManagementIccCreator::CColorManagementIccCreator(SP<CWpImageDescriptionCreatorIccV1> resource) : m_resource(resource) {
    if UNLIKELY (!good())
        return;
    //
    m_client = m_resource->client();

    m_resource->setOnDestroy([this](CWpImageDescriptionCreatorIccV1* r) { PROTO::colorManagement->destroyResource(this); });

    m_resource->setCreate([this](CWpImageDescriptionCreatorIccV1* r, uint32_t id) {
        LOGM(TRACE, "Create image description from icc for id {}", id);

        // FIXME actually check completeness
        if (m_settings.icc.fd < 0 || !m_settings.icc.length) {
            r->error(WP_IMAGE_DESCRIPTION_CREATOR_PARAMS_V1_ERROR_INCOMPLETE_SET, "Missing required settings");
            return;
        }

        const auto RESOURCE = PROTO::colorManagement->m_imageDescriptions.emplace_back(
            makeShared<CColorManagementImageDescription>(makeShared<CWpImageDescriptionV1>(r->client(), r->version(), id), false));

        if UNLIKELY (!RESOURCE->good()) {
            r->noMemory();
            PROTO::colorManagement->m_imageDescriptions.pop_back();
            return;
        }

        LOGM(ERR, "FIXME: Parse icc file {}({},{}) for id {}", m_settings.icc.fd, m_settings.icc.offset, m_settings.icc.length, id);

        // FIXME actually check support
        if (m_settings.icc.fd < 0 || !m_settings.icc.length) {
            RESOURCE->resource()->sendFailed(WP_IMAGE_DESCRIPTION_V1_CAUSE_UNSUPPORTED, "unsupported");
            return;
        }

        RESOURCE->m_self     = RESOURCE;
        RESOURCE->m_settings = m_settings;
        RESOURCE->resource()->sendReady(m_settings.updateId());

        PROTO::colorManagement->destroyResource(this);
    });

    m_resource->setSetIccFile([this](CWpImageDescriptionCreatorIccV1* r, int fd, uint32_t offset, uint32_t length) {
        m_settings.icc.fd     = fd;
        m_settings.icc.offset = offset;
        m_settings.icc.length = length;
    });
}

bool CColorManagementIccCreator::good() const {
    return m_resource->resource();
}

wl_client* CColorManagementIccCreator::client() const {
    return m_client;
}

CColorManagementParametricCreator::CColorManagementParametricCreator(SP<CWpImageDescriptionCreatorParamsV1> resource) : m_resource(resource) {
    if UNLIKELY (!good())
        return;
    
    m_client = m_resource->client();

    m_resource->setOnDestroy([this](CWpImageDescriptionCreatorParamsV1* r) { PROTO::colorManagement->destroyResource(this); });

    m_resource->setCreate([this](CWpImageDescriptionCreatorParamsV1* r, uint32_t id) {
        LOGM(TRACE, "Create image description from params for id {}", id);

        if (!m_valuesSet) {
            r->error(WP_IMAGE_DESCRIPTION_CREATOR_PARAMS_V1_ERROR_INCOMPLETE_SET, "Missing required settings");
            return;
        }

        const auto RESOURCE = PROTO::colorManagement->m_imageDescriptions.emplace_back(
            makeShared<CColorManagementImageDescription>(makeShared<CWpImageDescriptionV1>(r->client(), r->version(), id), false));

        if UNLIKELY (!RESOURCE->good()) {
            r->noMemory();
            PROTO::colorManagement->m_imageDescriptions.pop_back();
            return;
        }

        const bool supportsExtended = PROTO::colorManagement->m_debug;

        if (hasFlag(eValuesSet::PC_PRIMARIES) && hasFlag(eValuesSet::PC_MASTERING_PRIMARIES) && !supportsExtended) {
            const auto& p = m_settings.primaries;
            const auto& m = m_settings.masteringPrimaries;

            const auto isInside = [](const auto& pt, const auto& v1, const auto& v2, const auto& v3) -> bool {
                // A small tolerance for floating point jitter (approx 0.0001 in normalized space)
                constexpr float EPSILON = 1e-4f; 

                auto sign = [](const auto& p1, const auto& p2, const auto& p3) {
                    return (p1.x - p3.x) * (p2.y - p3.y) - (p2.x - p3.x) * (p1.y - p3.y);
                };

                const float d1 = sign(pt, v1, v2);
                const float d2 = sign(pt, v2, v3);
                const float d3 = sign(pt, v3, v1);

                const bool has_neg = (d1 < -EPSILON) || (d2 < -EPSILON) || (d3 < -EPSILON);
                const bool has_pos = (d1 > EPSILON)  || (d2 > EPSILON)  || (d3 > EPSILON);

                return !(has_neg && has_pos);
            };

            // Check Red, Green, and Blue mastering points
            bool outOfBounds = !isInside(m.red,   p.red, p.green, p.blue) ||
                               !isInside(m.green, p.red, p.green, p.blue) ||
                               !isInside(m.blue,  p.red, p.green, p.blue);

            if (outOfBounds) {
                LOGM(WARN, "Mastering primaries outside content gamut. R_in:{} G_in:{} B_in:{}", 
                    isInside(m.red, p.red, p.green, p.blue),
                    isInside(m.green, p.red, p.green, p.blue),
                    isInside(m.blue, p.red, p.green, p.blue));

                RESOURCE->resource()->sendFailed(WP_IMAGE_DESCRIPTION_V1_CAUSE_UNSUPPORTED, "Mastering primaries exceed content primaries");
                PROTO::colorManagement->destroyResource(this);
                return;
            }
        }

        RESOURCE->m_self     = RESOURCE;
        RESOURCE->m_settings = m_settings;
        RESOURCE->resource()->sendReady(m_settings.updateId());

        PROTO::colorManagement->destroyResource(this);
    });    
    
    m_resource->setSetTfNamed([this](CWpImageDescriptionCreatorParamsV1* r, uint32_t tf) {
        LOGM(TRACE, "Set image description transfer function to {}", tf);
        
        if (hasFlag(eValuesSet::PC_TF)) {
            r->error(WP_IMAGE_DESCRIPTION_CREATOR_PARAMS_V1_ERROR_ALREADY_SET, "Transfer function already set");
            return;
        }
        
        switch (tf) {
            case WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_SRGB: 
            case WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_ST2084_PQ: 
            case WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_EXT_LINEAR: 
            case WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_GAMMA22: 
            case WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_GAMMA28: 
            case WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_HLG: 
            case WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_BT1886: 
            case WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_ST240: 
            case WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_LOG_100: 
            case WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_LOG_316: 
            case WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_XVYCC: 
            case WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_EXT_SRGB: 
            case WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_ST428: 
                break;
            default: 
                r->error(WP_IMAGE_DESCRIPTION_CREATOR_PARAMS_V1_ERROR_INVALID_TF, "Unsupported transfer function"); 
                return;
        }

        m_settings.transferFunction = convertTransferFunction(sc<wpColorManagerV1TransferFunction>(tf));
        setFlag(eValuesSet::PC_TF);
    });

    m_resource->setSetTfPower([this](CWpImageDescriptionCreatorParamsV1* r, uint32_t eexp) {
        if (hasFlag(eValuesSet::PC_TF_POWER)) {
            r->error(WP_IMAGE_DESCRIPTION_CREATOR_PARAMS_V1_ERROR_ALREADY_SET, "Transfer function power already set");
            return;
        }
        if (!PROTO::colorManagement->m_debug) {
            r->error(WP_COLOR_MANAGER_V1_ERROR_UNSUPPORTED_FEATURE, "TF power is not supported");
            return;
        }

        float power = eexp / 10000.0f;
        
        if (power < 1.0f || power > 10.0f) {
            r->error(WP_IMAGE_DESCRIPTION_CREATOR_PARAMS_V1_ERROR_INVALID_TF, "Power should be between 1.0 and 10.0");
            return;
        }
        
        m_settings.transferFunctionPower = power;
        setFlag(eValuesSet::PC_TF_POWER);
    });

   m_resource->setSetPrimariesNamed([this](CWpImageDescriptionCreatorParamsV1* r, uint32_t primaries) {
        if (hasFlag(eValuesSet::PC_PRIMARIES)) {
            r->error(WP_IMAGE_DESCRIPTION_CREATOR_PARAMS_V1_ERROR_ALREADY_SET, "Primaries already set");
            return;
        }

        switch (primaries) {
            case WP_COLOR_MANAGER_V1_PRIMARIES_SRGB:
            case WP_COLOR_MANAGER_V1_PRIMARIES_BT2020:
            case WP_COLOR_MANAGER_V1_PRIMARIES_PAL_M:
            case WP_COLOR_MANAGER_V1_PRIMARIES_PAL:
            case WP_COLOR_MANAGER_V1_PRIMARIES_NTSC:
            case WP_COLOR_MANAGER_V1_PRIMARIES_GENERIC_FILM:
            case WP_COLOR_MANAGER_V1_PRIMARIES_DCI_P3:
            case WP_COLOR_MANAGER_V1_PRIMARIES_DISPLAY_P3:
            case WP_COLOR_MANAGER_V1_PRIMARIES_ADOBE_RGB: break;
            default:
                if (!PROTO::colorManagement->m_debug) {
                    r->error(WP_IMAGE_DESCRIPTION_CREATOR_PARAMS_V1_ERROR_INVALID_PRIMARIES_NAMED, "Unsupported primaries");
                    return;
                }
        }

        m_settings.primariesNameSet = true;
        m_settings.primariesNamed   = convertPrimaries(sc<wpColorManagerV1Primaries>(primaries));
        m_settings.primaries        = getPrimaries(m_settings.primariesNamed);
        setFlag(eValuesSet::PC_PRIMARIES);
    });

    m_resource->setSetPrimaries(
        [this](CWpImageDescriptionCreatorParamsV1* r, int32_t r_x, int32_t r_y, int32_t g_x, int32_t g_y, int32_t b_x, int32_t b_y, int32_t w_x, int32_t w_y) {
            if (hasFlag(eValuesSet::PC_PRIMARIES)) {
                r->error(WP_IMAGE_DESCRIPTION_CREATOR_PARAMS_V1_ERROR_ALREADY_SET, "Primaries already set");
                return;
            }
            m_settings.primariesNameSet = false;
        
            m_settings.primaries        = SPCPRimaries{
                .red   = {.x = r_x / PRIMARIES_SCALE, .y = r_y / PRIMARIES_SCALE},
                .green = {.x = g_x / PRIMARIES_SCALE, .y = g_y / PRIMARIES_SCALE},
                .blue  = {.x = b_x / PRIMARIES_SCALE, .y = b_y / PRIMARIES_SCALE},
                .white = {.x = w_x / PRIMARIES_SCALE, .y = w_y / PRIMARIES_SCALE}
            };
            setFlag(eValuesSet::PC_PRIMARIES);
        });

    m_resource->setSetLuminances([this](CWpImageDescriptionCreatorParamsV1* r, uint32_t min_lum, uint32_t max_lum, uint32_t reference_lum) {
        auto min = min_lum / 10000.0f;
        if (hasFlag(eValuesSet::PC_LUMINANCES)) {
            r->error(WP_IMAGE_DESCRIPTION_CREATOR_PARAMS_V1_ERROR_ALREADY_SET, "Luminances already set");
            return;
        }
        if (max_lum <= min || reference_lum <= min) {
            r->error(WP_IMAGE_DESCRIPTION_CREATOR_PARAMS_V1_ERROR_INVALID_LUMINANCE, "Invalid luminances");
            return;
        }
        m_settings.luminances = SImageDescription::SPCLuminances{.min = min, .max = max_lum, .reference = reference_lum};
        setFlag(eValuesSet::PC_LUMINANCES);
    });
    
    m_resource->setSetMasteringDisplayPrimaries(
        [this](CWpImageDescriptionCreatorParamsV1* r, int32_t r_x, int32_t r_y, int32_t g_x, int32_t g_y, int32_t b_x, int32_t b_y, int32_t w_x, int32_t w_y) {
        LOGM(TRACE, "Set image description mastering primaries: R:{},{} G:{},{} B:{},{} W:{},{}", r_x, r_y, g_x, g_y, b_x, b_y, w_x, w_y);

        if (hasFlag(eValuesSet::PC_MASTERING_PRIMARIES)) {
            r->error(WP_IMAGE_DESCRIPTION_CREATOR_PARAMS_V1_ERROR_ALREADY_SET, "Mastering primaries already set");
            return;
        }

        if (!PROTO::colorManagement->m_debug) {
            r->error(WP_COLOR_MANAGER_V1_ERROR_UNSUPPORTED_FEATURE, "Mastering primaries are not supported");
            return;
        }

        const auto toNorm = [](int32_t val) -> float { return val / PRIMARIES_SCALE; };

        m_settings.masteringPrimaries = SPCPRimaries{
            .red   = {.x = toNorm(r_x), .y = toNorm(r_y)},
            .green = {.x = toNorm(g_x), .y = toNorm(g_y)},
            .blue  = {.x = toNorm(b_x), .y = toNorm(b_y)},
            .white = {.x = toNorm(w_x), .y = toNorm(w_y)}
        };

        setFlag(eValuesSet::PC_MASTERING_PRIMARIES);
    });

    m_resource->setSetMasteringLuminance([this](CWpImageDescriptionCreatorParamsV1* r, uint32_t min_lum, uint32_t max_lum) {
        auto min = min_lum / 10000.0f;
        LOGM(TRACE, "Set image description mastering luminances to {} - {}", min, max_lum);
        
        if (hasFlag(eValuesSet::PC_MASTERING_LUMINANCES)) {
             r->error(WP_IMAGE_DESCRIPTION_CREATOR_PARAMS_V1_ERROR_ALREADY_SET, "Mastering luminances already set");
             return;
        }

        if (min > 0 && max_lum > 0 && max_lum <= min) {
            r->error(WP_IMAGE_DESCRIPTION_CREATOR_PARAMS_V1_ERROR_INVALID_LUMINANCE, "Invalid luminances");
            return;
        }
        if (!PROTO::colorManagement->m_debug) {
            r->error(WP_COLOR_MANAGER_V1_ERROR_UNSUPPORTED_FEATURE, "Mastering luminances are not supported");
            return;
        }
        m_settings.masteringLuminances = SImageDescription::SPCMasteringLuminances{.min = min, .max = max_lum};
        
        setFlag(eValuesSet::PC_MASTERING_LUMINANCES);
    });

    m_resource->setSetMaxCll([this](CWpImageDescriptionCreatorParamsV1* r, uint32_t max_cll) {
        LOGM(TRACE, "Set image description max content light level to {}", max_cll);
        
        if (hasFlag(eValuesSet::PC_CLL)) {
            r->error(WP_IMAGE_DESCRIPTION_CREATOR_PARAMS_V1_ERROR_ALREADY_SET, "Max CLL already set");
            return;
        }
        m_settings.maxCLL = max_cll;
        
        setFlag(eValuesSet::PC_CLL);
    });

    m_resource->setSetMaxFall([this](CWpImageDescriptionCreatorParamsV1* r, uint32_t max_fall) {
        LOGM(TRACE, "Set image description max frame-average light level to {}", max_fall);
        
        if (hasFlag(eValuesSet::PC_FALL)) {
             r->error(WP_IMAGE_DESCRIPTION_CREATOR_PARAMS_V1_ERROR_ALREADY_SET, "Max FALL already set");
             return;
        }
        m_settings.maxFALL = max_fall;
        
        setFlag(eValuesSet::PC_FALL);
    });
}

bool CColorManagementParametricCreator::good() const {
    return m_resource->resource();
}

wl_client* CColorManagementParametricCreator::client() const {
    return m_client;
}

CColorManagementImageDescription::CColorManagementImageDescription(SP<CWpImageDescriptionV1> resource, bool allowGetInformation) :
    m_resource(resource), m_allowGetInformation(allowGetInformation) {
    if UNLIKELY (!good())
        return;

    m_client = m_resource->client();

    m_resource->setDestroy([this](CWpImageDescriptionV1* r) { PROTO::colorManagement->destroyResource(this); });
    m_resource->setOnDestroy([this](CWpImageDescriptionV1* r) { PROTO::colorManagement->destroyResource(this); });

    m_resource->setGetInformation([this](CWpImageDescriptionV1* r, uint32_t id) {
        LOGM(TRACE, "Get image information for image={}, id={}", (uintptr_t)r, id);
        if (!m_allowGetInformation) {
            r->error(WP_IMAGE_DESCRIPTION_V1_ERROR_NO_INFORMATION, "Image descriptions doesn't allow get_information request");
            return;
        }

        // Store in m_infos to keep it alive
        auto INFO = m_infos.emplace_back(makeShared<CColorManagementImageDescriptionInfo>(
            makeShared<CWpImageDescriptionInfoV1>(r->client(), r->version(), id), 
            m_settings
        ));

        if UNLIKELY (!INFO->good()) {
            r->noMemory();
            m_infos.pop_back();
            return;
        }
    });
}

void CColorManagementImageDescription::destroyResource(CColorManagementImageDescriptionInfo* resource) {
    std::erase_if(m_infos, [&](const auto& other) { return other.get() == resource; });
}

bool CColorManagementImageDescription::good() const {
    return m_resource->resource();
}

wl_client* CColorManagementImageDescription::client() const {
    return m_client;
}

SP<CWpImageDescriptionV1> CColorManagementImageDescription::resource() {
    return m_resource;
}

CColorManagementImageDescriptionInfo::CColorManagementImageDescriptionInfo(SP<CWpImageDescriptionInfoV1> resource, const SImageDescription& settings_) :
    m_resource(resource), m_settings(settings_) {
    if UNLIKELY (!good())
        return;

    m_client = m_resource->client();
    
    const auto toProto = [](float value) { return static_cast<int32_t>(std::round(value * PRIMARIES_SCALE)); };

    if (m_settings.icc.fd >= 0)
        m_resource->sendIccFile(m_settings.icc.fd, m_settings.icc.length);

    bool validPrimaries = m_settings.primaries.red.x > 0 || m_settings.primaries.green.x > 0;
    
    if (!validPrimaries) {
        // Fallback: sRGB primaries
        m_resource->sendPrimaries(
            toProto(0.640f), toProto(0.330f), // Red
            toProto(0.300f), toProto(0.600f), // Green
            toProto(0.150f), toProto(0.060f), // Blue
            toProto(0.3127f), toProto(0.3290f) // White Point
        );
    } else {
        m_resource->sendPrimaries(
            toProto(m_settings.primaries.red.x), toProto(m_settings.primaries.red.y), 
            toProto(m_settings.primaries.green.x), toProto(m_settings.primaries.green.y), 
            toProto(m_settings.primaries.blue.x), toProto(m_settings.primaries.blue.y),
            toProto(m_settings.primaries.white.x), toProto(m_settings.primaries.white.y)
        );
    }
                              
    if (m_settings.primariesNameSet)
        m_resource->sendPrimariesNamed(m_settings.primariesNamed);

    auto tfToSend = m_settings.transferFunction;
    if (tfToSend == 0)
        tfToSend = CM_TRANSFER_FUNCTION_SRGB;

    m_resource->sendTfNamed(tfToSend);

    if (m_settings.transferFunctionPower > 0.0f) {
        m_resource->sendTfPower(static_cast<uint32_t>(std::round(m_settings.transferFunctionPower * 10000)));
    }

    
    uint32_t minLum = static_cast<uint32_t>(std::round(m_settings.luminances.min * 10000));
    uint32_t maxLum = m_settings.luminances.max; 
    uint32_t refLum = m_settings.luminances.reference;

    if (maxLum == 0) maxLum = 80; 
    if (refLum == 0) refLum = 80; 
    if (maxLum < refLum) maxLum = refLum;

    m_resource->sendLuminances(minLum, maxLum, refLum);

    if (m_settings.masteringPrimaries.red.x > 0) {
        m_resource->sendTargetPrimaries(toProto(m_settings.masteringPrimaries.red.x), toProto(m_settings.masteringPrimaries.red.y), 
                                        toProto(m_settings.masteringPrimaries.green.x), toProto(m_settings.masteringPrimaries.green.y), 
                                        toProto(m_settings.masteringPrimaries.blue.x), toProto(m_settings.masteringPrimaries.blue.y),
                                        toProto(m_settings.masteringPrimaries.white.x), toProto(m_settings.masteringPrimaries.white.y));
    }

    if (m_settings.masteringLuminances.max > 0) {
        m_resource->sendTargetLuminance(static_cast<uint32_t>(std::round(m_settings.masteringLuminances.min * 10000)), m_settings.masteringLuminances.max);
    }

    if (m_settings.maxCLL > 0) {
        m_resource->sendTargetMaxCll(m_settings.maxCLL);
    }

    if (m_settings.maxFALL > 0) {
        m_resource->sendTargetMaxFall(m_settings.maxFALL);
    }

    m_resource->sendDone();
}

bool CColorManagementImageDescriptionInfo::good() const {
    return m_resource->resource();
}

wl_client* CColorManagementImageDescriptionInfo::client() const {
    return m_client;
}

CColorManagementProtocol::CColorManagementProtocol(const wl_interface* iface, const int& ver, const std::string& name, bool debug) :
    IWaylandProtocol(iface, ver, name), m_debug(debug) {
    ;
}

void CColorManagementProtocol::bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id) {
    const auto RESOURCE = m_managers.emplace_back(makeShared<CColorManager>(makeShared<CWpColorManagerV1>(client, ver, id)));

    if UNLIKELY (!RESOURCE->good()) {
        wl_client_post_no_memory(client);
        m_managers.pop_back();
        return;
    }

    LOGM(TRACE, "New WP_color_manager at {:x}", (uintptr_t)RESOURCE.get());
}

void CColorManagementProtocol::onImagePreferredChanged(uint32_t preferredId) {
    for (auto const& feedback : m_feedbackSurfaces) {
        feedback->m_resource->sendPreferredChanged(preferredId);
    }
}

void CColorManagementProtocol::onMonitorImageDescriptionChanged(WP<CMonitor> monitor) {
    for (auto const& output : m_outputs) {
        if (output->m_output && output->m_output->m_monitor == monitor)
            output->m_resource->sendImageDescriptionChanged();
    }
    // recheck feedbacks
    for (auto const& feedback : m_feedbackSurfaces)
        feedback->onPreferredChanged();
}

bool CColorManagementProtocol::isClientCMAware(wl_client* client) {
    return std::ranges::any_of(m_managers, [client](const auto& m) { return m->client() == client; });
}

void CColorManagementProtocol::destroyResource(CColorManager* resource) {
    std::erase_if(m_managers, [&](const auto& other) { return other.get() == resource; });
}

void CColorManagementProtocol::destroyResource(CColorManagementOutput* resource) {
    std::erase_if(m_outputs, [&](const auto& other) { return other.get() == resource; });
}

void CColorManagementProtocol::destroyResource(CColorManagementSurface* resource) {
    std::erase_if(m_surfaces, [&](const auto& other) { return other.get() == resource; });
}

void CColorManagementProtocol::destroyResource(CColorManagementFeedbackSurface* resource) {
    std::erase_if(m_feedbackSurfaces, [&](const auto& other) { return other.get() == resource; });
}

void CColorManagementProtocol::destroyResource(CColorManagementIccCreator* resource) {
    std::erase_if(m_iccCreators, [&](const auto& other) { return other.get() == resource; });
}

void CColorManagementProtocol::destroyResource(CColorManagementParametricCreator* resource) {
    std::erase_if(m_parametricCreators, [&](const auto& other) { return other.get() == resource; });
}

void CColorManagementProtocol::destroyResource(CColorManagementImageDescription* resource) {
    std::erase_if(m_imageDescriptions, [&](const auto& other) { return other.get() == resource; });
}