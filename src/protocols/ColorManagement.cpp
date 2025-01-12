#include "ColorManagement.hpp"
#include "Compositor.hpp"

CColorManager::CColorManager(SP<CXxColorManagerV4> resource_) : resource(resource_) {
    if (!good())
        return;

    resource->sendSupportedFeature(XX_COLOR_MANAGER_V4_FEATURE_PARAMETRIC);
    resource->sendSupportedFeature(XX_COLOR_MANAGER_V4_FEATURE_EXTENDED_TARGET_VOLUME);
    resource->sendSupportedFeature(XX_COLOR_MANAGER_V4_FEATURE_SET_MASTERING_DISPLAY_PRIMARIES);
    resource->sendSupportedFeature(XX_COLOR_MANAGER_V4_FEATURE_SET_PRIMARIES);
    resource->sendSupportedFeature(XX_COLOR_MANAGER_V4_FEATURE_SET_LUMINANCES);

    resource->sendSupportedPrimariesNamed(XX_COLOR_MANAGER_V4_PRIMARIES_SRGB);
    // resource->sendSupportedPrimariesNamed(XX_COLOR_MANAGER_V4_PRIMARIES_PAL_M);
    // resource->sendSupportedPrimariesNamed(XX_COLOR_MANAGER_V4_PRIMARIES_PAL);
    // resource->sendSupportedPrimariesNamed(XX_COLOR_MANAGER_V4_PRIMARIES_NTSC);
    // resource->sendSupportedPrimariesNamed(XX_COLOR_MANAGER_V4_PRIMARIES_GENERIC_FILM);
    resource->sendSupportedPrimariesNamed(XX_COLOR_MANAGER_V4_PRIMARIES_BT2020);
    // resource->sendSupportedPrimariesNamed(XX_COLOR_MANAGER_V4_PRIMARIES_CIE1931_XYZ);
    // resource->sendSupportedPrimariesNamed(XX_COLOR_MANAGER_V4_PRIMARIES_DCI_P3);
    // resource->sendSupportedPrimariesNamed(XX_COLOR_MANAGER_V4_PRIMARIES_DISPLAY_P3);
    // resource->sendSupportedPrimariesNamed(XX_COLOR_MANAGER_V4_PRIMARIES_ADOBE_RGB);

    // resource->sendSupportedTfNamed(XX_COLOR_MANAGER_V4_TRANSFER_FUNCTION_GAMMA22);
    resource->sendSupportedTfNamed(XX_COLOR_MANAGER_V4_TRANSFER_FUNCTION_SRGB);
    resource->sendSupportedTfNamed(XX_COLOR_MANAGER_V4_TRANSFER_FUNCTION_ST2084_PQ);
    // resource->sendSupportedTfNamed(XX_COLOR_MANAGER_V4_TRANSFER_FUNCTION_LINEAR);

    resource->sendSupportedIntent(XX_COLOR_MANAGER_V4_RENDER_INTENT_PERCEPTUAL);
    // resource->sendSupportedIntent(XX_COLOR_MANAGER_V4_RENDER_INTENT_RELATIVE);
    // resource->sendSupportedIntent(XX_COLOR_MANAGER_V4_RENDER_INTENT_ABSOLUTE);
    // resource->sendSupportedIntent(XX_COLOR_MANAGER_V4_RENDER_INTENT_RELATIVE_BPC);

    resource->setDestroy([](CXxColorManagerV4* r) { LOGM(TRACE, "Destroy xx_color_manager at {:x} (generated default)", (uintptr_t)r); });
    resource->setGetOutput([](CXxColorManagerV4* r, uint32_t id, wl_resource* output) {
        LOGM(TRACE, "Get output for id={}, output={}", id, (uintptr_t)output);
        const auto RESOURCE =
            PROTO::colorManagement->m_vOutputs.emplace_back(makeShared<CColorManagementOutput>(makeShared<CXxColorManagementOutputV4>(r->client(), r->version(), id)));

        if (!RESOURCE->good()) {
            r->noMemory();
            PROTO::colorManagement->m_vOutputs.pop_back();
            return;
        }

        RESOURCE->self = RESOURCE;
    });
    resource->setGetSurface([](CXxColorManagerV4* r, uint32_t id, wl_resource* surface) {
        LOGM(TRACE, "Get surface for id={}, surface={}", id, (uintptr_t)surface);
        auto SURF = CWLSurfaceResource::fromResource(surface);

        if (!SURF) {
            LOGM(ERR, "No surface for resource {}", (uintptr_t)surface);
            r->error(-1, "Invalid surface (2)");
            return;
        }

        if (SURF->colorManagement) {
            r->error(XX_COLOR_MANAGER_V4_ERROR_SURFACE_EXISTS, "CM Surface already exists");
            return;
        }

        const auto RESOURCE =
            PROTO::colorManagement->m_vSurfaces.emplace_back(makeShared<CColorManagementSurface>(makeShared<CXxColorManagementSurfaceV4>(r->client(), r->version(), id), SURF));
        if (!RESOURCE->good()) {
            r->noMemory();
            PROTO::colorManagement->m_vSurfaces.pop_back();
            return;
        }

        RESOURCE->self = RESOURCE;

        SURF->colorManagement = RESOURCE;
    });
    resource->setGetFeedbackSurface([](CXxColorManagerV4* r, uint32_t id, wl_resource* surface) {
        LOGM(TRACE, "Get feedback surface for id={}, surface={}", id, (uintptr_t)surface);
        auto SURF = CWLSurfaceResource::fromResource(surface);

        if (!SURF) {
            LOGM(ERR, "No surface for resource {}", (uintptr_t)surface);
            r->error(-1, "Invalid surface (2)");
            return;
        }

        const auto RESOURCE = PROTO::colorManagement->m_vFeedbackSurfaces.emplace_back(
            makeShared<CColorManagementFeedbackSurface>(makeShared<CXxColorManagementFeedbackSurfaceV4>(r->client(), r->version(), id), SURF));

        if (!RESOURCE->good()) {
            r->noMemory();
            PROTO::colorManagement->m_vFeedbackSurfaces.pop_back();
            return;
        }

        RESOURCE->self = RESOURCE;
    });
    resource->setNewIccCreator([](CXxColorManagerV4* r, uint32_t id) {
        LOGM(WARN, "New ICC creator for id={} (unsupported)", id);
        r->error(XX_COLOR_MANAGER_V4_ERROR_UNSUPPORTED_FEATURE, "ICC profiles are not supported");
    });
    resource->setNewParametricCreator([](CXxColorManagerV4* r, uint32_t id) {
        LOGM(TRACE, "New parametric creator for id={}", id);

        const auto RESOURCE = PROTO::colorManagement->m_vParametricCreators.emplace_back(
            makeShared<CColorManagementParametricCreator>(makeShared<CXxImageDescriptionCreatorParamsV4>(r->client(), r->version(), id)));

        if (!RESOURCE->good()) {
            r->noMemory();
            PROTO::colorManagement->m_vParametricCreators.pop_back();
            return;
        }

        RESOURCE->self = RESOURCE;
    });

    resource->setOnDestroy([this](CXxColorManagerV4* r) { PROTO::colorManagement->destroyResource(this); });
}

