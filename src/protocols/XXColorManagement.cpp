#include "XXColorManagement.hpp"
#include "../Compositor.hpp"
#include "ColorManagement.hpp"
#include "color-management-v1.hpp"
#include "types/ColorManagement.hpp"
#include "xx-color-management-v4.hpp"

using namespace NColorManagement;

static wpColorManagerV1TransferFunction getWPTransferFunction(xxColorManagerV4TransferFunction tf) {
    switch (tf) {
        case XX_COLOR_MANAGER_V4_TRANSFER_FUNCTION_BT709:
        case XX_COLOR_MANAGER_V4_TRANSFER_FUNCTION_BT1361: return WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_BT1886;
        case XX_COLOR_MANAGER_V4_TRANSFER_FUNCTION_GAMMA22: return WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_GAMMA22;
        case XX_COLOR_MANAGER_V4_TRANSFER_FUNCTION_GAMMA28: return WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_GAMMA28;
        case XX_COLOR_MANAGER_V4_TRANSFER_FUNCTION_ST240: return WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_ST240;
        case XX_COLOR_MANAGER_V4_TRANSFER_FUNCTION_LINEAR: return WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_EXT_LINEAR;
        case XX_COLOR_MANAGER_V4_TRANSFER_FUNCTION_LOG_100: return WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_LOG_100;
        case XX_COLOR_MANAGER_V4_TRANSFER_FUNCTION_LOG_316: return WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_LOG_316;
        case XX_COLOR_MANAGER_V4_TRANSFER_FUNCTION_XVYCC: return WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_XVYCC;
        case XX_COLOR_MANAGER_V4_TRANSFER_FUNCTION_SRGB: return WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_SRGB;
        case XX_COLOR_MANAGER_V4_TRANSFER_FUNCTION_EXT_SRGB: return WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_EXT_SRGB;
        case XX_COLOR_MANAGER_V4_TRANSFER_FUNCTION_ST2084_PQ: return WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_ST2084_PQ;
        case XX_COLOR_MANAGER_V4_TRANSFER_FUNCTION_ST428: return WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_ST428;
        case XX_COLOR_MANAGER_V4_TRANSFER_FUNCTION_HLG: return WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_HLG;
        default: UNREACHABLE();
    }
}

static wpColorManagerV1Primaries getWPPrimaries(xxColorManagerV4Primaries primaries) {
    return static_cast<wpColorManagerV1Primaries>(primaries + 1);
}

