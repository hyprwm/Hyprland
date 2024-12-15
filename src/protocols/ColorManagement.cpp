#include "ColorManagement.hpp"

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

    resource->setSetImageDescription([](CXxColorManagementSurfaceV4* r, wl_resource* image_description, uint32_t render_intent) {
        LOGM(TRACE, "Set image description for surface={}, desc={}, intent={} (FIXME: stub)", (uintptr_t)r, (uintptr_t)image_description, render_intent);
    });
    resource->setUnsetImageDescription([](CXxColorManagementSurfaceV4* r) { LOGM(TRACE, "Unset image description for surface={} (FIXME: stub)", (uintptr_t)r); });
}

bool CColorManagementSurface::good() {
    return resource->resource();
}

wl_client* CColorManagementSurface::client() {
    return pClient;
}

CColorManagementFeedbackSurface::CColorManagementFeedbackSurface(SP<CXxColorManagementFeedbackSurfaceV4> resource_, SP<CWLSurfaceResource> surface_) :
    surface(surface_), resource(resource_) {
    if (!good())
        return;

    pClient = resource->client();

    resource->setDestroy([this](CXxColorManagementFeedbackSurfaceV4* r) {
        LOGM(TRACE, "Destroy xx cm feedback surface {}", (uintptr_t)surface);
        PROTO::colorManagement->destroyResource(this);
    });
    resource->setOnDestroy([this](CXxColorManagementFeedbackSurfaceV4* r) {
        LOGM(TRACE, "Destroy xx cm feedback surface {}", (uintptr_t)surface);
        PROTO::colorManagement->destroyResource(this);
    });

    resource->setGetPreferred([](CXxColorManagementFeedbackSurfaceV4* r, uint32_t id) { LOGM(TRACE, "Get preferred for id {} (FIXME: stub)", id); });
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
        if (!this->valuesSet) {
            r->error(XX_IMAGE_DESCRIPTION_CREATOR_PARAMS_V4_ERROR_INCOMPLETE_SET, "Missing required settings");
            return;
        }

        // FIXME actually check consistency
        if (!this->valuesSet) {
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
        if (!this->valuesSet) {
            RESOURCE->resource()->sendFailed(XX_IMAGE_DESCRIPTION_V4_CAUSE_UNSUPPORTED, "unsupported");
            return;
        }

        RESOURCE->self     = RESOURCE;
        RESOURCE->settings = this->settings;
        RESOURCE->resource()->sendReady(id);

        PROTO::colorManagement->destroyResource(this);
    });
    resource->setSetTfNamed([this](CXxImageDescriptionCreatorParamsV4* r, uint32_t tf) {
        LOGM(TRACE, "Set image description transfer function to {}", tf);
        if (this->valuesSet & PC_TF) {
            r->error(XX_IMAGE_DESCRIPTION_CREATOR_PARAMS_V4_ERROR_ALREADY_SET, "Transfer function already set");
            return;
        }

        switch (tf) {
            case XX_COLOR_MANAGER_V4_TRANSFER_FUNCTION_SRGB: break;
            case XX_COLOR_MANAGER_V4_TRANSFER_FUNCTION_ST2084_PQ: break;
            default: r->error(XX_IMAGE_DESCRIPTION_CREATOR_PARAMS_V4_ERROR_INVALID_TF, "Unsupported transfer function"); return;
        }

        this->settings.transferFunction = (xxColorManagerV4TransferFunction)tf;
        this->valuesSet |= PC_TF;
    });
    resource->setSetTfPower([this](CXxImageDescriptionCreatorParamsV4* r, uint32_t eexp) {
        LOGM(TRACE, "Set image description tf power to {}", eexp);
        if (this->valuesSet & PC_TF_POWER) {
            r->error(XX_IMAGE_DESCRIPTION_CREATOR_PARAMS_V4_ERROR_ALREADY_SET, "Transfer function power already set");
            return;
        }
        this->settings.transferFunctionPower = eexp / 10000.0f;
        this->valuesSet |= PC_TF_POWER;
    });
    resource->setSetPrimariesNamed([this](CXxImageDescriptionCreatorParamsV4* r, uint32_t primaries) {
        LOGM(TRACE, "Set image description primaries by name {} (FIXME: stub)", primaries);
        if (this->valuesSet & PC_PRIMARIES) {
            r->error(XX_IMAGE_DESCRIPTION_CREATOR_PARAMS_V4_ERROR_ALREADY_SET, "Primaries already set");
            return;
        }

        switch (primaries) {
            case XX_COLOR_MANAGER_V4_PRIMARIES_SRGB: break;
            case XX_COLOR_MANAGER_V4_PRIMARIES_BT2020: break;
            default: r->error(XX_IMAGE_DESCRIPTION_CREATOR_PARAMS_V4_ERROR_INVALID_PRIMARIES, "Unsupported primaries");
        }
    });
    resource->setSetPrimaries(
        [this](CXxImageDescriptionCreatorParamsV4* r, int32_t r_x, int32_t r_y, int32_t g_x, int32_t g_y, int32_t b_x, int32_t b_y, int32_t w_x, int32_t w_y) {
            LOGM(TRACE, "Set image description primaries by values r:{},{} g:{},{} b:{},{} w:{},{}", r_x, r_y, g_x, g_y, b_x, b_y, w_x, w_y);
            if (this->valuesSet & PC_PRIMARIES) {
                r->error(XX_IMAGE_DESCRIPTION_CREATOR_PARAMS_V4_ERROR_ALREADY_SET, "Primaries already set");
                return;
            }
            this->settings.primaries =
                SImageDescription::SPCPRimaries{.red = {.x = r_x, .y = r_y}, .green = {.x = g_x, .y = g_y}, .blue = {.x = b_x, .y = b_y}, .white = {.x = w_x, .y = w_y}};
            this->valuesSet |= PC_PRIMARIES;
        });
    resource->setSetLuminances([this](CXxImageDescriptionCreatorParamsV4* r, uint32_t min_lum, uint32_t max_lum, uint32_t reference_lum) {
        auto min = min_lum / 10000.0f;
        LOGM(TRACE, "Set image description luminances to {} - {} ({})", min, max_lum, reference_lum);
        if (this->valuesSet & PC_LUMINANCES) {
            r->error(XX_IMAGE_DESCRIPTION_CREATOR_PARAMS_V4_ERROR_ALREADY_SET, "Luminances already set");
            return;
        }
        if (max_lum < reference_lum || reference_lum <= min) {
            r->error(XX_IMAGE_DESCRIPTION_CREATOR_PARAMS_V4_ERROR_INVALID_LUMINANCE, "Invalid luminances");
            return;
        }
        this->settings.luminances = SImageDescription::SPCLuminances{.min = min_lum, .max = max_lum, .reference = reference_lum};
        this->valuesSet |= PC_LUMINANCES;
    });
    resource->setSetMasteringDisplayPrimaries(
        [this](CXxImageDescriptionCreatorParamsV4* r, int32_t r_x, int32_t r_y, int32_t g_x, int32_t g_y, int32_t b_x, int32_t b_y, int32_t w_x, int32_t w_y) {
            LOGM(TRACE, "Set image description mastering primaries by values r:{},{} g:{},{} b:{},{} w:{},{}", r_x, r_y, g_x, g_y, b_x, b_y, w_x, w_y);
            // if (this->valuesSet & PC_MASTERING_PRIMARIES) {
            //     r->error(XX_IMAGE_DESCRIPTION_CREATOR_PARAMS_V4_ERROR_ALREADY_SET, "Mastering primaries already set");
            //     return;
            // }
            this->settings.masteringPrimaries =
                SImageDescription::SPCPRimaries{.red = {.x = r_x, .y = r_y}, .green = {.x = g_x, .y = g_y}, .blue = {.x = b_x, .y = b_y}, .white = {.x = w_x, .y = w_y}};
            this->valuesSet |= PC_MASTERING_PRIMARIES;
        });
    resource->setSetMasteringLuminance([this](CXxImageDescriptionCreatorParamsV4* r, uint32_t min_lum, uint32_t max_lum) {
        auto min = min_lum / 10000.0f;
        LOGM(TRACE, "Set image description mastering luminances to {} - {}", min, max_lum);
        // if (this->valuesSet & PC_MASTERING_LUMINANCES) {
        //     r->error(XX_IMAGE_DESCRIPTION_CREATOR_PARAMS_V4_ERROR_ALREADY_SET, "Mastering luminances already set");
        //     return;
        // }
        if (min > 0 && max_lum > 0 && max_lum <= min) {
            r->error(XX_IMAGE_DESCRIPTION_CREATOR_PARAMS_V4_ERROR_INVALID_LUMINANCE, "Invalid luminances");
            return;
        }
        this->settings.masteringLuminances = SImageDescription::SPCMasteringLuminances{.min = min_lum, .max = max_lum};
        this->valuesSet |= PC_MASTERING_LUMINANCES;
    });
    resource->setSetMaxCll([this](CXxImageDescriptionCreatorParamsV4* r, uint32_t max_cll) {
        LOGM(TRACE, "Set image description max content light level to {}", max_cll);
        // if (this->valuesSet & PC_CLL) {
        //     r->error(XX_IMAGE_DESCRIPTION_CREATOR_PARAMS_V4_ERROR_ALREADY_SET, "Max CLL already set");
        //     return;
        // }
        this->settings.maxCLL = max_cll;
        this->valuesSet |= PC_CLL;
    });
    resource->setSetMaxFall([this](CXxImageDescriptionCreatorParamsV4* r, uint32_t max_fall) {
        LOGM(TRACE, "Set image description max frame-average light level to {}", max_fall);
        // if (this->valuesSet & PC_FALL) {
        //     r->error(XX_IMAGE_DESCRIPTION_CREATOR_PARAMS_V4_ERROR_ALREADY_SET, "Max FALL already set");
        //     return;
        // }
        this->settings.maxFALL = max_fall;
        this->valuesSet |= PC_FALL;
    });
}

bool CColorManagementParametricCreator::good() {
    return resource->resource();
}

wl_client* CColorManagementParametricCreator::client() {
    return pClient;
}

CColorManagementImageDescription::CColorManagementImageDescription(SP<CXxImageDescriptionV4> resource_) : m_resource(resource_) {
    if (!good())
        return;

    pClient = m_resource->client();

    m_resource->setDestroy([this](CXxImageDescriptionV4* r) { PROTO::colorManagement->destroyResource(this); });
    m_resource->setOnDestroy([this](CXxImageDescriptionV4* r) { PROTO::colorManagement->destroyResource(this); });

    m_resource->setGetInformation([](CXxImageDescriptionV4* r, uint32_t id) { LOGM(TRACE, "Get image information for image={}, id={} (FIXME: stub)", (uintptr_t)r, id); });
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