bool CColorManager::good() {
    return resource->resource();
}

CColorManagementOutput::CColorManagementOutput(SP<CXxColorManagementOutputV4> resource_) : resource(resource_) {
    if (!good())
        return;

    pClient = resource->client();

    resource->setDestroy([this](CXxColorManagementOutputV4* r) { PROTO::colorManagement->destroyResource(this); });
    resource->setOnDestroy([this](CXxColorManagementOutputV4* r) { PROTO::colorManagement->destroyResource(this); });

    resource->setGetImageDescription([this](CXxColorManagementOutputV4* r, uint32_t id) {
        LOGM(TRACE, "Get image description for output={}, id={}", (uintptr_t)r, id);
        if (imageDescription.valid())
            PROTO::colorManagement->destroyResource(imageDescription.get());

        const auto RESOURCE = PROTO::colorManagement->m_vImageDescriptions.emplace_back(
            makeShared<CColorManagementImageDescription>(makeShared<CXxImageDescriptionV4>(r->client(), r->version(), id)));

        if (!RESOURCE->good()) {
            r->noMemory();
            PROTO::colorManagement->m_vImageDescriptions.pop_back();
            return;
        }

        RESOURCE->self = RESOURCE;
    });
}

bool CColorManagementOutput::good() {
    return resource->resource();
}