CXXColorManager::CXXColorManager(SP<CXxColorManagerV4> resource_) : m_resource(resource_) {
    if UNLIKELY (!good())
        return;

    m_resource->sendSupportedFeature(XX_COLOR_MANAGER_V4_FEATURE_PARAMETRIC);
    m_resource->sendSupportedFeature(XX_COLOR_MANAGER_V4_FEATURE_EXTENDED_TARGET_VOLUME);
    m_resource->sendSupportedFeature(XX_COLOR_MANAGER_V4_FEATURE_SET_MASTERING_DISPLAY_PRIMARIES);
    m_resource->sendSupportedFeature(XX_COLOR_MANAGER_V4_FEATURE_SET_PRIMARIES);
    m_resource->sendSupportedFeature(XX_COLOR_MANAGER_V4_FEATURE_SET_LUMINANCES);

    m_resource->sendSupportedPrimariesNamed(XX_COLOR_MANAGER_V4_PRIMARIES_SRGB);
    m_resource->sendSupportedPrimariesNamed(XX_COLOR_MANAGER_V4_PRIMARIES_PAL_M);
    m_resource->sendSupportedPrimariesNamed(XX_COLOR_MANAGER_V4_PRIMARIES_PAL);
    m_resource->sendSupportedPrimariesNamed(XX_COLOR_MANAGER_V4_PRIMARIES_NTSC);
    m_resource->sendSupportedPrimariesNamed(XX_COLOR_MANAGER_V4_PRIMARIES_GENERIC_FILM);
    m_resource->sendSupportedPrimariesNamed(XX_COLOR_MANAGER_V4_PRIMARIES_BT2020);
    // resource->sendSupportedPrimariesNamed(XX_COLOR_MANAGER_V4_PRIMARIES_CIE1931_XYZ);
    m_resource->sendSupportedPrimariesNamed(XX_COLOR_MANAGER_V4_PRIMARIES_DCI_P3);
    m_resource->sendSupportedPrimariesNamed(XX_COLOR_MANAGER_V4_PRIMARIES_DISPLAY_P3);
    m_resource->sendSupportedPrimariesNamed(XX_COLOR_MANAGER_V4_PRIMARIES_ADOBE_RGB);

    m_resource->sendSupportedTfNamed(XX_COLOR_MANAGER_V4_TRANSFER_FUNCTION_GAMMA22);
    m_resource->sendSupportedTfNamed(XX_COLOR_MANAGER_V4_TRANSFER_FUNCTION_GAMMA28);
    m_resource->sendSupportedTfNamed(XX_COLOR_MANAGER_V4_TRANSFER_FUNCTION_HLG);
    m_resource->sendSupportedTfNamed(XX_COLOR_MANAGER_V4_TRANSFER_FUNCTION_SRGB);
    m_resource->sendSupportedTfNamed(XX_COLOR_MANAGER_V4_TRANSFER_FUNCTION_ST2084_PQ);
    m_resource->sendSupportedTfNamed(XX_COLOR_MANAGER_V4_TRANSFER_FUNCTION_LINEAR);
    m_resource->sendSupportedTfNamed(XX_COLOR_MANAGER_V4_TRANSFER_FUNCTION_BT709);
    m_resource->sendSupportedTfNamed(XX_COLOR_MANAGER_V4_TRANSFER_FUNCTION_BT1361);
    m_resource->sendSupportedTfNamed(XX_COLOR_MANAGER_V4_TRANSFER_FUNCTION_ST240);
    m_resource->sendSupportedTfNamed(XX_COLOR_MANAGER_V4_TRANSFER_FUNCTION_LOG_100);
    m_resource->sendSupportedTfNamed(XX_COLOR_MANAGER_V4_TRANSFER_FUNCTION_LOG_316);
    m_resource->sendSupportedTfNamed(XX_COLOR_MANAGER_V4_TRANSFER_FUNCTION_XVYCC);
    m_resource->sendSupportedTfNamed(XX_COLOR_MANAGER_V4_TRANSFER_FUNCTION_EXT_SRGB);
    m_resource->sendSupportedTfNamed(XX_COLOR_MANAGER_V4_TRANSFER_FUNCTION_ST428);

    m_resource->sendSupportedIntent(XX_COLOR_MANAGER_V4_RENDER_INTENT_PERCEPTUAL);
    // resource->sendSupportedIntent(XX_COLOR_MANAGER_V4_RENDER_INTENT_RELATIVE);
    // resource->sendSupportedIntent(XX_COLOR_MANAGER_V4_RENDER_INTENT_ABSOLUTE);
    // resource->sendSupportedIntent(XX_COLOR_MANAGER_V4_RENDER_INTENT_RELATIVE_BPC);

    m_resource->setDestroy([](CXxColorManagerV4* r) { LOGM(TRACE, "Destroy xx_color_manager at {:x} (generated default)", (uintptr_t)r); });
    m_resource->setGetOutput([](CXxColorManagerV4* r, uint32_t id, wl_resource* output) {
        LOGM(TRACE, "Get output for id={}, output={}", id, (uintptr_t)output);
        const auto RESOURCE =
            PROTO::xxColorManagement->m_outputs.emplace_back(makeShared<CXXColorManagementOutput>(makeShared<CXxColorManagementOutputV4>(r->client(), r->version(), id)));

        if UNLIKELY (!RESOURCE->good()) {
            r->noMemory();
            PROTO::xxColorManagement->m_outputs.pop_back();
            return;
        }

        RESOURCE->m_self = RESOURCE;
    });
    m_resource->setGetSurface([](CXxColorManagerV4* r, uint32_t id, wl_resource* surface) {
        LOGM(TRACE, "Get surface for id={}, surface={}", id, (uintptr_t)surface);
        auto SURF = CWLSurfaceResource::fromResource(surface);

        if (!SURF) {
            LOGM(ERR, "No surface for resource {}", (uintptr_t)surface);
            r->error(-1, "Invalid surface (2)");
            return;
        }

        if (SURF->m_colorManagement) {
            r->error(XX_COLOR_MANAGER_V4_ERROR_SURFACE_EXISTS, "CM Surface already exists");
            return;
        }

        const auto RESOURCE =
            PROTO::xxColorManagement->m_surfaces.emplace_back(makeShared<CXXColorManagementSurface>(makeShared<CXxColorManagementSurfaceV4>(r->client(), r->version(), id), SURF));
        if UNLIKELY (!RESOURCE->good()) {
            r->noMemory();
            PROTO::xxColorManagement->m_surfaces.pop_back();
            return;
        }

        RESOURCE->m_self = RESOURCE;
    });
    m_resource->setGetFeedbackSurface([](CXxColorManagerV4* r, uint32_t id, wl_resource* surface) {
        LOGM(TRACE, "Get feedback surface for id={}, surface={}", id, (uintptr_t)surface);
        auto SURF = CWLSurfaceResource::fromResource(surface);

        if (!SURF) {
            LOGM(ERR, "No surface for resource {}", (uintptr_t)surface);
            r->error(-1, "Invalid surface (2)");
            return;
        }

        const auto RESOURCE = PROTO::xxColorManagement->m_feedbackSurfaces.emplace_back(
            makeShared<CXXColorManagementFeedbackSurface>(makeShared<CXxColorManagementFeedbackSurfaceV4>(r->client(), r->version(), id), SURF));

        if UNLIKELY (!RESOURCE->good()) {
            r->noMemory();
            PROTO::xxColorManagement->m_feedbackSurfaces.pop_back();
            return;
        }

        RESOURCE->m_self = RESOURCE;
    });
    m_resource->setNewIccCreator([](CXxColorManagerV4* r, uint32_t id) {
        LOGM(WARN, "New ICC creator for id={} (unsupported)", id);
        r->error(XX_COLOR_MANAGER_V4_ERROR_UNSUPPORTED_FEATURE, "ICC profiles are not supported");
    });
    m_resource->setNewParametricCreator([](CXxColorManagerV4* r, uint32_t id) {
        LOGM(TRACE, "New parametric creator for id={}", id);

        const auto RESOURCE = PROTO::xxColorManagement->m_parametricCreators.emplace_back(
            makeShared<CXXColorManagementParametricCreator>(makeShared<CXxImageDescriptionCreatorParamsV4>(r->client(), r->version(), id)));

        if UNLIKELY (!RESOURCE->good()) {
            r->noMemory();
            PROTO::xxColorManagement->m_parametricCreators.pop_back();
            return;
        }

        RESOURCE->m_self = RESOURCE;
    });

    m_resource->setOnDestroy([this](CXxColorManagerV4* r) { PROTO::xxColorManagement->destroyResource(this); });
}

