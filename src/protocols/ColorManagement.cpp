#include "ColorManagement.hpp"
#include "Compositor.hpp"
#include "color-management-v1.hpp"
#include "../helpers/Monitor.hpp"
#include "core/Output.hpp"
#include "types/ColorManagement.hpp"
#include <cstdint>

using namespace NColorManagement;

CColorManager::CColorManager(SP<CWpColorManagerV1> resource) : m_resource(resource) {
    if UNLIKELY (!good())
        return;

    m_resource->sendSupportedFeature(WP_COLOR_MANAGER_V1_FEATURE_PARAMETRIC);
    m_resource->sendSupportedFeature(WP_COLOR_MANAGER_V1_FEATURE_SET_PRIMARIES);
    m_resource->sendSupportedFeature(WP_COLOR_MANAGER_V1_FEATURE_SET_LUMINANCES);

    if (PROTO::colorManagement->m_debug) {
        m_resource->sendSupportedFeature(WP_COLOR_MANAGER_V1_FEATURE_ICC_V2_V4);
        m_resource->sendSupportedFeature(WP_COLOR_MANAGER_V1_FEATURE_SET_TF_POWER);
        m_resource->sendSupportedFeature(WP_COLOR_MANAGER_V1_FEATURE_SET_MASTERING_DISPLAY_PRIMARIES);
        m_resource->sendSupportedFeature(WP_COLOR_MANAGER_V1_FEATURE_EXTENDED_TARGET_VOLUME);
        m_resource->sendSupportedFeature(WP_COLOR_MANAGER_V1_FEATURE_WINDOWS_SCRGB);
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

    if (PROTO::colorManagement->m_debug) {
        m_resource->sendSupportedTfNamed(WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_BT1886);
        m_resource->sendSupportedTfNamed(WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_ST240);
        m_resource->sendSupportedTfNamed(WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_LOG_100);
        m_resource->sendSupportedTfNamed(WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_LOG_316);
        m_resource->sendSupportedTfNamed(WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_XVYCC);
        m_resource->sendSupportedTfNamed(WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_EXT_SRGB);
        m_resource->sendSupportedTfNamed(WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_ST428);
    }

    m_resource->sendSupportedIntent(WP_COLOR_MANAGER_V1_RENDER_INTENT_PERCEPTUAL);
    if (PROTO::colorManagement->m_debug) {
        m_resource->sendSupportedIntent(WP_COLOR_MANAGER_V1_RENDER_INTENT_RELATIVE);
        m_resource->sendSupportedIntent(WP_COLOR_MANAGER_V1_RENDER_INTENT_SATURATION);
        m_resource->sendSupportedIntent(WP_COLOR_MANAGER_V1_RENDER_INTENT_ABSOLUTE);
        m_resource->sendSupportedIntent(WP_COLOR_MANAGER_V1_RENDER_INTENT_RELATIVE_BPC);
    }

    m_resource->setDestroy([](CWpColorManagerV1* r) { LOGM(TRACE, "Destroy WP_color_manager at {:x} (generated default)", (uintptr_t)r); });
    m_resource->setGetOutput([](CWpColorManagerV1* r, uint32_t id, wl_resource* output) {
        LOGM(TRACE, "Get output for id={}, output={}", id, (uintptr_t)output);

        const auto OUTPUTRESOURCE = CWLOutputResource::fromResource(output);

        if UNLIKELY (!OUTPUTRESOURCE) {
            r->error(-1, "Invalid output (2)");
            return;
        }

        const auto PMONITOR = OUTPUTRESOURCE->m_monitor.lock();

        if UNLIKELY (!PMONITOR) {
            r->error(-1, "Invalid output (2)");
            return;
        }

        const auto RESOURCE =
            PROTO::colorManagement->m_outputs.emplace_back(makeShared<CColorManagementOutput>(makeShared<CWpColorManagementOutputV1>(r->client(), r->version(), id), PMONITOR));

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
        LOGM(WARN, "New Windows scRGB description id={} (unsupported)", id);
        if (!PROTO::colorManagement->m_debug) {
            r->error(WP_COLOR_MANAGER_V1_ERROR_UNSUPPORTED_FEATURE, "Windows scRGB profiles are not supported");
            return;
        }

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
        RESOURCE->m_settings.luminances.reference = 203;
        RESOURCE->resource()->sendReady(RESOURCE->m_settings.updateId());
    });

    m_resource->setOnDestroy([this](CWpColorManagerV1* r) { PROTO::colorManagement->destroyResource(this); });

    m_resource->sendDone();
}

bool CColorManager::good() {
    return m_resource->resource();
}

CColorManagementOutput::CColorManagementOutput(SP<CWpColorManagementOutputV1> resource, WP<CMonitor> monitor) : m_resource(resource), m_monitor(monitor) {
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
        if (!m_monitor.valid())
            RESOURCE->m_resource->sendFailed(WP_IMAGE_DESCRIPTION_V1_CAUSE_NO_OUTPUT, "No output");
        else {
            RESOURCE->m_settings = m_monitor->m_imageDescription;
            RESOURCE->m_resource->sendReady(RESOURCE->m_settings.updateId());
        }
    });
}