wl_client* CColorManagementOutput::client() {
    return pClient;
}

CColorManagementSurface::CColorManagementSurface(SP<CWLSurfaceResource> surface_) : surface(surface_) {
    // only for frog cm untill wayland cm is adopted
}

CColorManagementSurface::CColorManagementSurface(SP<CXxColorManagementSurfaceV4> resource_, SP<CWLSurfaceResource> surface_) : surface(surface_), resource(resource_) {
    if (!good())
        return;

    pClient = resource->client();

    resource->setDestroy([this](CXxColorManagementSurfaceV4* r) {
        LOGM(TRACE, "Destroy xx cm surface {}", (uintptr_t)surface);
        PROTO::colorManagement->destroyResource(this);
    });
    resource->setOnDestroy([this](CXxColorManagementSurfaceV4* r) {
        LOGM(TRACE, "Destroy xx cm surface {}", (uintptr_t)surface);
        PROTO::colorManagement->destroyResource(this);
    });

    resource->setSetImageDescription([this](CXxColorManagementSurfaceV4* r, wl_resource* image_description, uint32_t render_intent) {
        LOGM(TRACE, "Set image description for surface={}, desc={}, intent={}", (uintptr_t)r, (uintptr_t)image_description, render_intent);

        const auto PO = (CXxImageDescriptionV4*)wl_resource_get_user_data(image_description);
        if (!PO) { // FIXME check validity
            r->error(XX_COLOR_MANAGEMENT_SURFACE_V4_ERROR_IMAGE_DESCRIPTION, "Image description creation failed");
            return;
        }
        if (render_intent != XX_COLOR_MANAGER_V4_RENDER_INTENT_PERCEPTUAL) {
            r->error(XX_COLOR_MANAGEMENT_SURFACE_V4_ERROR_RENDER_INTENT, "Unsupported render intent");
            return;
        }

        const auto imageDescription = std::find_if(PROTO::colorManagement->m_vImageDescriptions.begin(), PROTO::colorManagement->m_vImageDescriptions.end(),
                                                   [&](const auto& other) { return other->resource()->resource() == image_description; });
        if (imageDescription == PROTO::colorManagement->m_vImageDescriptions.end()) {
            r->error(XX_COLOR_MANAGEMENT_SURFACE_V4_ERROR_IMAGE_DESCRIPTION, "Image description not found");
            return;
        }

        setHasImageDescription(true);
        m_imageDescription = imageDescription->get()->settings;
    });
    resource->setUnsetImageDescription([this](CXxColorManagementSurfaceV4* r) {
        LOGM(TRACE, "Unset image description for surface={}", (uintptr_t)r);
        m_imageDescription = SImageDescription{};
        setHasImageDescription(false);
    });
}

bool CColorManagementSurface::good() {
    return resource && resource->resource();
}

wl_client* CColorManagementSurface::client() {
    return pClient;
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
    m_hdrMetadata      = metadata;
    m_needsNewMetadata = false;
}

bool CColorManagementSurface::needsHdrMetadataUpdate() {
    return m_needsNewMetadata;
}