bool CXXColorManager::good() {
    return m_resource->resource();
}

CXXColorManagementOutput::CXXColorManagementOutput(SP<CXxColorManagementOutputV4> resource_) : m_resource(resource_) {
    if UNLIKELY (!good())
        return;

    m_client = m_resource->client();

    m_resource->setDestroy([this](CXxColorManagementOutputV4* r) { PROTO::xxColorManagement->destroyResource(this); });
    m_resource->setOnDestroy([this](CXxColorManagementOutputV4* r) { PROTO::xxColorManagement->destroyResource(this); });

    m_resource->setGetImageDescription([this](CXxColorManagementOutputV4* r, uint32_t id) {
        LOGM(TRACE, "Get image description for output={}, id={}", (uintptr_t)r, id);
        if (m_imageDescription.valid())
            PROTO::xxColorManagement->destroyResource(m_imageDescription.get());

        const auto RESOURCE = PROTO::xxColorManagement->m_imageDescriptions.emplace_back(
            makeShared<CXXColorManagementImageDescription>(makeShared<CXxImageDescriptionV4>(r->client(), r->version(), id), true));

        if UNLIKELY (!RESOURCE->good()) {
            r->noMemory();
            PROTO::xxColorManagement->m_imageDescriptions.pop_back();
            return;
        }

        RESOURCE->m_self = RESOURCE;
    });
}

bool CXXColorManagementOutput::good() {
    return m_resource->resource();
}

wl_client* CXXColorManagementOutput::client() {
    return m_client;
}

CXXColorManagementSurface::CXXColorManagementSurface(SP<CWLSurfaceResource> surface_) : m_surface(surface_) {
    // only for frog cm until wayland cm is adopted
}