bool CColorManagementOutput::good() {
    return m_resource->resource();
}

wl_client* CColorManagementOutput::client() {
    return m_client;
}

CColorManagementSurface::CColorManagementSurface(SP<CWLSurfaceResource> surface_) : m_surface(surface_) {
    // only for frog cm untill wayland cm is adopted
}

CColorManagementSurface::CColorManagementSurface(SP<CWpColorManagementSurfaceV1> resource, SP<CWLSurfaceResource> surface_) : m_surface(surface_), m_resource(resource) {
    if UNLIKELY (!good())
        return;

    m_client = m_resource->client();

    m_resource->setDestroy([this](CWpColorManagementSurfaceV1* r) {
        LOGM(TRACE, "Destroy xx cm surface {}", (uintptr_t)m_surface);
        PROTO::colorManagement->destroyResource(this);
    });
    m_resource->setOnDestroy([this](CWpColorManagementSurfaceV1* r) {
        LOGM(TRACE, "Destroy xx cm surface {}", (uintptr_t)m_surface);
        PROTO::colorManagement->destroyResource(this);
    });

    m_resource->setSetImageDescription([this](CWpColorManagementSurfaceV1* r, wl_resource* image_description, uint32_t render_intent) {
        LOGM(TRACE, "Set image description for surface={}, desc={}, intent={}", (uintptr_t)r, (uintptr_t)image_description, render_intent);

        const auto PO = (CWpImageDescriptionV1*)wl_resource_get_user_data(image_description);
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

bool CColorManagementSurface::good() {
    return m_resource && m_resource->resource();
}

wl_client* CColorManagementSurface::client() {
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

CColorManagementFeedbackSurface::CColorManagementFeedbackSurface(SP<CWpColorManagementSurfaceFeedbackV1> resource, SP<CWLSurfaceResource> surface_) :
    m_surface(surface_), m_resource(resource) {
    if UNLIKELY (!good())
        return;

    m_client = m_resource->client();

    m_resource->setDestroy([this](CWpColorManagementSurfaceFeedbackV1* r) {
        LOGM(TRACE, "Destroy xx cm feedback surface {}", (uintptr_t)m_surface);
        if (m_currentPreferred.valid())
            PROTO::colorManagement->destroyResource(m_currentPreferred.get());
        PROTO::colorManagement->destroyResource(this);
    });
    m_resource->setOnDestroy([this](CWpColorManagementSurfaceFeedbackV1* r) {
        LOGM(TRACE, "Destroy xx cm feedback surface {}", (uintptr_t)m_surface);
        if (m_currentPreferred.valid())
            PROTO::colorManagement->destroyResource(m_currentPreferred.get());
        PROTO::colorManagement->destroyResource(this);
    });

    m_resource->setGetPreferred([this](CWpColorManagementSurfaceFeedbackV1* r, uint32_t id) {
        LOGM(TRACE, "Get preferred for id {}", id);

        if (m_currentPreferred.valid())
            PROTO::colorManagement->destroyResource(m_currentPreferred.get());

        const auto RESOURCE = PROTO::colorManagement->m_imageDescriptions.emplace_back(
            makeShared<CColorManagementImageDescription>(makeShared<CWpImageDescriptionV1>(r->client(), r->version(), id), true));

        if UNLIKELY (!RESOURCE->good()) {
            r->noMemory();
            PROTO::colorManagement->m_imageDescriptions.pop_back();
            return;
        }

        RESOURCE->m_self   = RESOURCE;
        m_currentPreferred = RESOURCE;

        m_currentPreferred->m_settings = g_pCompositor->getPreferredImageDescription();
        RESOURCE->resource()->sendReady(m_currentPreferred->m_settings.updateId());
    });

    m_resource->setGetPreferredParametric([this](CWpColorManagementSurfaceFeedbackV1* r, uint32_t id) {
        LOGM(TRACE, "Get preferred for id {}", id);

        if (!PROTO::colorManagement->m_debug) {
            r->error(WP_COLOR_MANAGER_V1_ERROR_UNSUPPORTED_FEATURE, "Parametric descriptions are not supported");
            return;
        }

        if (m_surface.expired()) {
            r->error(WP_COLOR_MANAGEMENT_SURFACE_FEEDBACK_V1_ERROR_INERT, "Surface is inert");
            return;
        }

        if (m_currentPreferred.valid())
            PROTO::colorManagement->destroyResource(m_currentPreferred.get());

        const auto RESOURCE = PROTO::colorManagement->m_imageDescriptions.emplace_back(
            makeShared<CColorManagementImageDescription>(makeShared<CWpImageDescriptionV1>(r->client(), r->version(), id), true));

        if UNLIKELY (!RESOURCE->good()) {
            r->noMemory();
            PROTO::colorManagement->m_imageDescriptions.pop_back();
            return;
        }

        RESOURCE->m_self   = RESOURCE;
        m_currentPreferred = RESOURCE;

        m_currentPreferred->m_settings = m_surface->getPreferredImageDescription();
        if (!PROTO::colorManagement->m_debug && m_currentPreferred->m_settings.icc.fd) {
            LOGM(ERR, "FIXME: parse icc profile");
            r->error(WP_COLOR_MANAGER_V1_ERROR_UNSUPPORTED_FEATURE, "ICC profiles are not supported");
            return;
        }

        RESOURCE->resource()->sendReady(m_currentPreferred->m_settings.updateId());
    });
}

bool CColorManagementFeedbackSurface::good() {
    return m_resource->resource();
}

wl_client* CColorManagementFeedbackSurface::client() {
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

bool CColorManagementIccCreator::good() {
    return m_resource->resource();
}

wl_client* CColorManagementIccCreator::client() {
    return m_client;
}

CColorManagementParametricCreator::CColorManagementParametricCreator(SP<CWpImageDescriptionCreatorParamsV1> resource) : m_resource(resource) {
    if UNLIKELY (!good())
        return;
    //
    m_client = m_resource->client();

    m_resource->setOnDestroy([this](CWpImageDescriptionCreatorParamsV1* r) { PROTO::colorManagement->destroyResource(this); });

    m_resource->setCreate([this](CWpImageDescriptionCreatorParamsV1* r, uint32_t id) {
        LOGM(TRACE, "Create image description from params for id {}", id);

        // FIXME actually check completeness
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

        // FIXME actually check support
        if (!m_valuesSet) {
            RESOURCE->resource()->sendFailed(WP_IMAGE_DESCRIPTION_V1_CAUSE_UNSUPPORTED, "unsupported");
            return;
        }

        RESOURCE->m_self     = RESOURCE;
        RESOURCE->m_settings = m_settings;
        RESOURCE->resource()->sendReady(m_settings.updateId());

        PROTO::colorManagement->destroyResource(this);
    });
    m_resource->setSetTfNamed([this](CWpImageDescriptionCreatorParamsV1* r, uint32_t tf) {
        LOGM(TRACE, "Set image description transfer function to {}", tf);
        if (m_valuesSet & PC_TF) {
            r->error(WP_IMAGE_DESCRIPTION_CREATOR_PARAMS_V1_ERROR_ALREADY_SET, "Transfer function already set");
            return;
        }

        switch (tf) {
            case WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_SRGB: break;
            case WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_ST2084_PQ: break;
            case WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_EXT_LINEAR: break;
            case WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_GAMMA22: break;
            case WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_GAMMA28: break;
            case WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_HLG: break;
            default: r->error(WP_IMAGE_DESCRIPTION_CREATOR_PARAMS_V1_ERROR_INVALID_TF, "Unsupported transfer function"); return;
        }

        m_settings.transferFunction = convertTransferFunction((wpColorManagerV1TransferFunction)tf);
        m_valuesSet |= PC_TF;
    });
    m_resource->setSetTfPower([this](CWpImageDescriptionCreatorParamsV1* r, uint32_t eexp) {
        LOGM(TRACE, "Set image description tf power to {}", eexp);
        if (m_valuesSet & PC_TF_POWER) {
            r->error(WP_IMAGE_DESCRIPTION_CREATOR_PARAMS_V1_ERROR_ALREADY_SET, "Transfer function power already set");
            return;
        }
        if (!PROTO::colorManagement->m_debug) {
            r->error(WP_COLOR_MANAGER_V1_ERROR_UNSUPPORTED_FEATURE, "TF power is not supported");
            return;
        }
        m_settings.transferFunctionPower = eexp / 10000.0f;
        if (m_settings.transferFunctionPower < 1.0 || m_settings.transferFunctionPower > 10.0) {
            r->error(WP_IMAGE_DESCRIPTION_CREATOR_PARAMS_V1_ERROR_INVALID_TF, "Power should be between 1.0 and 10.0");
            return;
        }
        m_valuesSet |= PC_TF_POWER;
    });
    m_resource->setSetPrimariesNamed([this](CWpImageDescriptionCreatorParamsV1* r, uint32_t primaries) {
        LOGM(TRACE, "Set image description primaries by name {}", primaries);
        if (m_valuesSet & PC_PRIMARIES) {
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
        m_settings.primariesNamed   = convertPrimaries((wpColorManagerV1Primaries)primaries);
        m_settings.primaries        = getPrimaries(m_settings.primariesNamed);
        m_valuesSet |= PC_PRIMARIES;
    });
    m_resource->setSetPrimaries(
        [this](CWpImageDescriptionCreatorParamsV1* r, int32_t r_x, int32_t r_y, int32_t g_x, int32_t g_y, int32_t b_x, int32_t b_y, int32_t w_x, int32_t w_y) {
            LOGM(TRACE, "Set image description primaries by values r:{},{} g:{},{} b:{},{} w:{},{}", r_x, r_y, g_x, g_y, b_x, b_y, w_x, w_y);
            if (m_valuesSet & PC_PRIMARIES) {
                r->error(WP_IMAGE_DESCRIPTION_CREATOR_PARAMS_V1_ERROR_ALREADY_SET, "Primaries already set");
                return;
            }
            if (!PROTO::colorManagement->m_debug) {
                r->error(WP_COLOR_MANAGER_V1_ERROR_UNSUPPORTED_FEATURE, "Custom primaries aren't supported");
                return;
            }
            m_settings.primariesNameSet = false;
            m_settings.primaries        = SPCPRimaries{.red   = {.x = r_x / 1000000.0f, .y = r_y / 1000000.0f},
                                                       .green = {.x = g_x / 1000000.0f, .y = g_y / 1000000.0f},
                                                       .blue  = {.x = b_x / 1000000.0f, .y = b_y / 1000000.0f},
                                                       .white = {.x = w_x / 1000000.0f, .y = w_y / 1000000.0f}};
            m_valuesSet |= PC_PRIMARIES;
        });
    m_resource->setSetLuminances([this](CWpImageDescriptionCreatorParamsV1* r, uint32_t min_lum, uint32_t max_lum, uint32_t reference_lum) {
        auto min = min_lum / 10000.0f;
        LOGM(TRACE, "Set image description luminances to {} - {} ({})", min, max_lum, reference_lum);
        if (m_valuesSet & PC_LUMINANCES) {
            r->error(WP_IMAGE_DESCRIPTION_CREATOR_PARAMS_V1_ERROR_ALREADY_SET, "Luminances already set");
            return;
        }
        if (max_lum <= min || reference_lum <= min) {
            r->error(WP_IMAGE_DESCRIPTION_CREATOR_PARAMS_V1_ERROR_INVALID_LUMINANCE, "Invalid luminances");
            return;
        }
        m_settings.luminances = SImageDescription::SPCLuminances{.min = min, .max = max_lum, .reference = reference_lum};
        m_valuesSet |= PC_LUMINANCES;
    });
    m_resource->setSetMasteringDisplayPrimaries(
        [this](CWpImageDescriptionCreatorParamsV1* r, int32_t r_x, int32_t r_y, int32_t g_x, int32_t g_y, int32_t b_x, int32_t b_y, int32_t w_x, int32_t w_y) {
            LOGM(TRACE, "Set image description mastering primaries by values r:{},{} g:{},{} b:{},{} w:{},{}", r_x, r_y, g_x, g_y, b_x, b_y, w_x, w_y);
            if (m_valuesSet & PC_MASTERING_PRIMARIES) {
                r->error(WP_IMAGE_DESCRIPTION_CREATOR_PARAMS_V1_ERROR_ALREADY_SET, "Mastering primaries already set");
                return;
            }
            if (!PROTO::colorManagement->m_debug) {
                r->error(WP_COLOR_MANAGER_V1_ERROR_UNSUPPORTED_FEATURE, "Mastering primaries are not supported");
                return;
            }
            m_settings.masteringPrimaries = SPCPRimaries{.red   = {.x = r_x / 1000000.0f, .y = r_y / 1000000.0f},
                                                         .green = {.x = g_x / 1000000.0f, .y = g_y / 1000000.0f},
                                                         .blue  = {.x = b_x / 1000000.0f, .y = b_y / 1000000.0f},
                                                         .white = {.x = w_x / 1000000.0f, .y = w_y / 1000000.0f}};
            m_valuesSet |= PC_MASTERING_PRIMARIES;

            // FIXME:
            // If a compositor additionally supports target color volume exceeding the primary color volume, it must advertise wp_color_manager_v1.feature.extended_target_volume.
            // If a client uses target color volume exceeding the primary color volume and the compositor does not support it, the result is implementation defined.
            // Compositors are recommended to detect this case and fail the image description gracefully, but it may as well result in color artifacts.
        });
    m_resource->setSetMasteringLuminance([this](CWpImageDescriptionCreatorParamsV1* r, uint32_t min_lum, uint32_t max_lum) {
        auto min = min_lum / 10000.0f;
        LOGM(TRACE, "Set image description mastering luminances to {} - {}", min, max_lum);
        // if (valuesSet & PC_MASTERING_LUMINANCES) {
        //     r->error(WP_IMAGE_DESCRIPTION_CREATOR_PARAMS_V1_ERROR_ALREADY_SET, "Mastering luminances already set");
        //     return;
        // }
        if (min > 0 && max_lum > 0 && max_lum <= min) {
            r->error(WP_IMAGE_DESCRIPTION_CREATOR_PARAMS_V1_ERROR_INVALID_LUMINANCE, "Invalid luminances");
            return;
        }
        if (!PROTO::colorManagement->m_debug) {
            r->error(WP_COLOR_MANAGER_V1_ERROR_UNSUPPORTED_FEATURE, "Mastering luminances are not supported");
            return;
        }
        m_settings.masteringLuminances = SImageDescription::SPCMasteringLuminances{.min = min, .max = max_lum};
        m_valuesSet |= PC_MASTERING_LUMINANCES;
    });
    m_resource->setSetMaxCll([this](CWpImageDescriptionCreatorParamsV1* r, uint32_t max_cll) {
        LOGM(TRACE, "Set image description max content light level to {}", max_cll);
        // if (valuesSet & PC_CLL) {
        //     r->error(WP_IMAGE_DESCRIPTION_CREATOR_PARAMS_V1_ERROR_ALREADY_SET, "Max CLL already set");
        //     return;
        // }
        m_settings.maxCLL = max_cll;
        m_valuesSet |= PC_CLL;
    });
    m_resource->setSetMaxFall([this](CWpImageDescriptionCreatorParamsV1* r, uint32_t max_fall) {
        LOGM(TRACE, "Set image description max frame-average light level to {}", max_fall);
        // if (valuesSet & PC_FALL) {
        //     r->error(WP_IMAGE_DESCRIPTION_CREATOR_PARAMS_V1_ERROR_ALREADY_SET, "Max FALL already set");
        //     return;
        // }
        m_settings.maxFALL = max_fall;
        m_valuesSet |= PC_FALL;
    });
}

bool CColorManagementParametricCreator::good() {
    return m_resource->resource();
}

wl_client* CColorManagementParametricCreator::client() {
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

        auto RESOURCE = makeShared<CColorManagementImageDescriptionInfo>(makeShared<CWpImageDescriptionInfoV1>(r->client(), r->version(), id), m_settings);

        if UNLIKELY (!RESOURCE->good())
            r->noMemory();

        // CColorManagementImageDescriptionInfo should send everything in the constructor and be ready for destroying at this point
        RESOURCE.reset();
    });
}

bool CColorManagementImageDescription::good() {
    return m_resource->resource();
}

wl_client* CColorManagementImageDescription::client() {
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

    const auto toProto = [](float value) { return int32_t(std::round(value * 10000)); };

    if (m_settings.icc.fd >= 0)
        m_resource->sendIccFile(m_settings.icc.fd, m_settings.icc.length);

    // send preferred client paramateres
    m_resource->sendPrimaries(toProto(m_settings.primaries.red.x), toProto(m_settings.primaries.red.y), toProto(m_settings.primaries.green.x),
                              toProto(m_settings.primaries.green.y), toProto(m_settings.primaries.blue.x), toProto(m_settings.primaries.blue.y),
                              toProto(m_settings.primaries.white.x), toProto(m_settings.primaries.white.y));
    if (m_settings.primariesNameSet)
        m_resource->sendPrimariesNamed(m_settings.primariesNamed);
    m_resource->sendTfPower(std::round(m_settings.transferFunctionPower * 10000));
    m_resource->sendTfNamed(m_settings.transferFunction);
    m_resource->sendLuminances(std::round(m_settings.luminances.min * 10000), m_settings.luminances.max, m_settings.luminances.reference);

    // send expexted display paramateres
    m_resource->sendTargetPrimaries(toProto(m_settings.masteringPrimaries.red.x), toProto(m_settings.masteringPrimaries.red.y), toProto(m_settings.masteringPrimaries.green.x),
                                    toProto(m_settings.masteringPrimaries.green.y), toProto(m_settings.masteringPrimaries.blue.x), toProto(m_settings.masteringPrimaries.blue.y),
                                    toProto(m_settings.masteringPrimaries.white.x), toProto(m_settings.masteringPrimaries.white.y));
    m_resource->sendTargetLuminance(std::round(m_settings.masteringLuminances.min * 10000), m_settings.masteringLuminances.max);
    m_resource->sendTargetMaxCll(m_settings.maxCLL);
    m_resource->sendTargetMaxFall(m_settings.maxFALL);

    m_resource->sendDone();
}

bool CColorManagementImageDescriptionInfo::good() {
    return m_resource->resource();
}

wl_client* CColorManagementImageDescriptionInfo::client() {
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
        if (output->m_monitor == monitor)
            output->m_resource->sendImageDescriptionChanged();
    }
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