CColorManagementFeedbackSurface::CColorManagementFeedbackSurface(SP<CXxColorManagementFeedbackSurfaceV4> resource_, SP<CWLSurfaceResource> surface_) :
    surface(surface_), resource(resource_) {
    if (!good())
        return;

    pClient = resource->client();

    resource->setDestroy([this](CXxColorManagementFeedbackSurfaceV4* r) {
        LOGM(TRACE, "Destroy xx cm feedback surface {}", (uintptr_t)surface);
        if (m_currentPreferred.valid())
            PROTO::colorManagement->destroyResource(m_currentPreferred.get());
        PROTO::colorManagement->destroyResource(this);
    });
    resource->setOnDestroy([this](CXxColorManagementFeedbackSurfaceV4* r) {
        LOGM(TRACE, "Destroy xx cm feedback surface {}", (uintptr_t)surface);
        if (m_currentPreferred.valid())
            PROTO::colorManagement->destroyResource(m_currentPreferred.get());
        PROTO::colorManagement->destroyResource(this);
    });

    resource->setGetPreferred([this](CXxColorManagementFeedbackSurfaceV4* r, uint32_t id) {
        LOGM(TRACE, "Get preferred for id {}", id);

        if (m_currentPreferred.valid())
            PROTO::colorManagement->destroyResource(m_currentPreferred.get());

        const auto RESOURCE = PROTO::colorManagement->m_vImageDescriptions.emplace_back(
            makeShared<CColorManagementImageDescription>(makeShared<CXxImageDescriptionV4>(r->client(), r->version(), id), true));

        if (!RESOURCE->good()) {
            r->noMemory();
            PROTO::colorManagement->m_vImageDescriptions.pop_back();
            return;
        }

        RESOURCE->self     = RESOURCE;
        m_currentPreferred = RESOURCE;

        m_currentPreferred->settings = g_pCompositor->getPreferredImageDescription();

        RESOURCE->resource()->sendReady(id);
    });
}

bool CColorManagementFeedbackSurface::good() {
    return resource->resource();
}

wl_client* CColorManagementFeedbackSurface::client() {
    return pClient;
}