CXXColorManagementSurface::CXXColorManagementSurface(SP<CXxColorManagementSurfaceV4> resource_, SP<CWLSurfaceResource> surface_) : m_surface(surface_), m_resource(resource_) {
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

        m_resource->setOnDestroy([this](CXxColorManagementSurfaceV4* r) {
            LOGM(TRACE, "Destroy wp cm and xx cm for surface {}", (uintptr_t)m_surface);
            if (m_surface.valid())
                PROTO::colorManagement->destroyResource(m_surface->m_colorManagement.get());
            PROTO::xxColorManagement->destroyResource(this);
        });
    } else
        m_resource->setOnDestroy([this](CXxColorManagementSurfaceV4* r) {
            LOGM(TRACE, "Destroy xx cm surface {}", (uintptr_t)m_surface);
            PROTO::xxColorManagement->destroyResource(this);
        });

    m_resource->setDestroy([this](CXxColorManagementSurfaceV4* r) {
        LOGM(TRACE, "Destroy xx cm surface {}", (uintptr_t)m_surface);
        PROTO::xxColorManagement->destroyResource(this);
    });

    m_resource->setSetImageDescription([this](CXxColorManagementSurfaceV4* r, wl_resource* image_description, uint32_t render_intent) {
        LOGM(TRACE, "Set image description for surface={}, desc={}, intent={}", (uintptr_t)r, (uintptr_t)image_description, render_intent);

        const auto PO = static_cast<CXxImageDescriptionV4*>(wl_resource_get_user_data(image_description));
        if (!PO) { // FIXME check validity
            r->error(XX_COLOR_MANAGEMENT_SURFACE_V4_ERROR_IMAGE_DESCRIPTION, "Image description creation failed");
            return;
        }
        if (render_intent != XX_COLOR_MANAGER_V4_RENDER_INTENT_PERCEPTUAL) {
            r->error(XX_COLOR_MANAGEMENT_SURFACE_V4_ERROR_RENDER_INTENT, "Unsupported render intent");
            return;
        }

        const auto imageDescription =
            std::ranges::find_if(PROTO::xxColorManagement->m_imageDescriptions, [&](const auto& other) { return other->resource()->resource() == image_description; });
        if (imageDescription == PROTO::xxColorManagement->m_imageDescriptions.end()) {
            r->error(XX_COLOR_MANAGEMENT_SURFACE_V4_ERROR_IMAGE_DESCRIPTION, "Image description not found");
            return;
        }

        if (m_surface.valid()) {
            m_surface->m_colorManagement->m_imageDescription = imageDescription->get()->m_settings;
            m_surface->m_colorManagement->setHasImageDescription(true);
        } else
            LOGM(ERR, "Set image description for invalid surface");
    });
    m_resource->setUnsetImageDescription([this](CXxColorManagementSurfaceV4* r) {
        LOGM(TRACE, "Unset image description for surface={}", (uintptr_t)r);
        if (m_surface.valid()) {
            m_surface->m_colorManagement->m_imageDescription = SImageDescription{};
            m_surface->m_colorManagement->setHasImageDescription(false);
        } else
            LOGM(ERR, "Unset image description for invalid surface");
    });
}

bool CXXColorManagementSurface::good() {
    return m_resource && m_resource->resource();
}

wl_client* CXXColorManagementSurface::client() {
    return m_client;
}

const SImageDescription& CXXColorManagementSurface::imageDescription() {
    if (!hasImageDescription())
        LOGM(WARN, "Reading imageDescription while none set. Returns default or empty values");
    return m_imageDescription;
}

bool CXXColorManagementSurface::hasImageDescription() {
    return m_hasImageDescription;
}

void CXXColorManagementSurface::setHasImageDescription(bool has) {
    m_hasImageDescription = has;
    m_needsNewMetadata    = true;
}

const hdr_output_metadata& CXXColorManagementSurface::hdrMetadata() {
    return m_hdrMetadata;
}

void CXXColorManagementSurface::setHDRMetadata(const hdr_output_metadata& metadata) {
    m_hdrMetadata      = metadata;
    m_needsNewMetadata = false;
}

bool CXXColorManagementSurface::needsHdrMetadataUpdate() {
    return m_needsNewMetadata;
}

CXXColorManagementFeedbackSurface::CXXColorManagementFeedbackSurface(SP<CXxColorManagementFeedbackSurfaceV4> resource_, SP<CWLSurfaceResource> surface_) :
    m_surface(surface_), m_resource(resource_) {
    if UNLIKELY (!good())
        return;

    m_client = m_resource->client();

    m_resource->setDestroy([this](CXxColorManagementFeedbackSurfaceV4* r) {
        LOGM(TRACE, "Destroy xx cm feedback surface {}", (uintptr_t)m_surface);
        if (m_currentPreferred.valid())
            PROTO::xxColorManagement->destroyResource(m_currentPreferred.get());
        PROTO::xxColorManagement->destroyResource(this);
    });
    m_resource->setOnDestroy([this](CXxColorManagementFeedbackSurfaceV4* r) {
        LOGM(TRACE, "Destroy xx cm feedback surface {}", (uintptr_t)m_surface);
        if (m_currentPreferred.valid())
            PROTO::xxColorManagement->destroyResource(m_currentPreferred.get());
        PROTO::xxColorManagement->destroyResource(this);
    });

    m_resource->setGetPreferred([this](CXxColorManagementFeedbackSurfaceV4* r, uint32_t id) {
        LOGM(TRACE, "Get preferred for id {}", id);

        if (m_currentPreferred.valid())
            PROTO::xxColorManagement->destroyResource(m_currentPreferred.get());

        const auto RESOURCE = PROTO::xxColorManagement->m_imageDescriptions.emplace_back(
            makeShared<CXXColorManagementImageDescription>(makeShared<CXxImageDescriptionV4>(r->client(), r->version(), id), true));

        if UNLIKELY (!RESOURCE->good()) {
            r->noMemory();
            PROTO::xxColorManagement->m_imageDescriptions.pop_back();
            return;
        }

        RESOURCE->m_self   = RESOURCE;
        m_currentPreferred = RESOURCE;

        m_currentPreferred->m_settings = g_pCompositor->getPreferredImageDescription();

        RESOURCE->resource()->sendReady(id);
    });
}