CColorManagementParametricCreator::CColorManagementParametricCreator(SP<CXxImageDescriptionCreatorParamsV4> resource_) : resource(resource_) {
    if (!good())
        return;
    //
    pClient = resource->client();

    resource->setOnDestroy([this](CXxImageDescriptionCreatorParamsV4* r) { PROTO::colorManagement->destroyResource(this); });

    resource->setCreate([this](CXxImageDescriptionCreatorParamsV4* r, uint32_t id) {
        LOGM(TRACE, "Create image description from params for id {}", id);

        // FIXME actually check completeness
        if (!valuesSet) {
            r->error(XX_IMAGE_DESCRIPTION_CREATOR_PARAMS_V4_ERROR_INCOMPLETE_SET, "Missing required settings");
            return;
        }

        // FIXME actually check consistency
        if (!valuesSet) {
            r->error(XX_IMAGE_DESCRIPTION_CREATOR_PARAMS_V4_ERROR_INCONSISTENT_SET, "Set is not consistent");
            return;
        }

        const auto RESOURCE = PROTO::colorManagement->m_vImageDescriptions.emplace_back(
            makeShared<CColorManagementImageDescription>(makeShared<CXxImageDescriptionV4>(r->client(), r->version(), id)));

        if (!RESOURCE->good()) {
            r->noMemory();
            PROTO::colorManagement->m_vImageDescriptions.pop_back();
            return;
        }

        // FIXME actually check support
        if (!valuesSet) {
            RESOURCE->resource()->sendFailed(XX_IMAGE_DESCRIPTION_V4_CAUSE_UNSUPPORTED, "unsupported");
            return;
        }

        RESOURCE->self     = RESOURCE;
        RESOURCE->settings = settings;
        RESOURCE->resource()->sendReady(id);

        PROTO::colorManagement->destroyResource(this);
    });
    resource->setSetTfNamed([this](CXxImageDescriptionCreatorParamsV4* r, uint32_t tf) {
        LOGM(TRACE, "Set image description transfer function to {}", tf);
        if (valuesSet & PC_TF) {
            r->error(XX_IMAGE_DESCRIPTION_CREATOR_PARAMS_V4_ERROR_ALREADY_SET, "Transfer function already set");
            return;
        }

        switch (tf) {
            case XX_COLOR_MANAGER_V4_TRANSFER_FUNCTION_SRGB: break;
            case XX_COLOR_MANAGER_V4_TRANSFER_FUNCTION_ST2084_PQ: break;
            default: r->error(XX_IMAGE_DESCRIPTION_CREATOR_PARAMS_V4_ERROR_INVALID_TF, "Unsupported transfer function"); return;
        }

        settings.transferFunction = (xxColorManagerV4TransferFunction)tf;
        valuesSet |= PC_TF;
    });
    resource->setSetTfPower([this](CXxImageDescriptionCreatorParamsV4* r, uint32_t eexp) {
        LOGM(TRACE, "Set image description tf power to {}", eexp);
        if (valuesSet & PC_TF_POWER) {
            r->error(XX_IMAGE_DESCRIPTION_CREATOR_PARAMS_V4_ERROR_ALREADY_SET, "Transfer function power already set");
            return;
        }
        settings.transferFunctionPower = eexp / 10000.0f;
        valuesSet |= PC_TF_POWER;
    });
    resource->setSetPrimariesNamed([this](CXxImageDescriptionCreatorParamsV4* r, uint32_t primaries) {
        LOGM(TRACE, "Set image description primaries by name {}", primaries);
        if (valuesSet & PC_PRIMARIES) {
            r->error(XX_IMAGE_DESCRIPTION_CREATOR_PARAMS_V4_ERROR_ALREADY_SET, "Primaries already set");
            return;
        }

        switch (primaries) {
            case XX_COLOR_MANAGER_V4_PRIMARIES_SRGB:
                settings.primariesNameSet = true;
                settings.primariesNamed   = XX_COLOR_MANAGER_V4_PRIMARIES_SRGB;
                settings.primaries        = NColorPrimaries::BT709;
                valuesSet |= PC_PRIMARIES;
                break;
            case XX_COLOR_MANAGER_V4_PRIMARIES_BT2020:
                settings.primariesNameSet = true;
                settings.primariesNamed   = XX_COLOR_MANAGER_V4_PRIMARIES_BT2020;
                settings.primaries        = NColorPrimaries::BT2020;
                valuesSet |= PC_PRIMARIES;
                break;
            default: r->error(XX_IMAGE_DESCRIPTION_CREATOR_PARAMS_V4_ERROR_INVALID_PRIMARIES, "Unsupported primaries");
        }
    });
    resource->setSetPrimaries(
        [this](CXxImageDescriptionCreatorParamsV4* r, int32_t r_x, int32_t r_y, int32_t g_x, int32_t g_y, int32_t b_x, int32_t b_y, int32_t w_x, int32_t w_y) {
            LOGM(TRACE, "Set image description primaries by values r:{},{} g:{},{} b:{},{} w:{},{}", r_x, r_y, g_x, g_y, b_x, b_y, w_x, w_y);
            if (valuesSet & PC_PRIMARIES) {
                r->error(XX_IMAGE_DESCRIPTION_CREATOR_PARAMS_V4_ERROR_ALREADY_SET, "Primaries already set");
                return;
            }
            settings.primariesNameSet = false;
            settings.primaries =
                SImageDescription::SPCPRimaries{.red = {.x = r_x, .y = r_y}, .green = {.x = g_x, .y = g_y}, .blue = {.x = b_x, .y = b_y}, .white = {.x = w_x, .y = w_y}};
            valuesSet |= PC_PRIMARIES;
        });
    resource->setSetLuminances([this](CXxImageDescriptionCreatorParamsV4* r, uint32_t min_lum, uint32_t max_lum, uint32_t reference_lum) {
        auto min = min_lum / 10000.0f;
        LOGM(TRACE, "Set image description luminances to {} - {} ({})", min, max_lum, reference_lum);
        if (valuesSet & PC_LUMINANCES) {
            r->error(XX_IMAGE_DESCRIPTION_CREATOR_PARAMS_V4_ERROR_ALREADY_SET, "Luminances already set");
            return;
        }
        if (max_lum < reference_lum || reference_lum <= min) {
            r->error(XX_IMAGE_DESCRIPTION_CREATOR_PARAMS_V4_ERROR_INVALID_LUMINANCE, "Invalid luminances");
            return;
        }
        settings.luminances = SImageDescription::SPCLuminances{.min = min, .max = max_lum, .reference = reference_lum};
        valuesSet |= PC_LUMINANCES;
    });
    resource->setSetMasteringDisplayPrimaries(
        [this](CXxImageDescriptionCreatorParamsV4* r, int32_t r_x, int32_t r_y, int32_t g_x, int32_t g_y, int32_t b_x, int32_t b_y, int32_t w_x, int32_t w_y) {
            LOGM(TRACE, "Set image description mastering primaries by values r:{},{} g:{},{} b:{},{} w:{},{}", r_x, r_y, g_x, g_y, b_x, b_y, w_x, w_y);
            // if (valuesSet & PC_MASTERING_PRIMARIES) {
            //     r->error(XX_IMAGE_DESCRIPTION_CREATOR_PARAMS_V4_ERROR_ALREADY_SET, "Mastering primaries already set");
            //     return;
            // }
            settings.masteringPrimaries =
                SImageDescription::SPCPRimaries{.red = {.x = r_x, .y = r_y}, .green = {.x = g_x, .y = g_y}, .blue = {.x = b_x, .y = b_y}, .white = {.x = w_x, .y = w_y}};
            valuesSet |= PC_MASTERING_PRIMARIES;
        });
    resource->setSetMasteringLuminance([this](CXxImageDescriptionCreatorParamsV4* r, uint32_t min_lum, uint32_t max_lum) {
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
        settings.masteringLuminances = SImageDescription::SPCMasteringLuminances{.min = min, .max = max_lum};
        valuesSet |= PC_MASTERING_LUMINANCES;
    });
    resource->setSetMaxCll([this](CXxImageDescriptionCreatorParamsV4* r, uint32_t max_cll) {
        LOGM(TRACE, "Set image description max content light level to {}", max_cll);
        // if (valuesSet & PC_CLL) {
        //     r->error(XX_IMAGE_DESCRIPTION_CREATOR_PARAMS_V4_ERROR_ALREADY_SET, "Max CLL already set");
        //     return;
        // }
        settings.maxCLL = max_cll;
        valuesSet |= PC_CLL;
    });
    resource->setSetMaxFall([this](CXxImageDescriptionCreatorParamsV4* r, uint32_t max_fall) {
        LOGM(TRACE, "Set image description max frame-average light level to {}", max_fall);
        // if (valuesSet & PC_FALL) {
        //     r->error(XX_IMAGE_DESCRIPTION_CREATOR_PARAMS_V4_ERROR_ALREADY_SET, "Max FALL already set");
        //     return;
        // }
        settings.maxFALL = max_fall;
        valuesSet |= PC_FALL;
    });
}

bool CColorManagementParametricCreator::good() {
    return resource->resource();
}

wl_client* CColorManagementParametricCreator::client() {
    return pClient;
}

CColorManagementImageDescription::CColorManagementImageDescription(SP<CXxImageDescriptionV4> resource_, bool allowGetInformation) :
    m_resource(resource_), m_allowGetInformation(allowGetInformation) {
    if (!good())
        return;

    pClient = m_resource->client();

    m_resource->setDestroy([this](CXxImageDescriptionV4* r) { PROTO::colorManagement->destroyResource(this); });
    m_resource->setOnDestroy([this](CXxImageDescriptionV4* r) { PROTO::colorManagement->destroyResource(this); });

    m_resource->setGetInformation([this](CXxImageDescriptionV4* r, uint32_t id) {
        LOGM(TRACE, "Get image information for image={}, id={}", (uintptr_t)r, id);
        if (!m_allowGetInformation) {
            r->error(XX_IMAGE_DESCRIPTION_V4_ERROR_NO_INFORMATION, "Image descriptions doesn't allow get_information request");
            return;
        }

        auto RESOURCE = makeShared<CColorManagementImageDescriptionInfo>(makeShared<CXxImageDescriptionInfoV4>(r->client(), r->version(), id), settings);

        if (!RESOURCE->good())
            r->noMemory();

        // CColorManagementImageDescriptionInfo should send everything in the constructor and be ready for destroying at this point
        RESOURCE.reset();
    });
}

bool CColorManagementImageDescription::good() {
    return m_resource->resource();
}

wl_client* CColorManagementImageDescription::client() {
    return pClient;
}

SP<CXxImageDescriptionV4> CColorManagementImageDescription::resource() {
    return m_resource;
}