bool CXXColorManagementFeedbackSurface::good() {
    return m_resource->resource();
}

wl_client* CXXColorManagementFeedbackSurface::client() {
    return m_client;
}

CXXColorManagementParametricCreator::CXXColorManagementParametricCreator(SP<CXxImageDescriptionCreatorParamsV4> resource_) : m_resource(resource_) {
    if UNLIKELY (!good())
        return;
    //
    m_client = m_resource->client();

    m_resource->setOnDestroy([this](CXxImageDescriptionCreatorParamsV4* r) { PROTO::xxColorManagement->destroyResource(this); });

    m_resource->setCreate([this](CXxImageDescriptionCreatorParamsV4* r, uint32_t id) {
        LOGM(TRACE, "Create image description from params for id {}", id);

        // FIXME actually check completeness
        if (!m_valuesSet) {
            r->error(XX_IMAGE_DESCRIPTION_CREATOR_PARAMS_V4_ERROR_INCOMPLETE_SET, "Missing required settings");
            return;
        }

        // FIXME actually check consistency
        if (!m_valuesSet) {
            r->error(XX_IMAGE_DESCRIPTION_CREATOR_PARAMS_V4_ERROR_INCONSISTENT_SET, "Set is not consistent");
            return;
        }

        const auto RESOURCE = PROTO::xxColorManagement->m_imageDescriptions.emplace_back(
            makeShared<CXXColorManagementImageDescription>(makeShared<CXxImageDescriptionV4>(r->client(), r->version(), id), false));

        if UNLIKELY (!RESOURCE->good()) {
            r->noMemory();
            PROTO::xxColorManagement->m_imageDescriptions.pop_back();
            return;
        }

        // FIXME actually check support
        if (!m_valuesSet) {
            RESOURCE->resource()->sendFailed(XX_IMAGE_DESCRIPTION_V4_CAUSE_UNSUPPORTED, "unsupported");
            return;
        }

        RESOURCE->m_self     = RESOURCE;
        RESOURCE->m_settings = m_settings;
        RESOURCE->resource()->sendReady(id);

        PROTO::xxColorManagement->destroyResource(this);
    });
    m_resource->setSetTfNamed([this](CXxImageDescriptionCreatorParamsV4* r, uint32_t tf) {
        LOGM(TRACE, "Set image description transfer function to {}", tf);
        if (m_valuesSet & PC_TF) {
            r->error(XX_IMAGE_DESCRIPTION_CREATOR_PARAMS_V4_ERROR_ALREADY_SET, "Transfer function already set");
            return;
        }

        switch (tf) {
            case XX_COLOR_MANAGER_V4_TRANSFER_FUNCTION_GAMMA22:
            case XX_COLOR_MANAGER_V4_TRANSFER_FUNCTION_GAMMA28:
            case XX_COLOR_MANAGER_V4_TRANSFER_FUNCTION_HLG:
            case XX_COLOR_MANAGER_V4_TRANSFER_FUNCTION_SRGB:
            case XX_COLOR_MANAGER_V4_TRANSFER_FUNCTION_ST2084_PQ:
            case XX_COLOR_MANAGER_V4_TRANSFER_FUNCTION_LINEAR:
            case XX_COLOR_MANAGER_V4_TRANSFER_FUNCTION_BT709:
            case XX_COLOR_MANAGER_V4_TRANSFER_FUNCTION_BT1361:
            case XX_COLOR_MANAGER_V4_TRANSFER_FUNCTION_ST240:
            case XX_COLOR_MANAGER_V4_TRANSFER_FUNCTION_LOG_100:
            case XX_COLOR_MANAGER_V4_TRANSFER_FUNCTION_LOG_316:
            case XX_COLOR_MANAGER_V4_TRANSFER_FUNCTION_XVYCC:
            case XX_COLOR_MANAGER_V4_TRANSFER_FUNCTION_EXT_SRGB:
            case XX_COLOR_MANAGER_V4_TRANSFER_FUNCTION_ST428: break;
            default: r->error(XX_IMAGE_DESCRIPTION_CREATOR_PARAMS_V4_ERROR_INVALID_TF, "Unsupported transfer function"); return;
        }

        m_settings.transferFunction = convertTransferFunction(getWPTransferFunction(static_cast<xxColorManagerV4TransferFunction>(tf)));
        m_valuesSet |= PC_TF;
    });
    m_resource->setSetTfPower([this](CXxImageDescriptionCreatorParamsV4* r, uint32_t eexp) {
        LOGM(TRACE, "Set image description tf power to {}", eexp);
        if (m_valuesSet & PC_TF_POWER) {
            r->error(XX_IMAGE_DESCRIPTION_CREATOR_PARAMS_V4_ERROR_ALREADY_SET, "Transfer function power already set");
            return;
        }
        m_settings.transferFunctionPower = eexp / 10000.0f;
        m_valuesSet |= PC_TF_POWER;
    });
    m_resource->setSetPrimariesNamed([this](CXxImageDescriptionCreatorParamsV4* r, uint32_t primaries) {
        LOGM(TRACE, "Set image description primaries by name {}", primaries);
        if (m_valuesSet & PC_PRIMARIES) {
            r->error(XX_IMAGE_DESCRIPTION_CREATOR_PARAMS_V4_ERROR_ALREADY_SET, "Primaries already set");
            return;
        }

        switch (primaries) {
            case XX_COLOR_MANAGER_V4_PRIMARIES_SRGB:
            case XX_COLOR_MANAGER_V4_PRIMARIES_PAL_M:
            case XX_COLOR_MANAGER_V4_PRIMARIES_PAL:
            case XX_COLOR_MANAGER_V4_PRIMARIES_NTSC:
            case XX_COLOR_MANAGER_V4_PRIMARIES_GENERIC_FILM:
            case XX_COLOR_MANAGER_V4_PRIMARIES_BT2020:
            case XX_COLOR_MANAGER_V4_PRIMARIES_DCI_P3:
            case XX_COLOR_MANAGER_V4_PRIMARIES_DISPLAY_P3:
            case XX_COLOR_MANAGER_V4_PRIMARIES_ADOBE_RGB:
                m_settings.primariesNameSet = true;
                m_settings.primariesNamed   = convertPrimaries(getWPPrimaries(static_cast<xxColorManagerV4Primaries>(primaries)));
                m_settings.primaries        = getPrimaries(m_settings.primariesNamed);
                m_valuesSet |= PC_PRIMARIES;
                break;
            default: r->error(XX_IMAGE_DESCRIPTION_CREATOR_PARAMS_V4_ERROR_INVALID_PRIMARIES, "Unsupported primaries");
        }
    });
    m_resource->setSetPrimaries(
        [this](CXxImageDescriptionCreatorParamsV4* r, int32_t r_x, int32_t r_y, int32_t g_x, int32_t g_y, int32_t b_x, int32_t b_y, int32_t w_x, int32_t w_y) {
            LOGM(TRACE, "Set image description primaries by values r:{},{} g:{},{} b:{},{} w:{},{}", r_x, r_y, g_x, g_y, b_x, b_y, w_x, w_y);
            if (m_valuesSet & PC_PRIMARIES) {
                r->error(XX_IMAGE_DESCRIPTION_CREATOR_PARAMS_V4_ERROR_ALREADY_SET, "Primaries already set");
                return;
            }
            m_settings.primariesNameSet = false;
            m_settings.primaries        = SPCPRimaries{.red = {.x = r_x, .y = r_y}, .green = {.x = g_x, .y = g_y}, .blue = {.x = b_x, .y = b_y}, .white = {.x = w_x, .y = w_y}};
            m_valuesSet |= PC_PRIMARIES;
        });
    m_resource->setSetLuminances([this](CXxImageDescriptionCreatorParamsV4* r, uint32_t min_lum, uint32_t max_lum, uint32_t reference_lum) {
        auto min = min_lum / 10000.0f;
        LOGM(TRACE, "Set image description luminances to {} - {} ({})", min, max_lum, reference_lum);
        if (m_valuesSet & PC_LUMINANCES) {
            r->error(XX_IMAGE_DESCRIPTION_CREATOR_PARAMS_V4_ERROR_ALREADY_SET, "Luminances already set");
            return;
        }
        if (max_lum < reference_lum || reference_lum <= min) {
            r->error(XX_IMAGE_DESCRIPTION_CREATOR_PARAMS_V4_ERROR_INVALID_LUMINANCE, "Invalid luminances");
            return;
        }
        m_settings.luminances = SImageDescription::SPCLuminances{.min = min, .max = max_lum, .reference = reference_lum};
        m_valuesSet |= PC_LUMINANCES;
    });
    m_resource->setSetMasteringDisplayPrimaries(
        [this](CXxImageDescriptionCreatorParamsV4* r, int32_t r_x, int32_t r_y, int32_t g_x, int32_t g_y, int32_t b_x, int32_t b_y, int32_t w_x, int32_t w_y) {
            LOGM(TRACE, "Set image description mastering primaries by values r:{},{} g:{},{} b:{},{} w:{},{}", r_x, r_y, g_x, g_y, b_x, b_y, w_x, w_y);
            // if (valuesSet & PC_MASTERING_PRIMARIES) {
            //     r->error(XX_IMAGE_DESCRIPTION_CREATOR_PARAMS_V4_ERROR_ALREADY_SET, "Mastering primaries already set");
            //     return;
            // }
            m_settings.masteringPrimaries = SPCPRimaries{.red = {.x = r_x, .y = r_y}, .green = {.x = g_x, .y = g_y}, .blue = {.x = b_x, .y = b_y}, .white = {.x = w_x, .y = w_y}};
            m_valuesSet |= PC_MASTERING_PRIMARIES;
        });
    m_resource->setSetMasteringLuminance([this](CXxImageDescriptionCreatorParamsV4* r, uint32_t min_lum, uint32_t max_lum) {
        auto min = min_lum / 10000.0f;
        LOGM(TRACE, "Set image description mastering luminances to {} - {}", min, max_lum);
        // if (valuesSet & PC_MASTERING_LUMINANCES) {
        //     r->error(XX_IMAGE_DESCRIPTION_CREATOR_PARAMS_V4_ERROR_ALREADY_SET, "Mastering luminances already set");
        //     return;
        // }
        if (min > 0 && max_lum > 0 && max_lum <= min) {
            r->error(XX_IMAGE_DESCRIPTION_CREATOR_PARAMS_V4_ERROR_INVALID_LUMINANCE, "Invalid luminances");
            return;
        }
        m_settings.masteringLuminances = SImageDescription::SPCMasteringLuminances{.min = min, .max = max_lum};
        m_valuesSet |= PC_MASTERING_LUMINANCES;
    });
    m_resource->setSetMaxCll([this](CXxImageDescriptionCreatorParamsV4* r, uint32_t max_cll) {
        LOGM(TRACE, "Set image description max content light level to {}", max_cll);
        // if (valuesSet & PC_CLL) {
        //     r->error(XX_IMAGE_DESCRIPTION_CREATOR_PARAMS_V4_ERROR_ALREADY_SET, "Max CLL already set");
        //     return;
        // }
        m_settings.maxCLL = max_cll;
        m_valuesSet |= PC_CLL;
    });
    m_resource->setSetMaxFall([this](CXxImageDescriptionCreatorParamsV4* r, uint32_t max_fall) {
        LOGM(TRACE, "Set image description max frame-average light level to {}", max_fall);
        // if (valuesSet & PC_FALL) {
        //     r->error(XX_IMAGE_DESCRIPTION_CREATOR_PARAMS_V4_ERROR_ALREADY_SET, "Max FALL already set");
        //     return;
        // }
        m_settings.maxFALL = max_fall;
        m_valuesSet |= PC_FALL;
    });
}