CColorManagementImageDescriptionInfo::CColorManagementImageDescriptionInfo(SP<CXxImageDescriptionInfoV4> resource_, const SImageDescription& settings_) :
    m_resource(resource_), settings(settings_) {
    if (!good())
        return;

    pClient = m_resource->client();

    const auto toProto = [](float value) { return int32_t(std::round(value * 10000)); };

    if (settings.iccFd >= 0)
        m_resource->sendIccFile(settings.iccFd, settings.iccSize);

    // send preferred client paramateres
    m_resource->sendPrimaries(toProto(settings.primaries.red.x), toProto(settings.primaries.red.y), toProto(settings.primaries.green.x), toProto(settings.primaries.green.y),
                              toProto(settings.primaries.blue.x), toProto(settings.primaries.blue.y), toProto(settings.primaries.white.x), toProto(settings.primaries.white.y));
    if (settings.primariesNameSet)
        m_resource->sendPrimariesNamed(settings.primariesNamed);
    m_resource->sendTfPower(std::round(settings.transferFunctionPower * 10000));
    m_resource->sendTfNamed(settings.transferFunction);
    m_resource->sendLuminances(std::round(settings.luminances.min * 10000), settings.luminances.max, settings.luminances.reference);

    // send expexted display paramateres
    m_resource->sendTargetPrimaries(toProto(settings.masteringPrimaries.red.x), toProto(settings.masteringPrimaries.red.y), toProto(settings.masteringPrimaries.green.x),
                                    toProto(settings.masteringPrimaries.green.y), toProto(settings.masteringPrimaries.blue.x), toProto(settings.masteringPrimaries.blue.y),
                                    toProto(settings.masteringPrimaries.white.x), toProto(settings.masteringPrimaries.white.y));
    m_resource->sendTargetLuminance(std::round(settings.masteringLuminances.min * 10000), settings.masteringLuminances.max);
    m_resource->sendTargetMaxCll(settings.maxCLL);
    m_resource->sendTargetMaxFall(settings.maxFALL);

    m_resource->sendDone();
}

bool CColorManagementImageDescriptionInfo::good() {
    return m_resource->resource();
}

wl_client* CColorManagementImageDescriptionInfo::client() {
    return pClient;
}

CColorManagementProtocol::CColorManagementProtocol(const wl_interface* iface, const int& ver, const std::string& name) : IWaylandProtocol(iface, ver, name) {
    ;
}

void CColorManagementProtocol::bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id) {
    const auto RESOURCE = m_vManagers.emplace_back(makeShared<CColorManager>(makeShared<CXxColorManagerV4>(client, ver, id)));

    if (!RESOURCE->good()) {
        wl_client_post_no_memory(client);
        m_vManagers.pop_back();
        return;
    }

    LOGM(TRACE, "New xx_color_manager at {:x}", (uintptr_t)RESOURCE.get());
}

void CColorManagementProtocol::onImagePreferredChanged() {
    for (auto const& feedback : m_vFeedbackSurfaces) {
        feedback->resource->sendPreferredChanged();
    }
}

void CColorManagementProtocol::destroyResource(CColorManager* resource) {
    std::erase_if(m_vManagers, [&](const auto& other) { return other.get() == resource; });
}

void CColorManagementProtocol::destroyResource(CColorManagementOutput* resource) {
    std::erase_if(m_vOutputs, [&](const auto& other) { return other.get() == resource; });
}

void CColorManagementProtocol::destroyResource(CColorManagementSurface* resource) {
    std::erase_if(m_vSurfaces, [&](const auto& other) { return other.get() == resource; });
}

void CColorManagementProtocol::destroyResource(CColorManagementFeedbackSurface* resource) {
    std::erase_if(m_vFeedbackSurfaces, [&](const auto& other) { return other.get() == resource; });
}

void CColorManagementProtocol::destroyResource(CColorManagementParametricCreator* resource) {
    std::erase_if(m_vParametricCreators, [&](const auto& other) { return other.get() == resource; });
}

void CColorManagementProtocol::destroyResource(CColorManagementImageDescription* resource) {
    std::erase_if(m_vImageDescriptions, [&](const auto& other) { return other.get() == resource; });
}