bool CXXColorManagementParametricCreator::good() {
    return m_resource->resource();
}

wl_client* CXXColorManagementParametricCreator::client() {
    return m_client;
}

CXXColorManagementImageDescription::CXXColorManagementImageDescription(SP<CXxImageDescriptionV4> resource_, bool allowGetInformation) :
    m_resource(resource_), m_allowGetInformation(allowGetInformation) {
    if UNLIKELY (!good())
        return;

    m_client = m_resource->client();

    m_resource->setDestroy([this](CXxImageDescriptionV4* r) { PROTO::xxColorManagement->destroyResource(this); });
    m_resource->setOnDestroy([this](CXxImageDescriptionV4* r) { PROTO::xxColorManagement->destroyResource(this); });

    m_resource->setGetInformation([this](CXxImageDescriptionV4* r, uint32_t id) {
        LOGM(TRACE, "Get image information for image={}, id={}", (uintptr_t)r, id);
        if (!m_allowGetInformation) {
            r->error(XX_IMAGE_DESCRIPTION_V4_ERROR_NO_INFORMATION, "Image descriptions doesn't allow get_information request");
            return;
        }

        auto RESOURCE = makeShared<CXXColorManagementImageDescriptionInfo>(makeShared<CXxImageDescriptionInfoV4>(r->client(), r->version(), id), m_settings);

        if UNLIKELY (!RESOURCE->good())
            r->noMemory();

        // CXXColorManagementImageDescriptionInfo should send everything in the constructor and be ready for destroying at this point
        RESOURCE.reset();
    });
}

bool CXXColorManagementImageDescription::good() {
    return m_resource->resource();
}

wl_client* CXXColorManagementImageDescription::client() {
    return m_client;
}

SP<CXxImageDescriptionV4> CXXColorManagementImageDescription::resource() {
    return m_resource;
}

CXXColorManagementImageDescriptionInfo::CXXColorManagementImageDescriptionInfo(SP<CXxImageDescriptionInfoV4> resource_, const SImageDescription& settings_) :
    m_resource(resource_), m_settings(settings_) {
    if UNLIKELY (!good())
        return;

    m_client = m_resource->client();

    const auto toProto = [](float value) { return static_cast<int32_t>(std::round(value * 10000)); };

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

    // send expected display paramateres
    m_resource->sendTargetPrimaries(toProto(m_settings.masteringPrimaries.red.x), toProto(m_settings.masteringPrimaries.red.y), toProto(m_settings.masteringPrimaries.green.x),
                                    toProto(m_settings.masteringPrimaries.green.y), toProto(m_settings.masteringPrimaries.blue.x), toProto(m_settings.masteringPrimaries.blue.y),
                                    toProto(m_settings.masteringPrimaries.white.x), toProto(m_settings.masteringPrimaries.white.y));
    m_resource->sendTargetLuminance(std::round(m_settings.masteringLuminances.min * 10000), m_settings.masteringLuminances.max);
    m_resource->sendTargetMaxCll(m_settings.maxCLL);
    m_resource->sendTargetMaxFall(m_settings.maxFALL);

    m_resource->sendDone();
}

bool CXXColorManagementImageDescriptionInfo::good() {
    return m_resource->resource();
}

wl_client* CXXColorManagementImageDescriptionInfo::client() {
    return m_client;
}

CXXColorManagementProtocol::CXXColorManagementProtocol(const wl_interface* iface, const int& ver, const std::string& name) : IWaylandProtocol(iface, ver, name) {
    ;
}

void CXXColorManagementProtocol::bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id) {
    const auto RESOURCE = m_managers.emplace_back(makeShared<CXXColorManager>(makeShared<CXxColorManagerV4>(client, ver, id)));

    if UNLIKELY (!RESOURCE->good()) {
        wl_client_post_no_memory(client);
        m_managers.pop_back();
        return;
    }

    LOGM(TRACE, "New xx_color_manager at {:x}", (uintptr_t)RESOURCE.get());
}

void CXXColorManagementProtocol::onImagePreferredChanged() {
    for (auto const& feedback : m_feedbackSurfaces) {
        feedback->m_resource->sendPreferredChanged();
    }
}

void CXXColorManagementProtocol::destroyResource(CXXColorManager* resource) {
    std::erase_if(m_managers, [&](const auto& other) { return other.get() == resource; });
}

void CXXColorManagementProtocol::destroyResource(CXXColorManagementOutput* resource) {
    std::erase_if(m_outputs, [&](const auto& other) { return other.get() == resource; });
}

void CXXColorManagementProtocol::destroyResource(CXXColorManagementSurface* resource) {
    std::erase_if(m_surfaces, [&](const auto& other) { return other.get() == resource; });
}

void CXXColorManagementProtocol::destroyResource(CXXColorManagementFeedbackSurface* resource) {
    std::erase_if(m_feedbackSurfaces, [&](const auto& other) { return other.get() == resource; });
}

void CXXColorManagementProtocol::destroyResource(CXXColorManagementParametricCreator* resource) {
    std::erase_if(m_parametricCreators, [&](const auto& other) { return other.get() == resource; });
}

void CXXColorManagementProtocol::destroyResource(CXXColorManagementImageDescription* resource) {
    std::erase_if(m_imageDescriptions, [&](const auto& other) { return other.get() == resource; });
}
