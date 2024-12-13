#include "ColorManagement.hpp"
#include "xx-color-management-v4.hpp"

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

    resource->setDestroy([](CXxColorManagerV4* r) { LOGM(LOG, "Destroy xx_color_manager at {:x} (generated default)", (void*)r); });
    resource->setGetOutput([](CXxColorManagerV4* r, uint32_t id, wl_resource* output) {
        LOGM(LOG, "Get output for id={}, output={}", id, (void*)output);
        const auto RESOURCE =
            PROTO::colorManagement->m_vOutputs.emplace_back(makeShared<CColorManagementOutput>(makeShared<CXxColorManagementOutputV4>(r->client(), r->version(), id)));

        if (!RESOURCE->good()) {
            r->noMemory();
            PROTO::colorManagement->m_vOutputs.pop_back();
            return;
        }

        RESOURCE->self = RESOURCE;
    });
    resource->setGetSurface([](CXxColorManagerV4* r, uint32_t id, wl_resource* surface) { LOGM(LOG, "Get surface for id={}, surface={} (FIXME: stub)", id, (void*)surface); });
    resource->setGetFeedbackSurface(
        [](CXxColorManagerV4* r, uint32_t id, wl_resource* surface) { LOGM(LOG, "Get feedback surface for id={}, surface={} (FIXME: stub)", id, (void*)surface); });
    resource->setNewIccCreator([](CXxColorManagerV4* r, uint32_t id) {
        LOGM(LOG, "New ICC creator for id={} (unsupported)", id);
        wl_resource_post_error(r->resource(), XX_COLOR_MANAGER_V4_ERROR_UNSUPPORTED_FEATURE, "ICC profiles are not supported");
    });
    resource->setNewParametricCreator([](CXxColorManagerV4* r, uint32_t id) { LOGM(LOG, "New parametric creator for id={} (FIXME: stub)", id); });

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
        LOGM(LOG, "Get image description for output={}, id={}", (void*)r, id);
        if (m_imageDescription)
            delete m_imageDescription;

        m_imageDescription = new CColorManagementImageDescription(makeShared<CXxImageDescriptionV4>(r->client(), r->version(), id), this->self);

        if (!m_imageDescription->good()) {
            r->noMemory();
            delete m_imageDescription;
            return;
        }
    });
}

CColorManagementOutput::~CColorManagementOutput() {
    if (m_imageDescription)
        delete m_imageDescription;
}

bool CColorManagementOutput::good() {
    return resource->resource();
}

wl_client* CColorManagementOutput::client() {
    return pClient;
}

CColorManagementImageDescription::CColorManagementImageDescription(SP<CXxImageDescriptionV4> resource_, WP<CColorManagementOutput> output) : resource(resource_), m_Output(output) {
    if (!good())
        return;

    pClient = resource->client();

    resource->setDestroy([this](CXxImageDescriptionV4* r) {
        // m_Output->resource->sendImageDescriptionChanged();
        if (m_Output->m_imageDescription)
            delete m_Output->m_imageDescription;
    });
    resource->setOnDestroy([this](CXxImageDescriptionV4* r) {
        // m_Output->resource->sendImageDescriptionChanged();
        if (m_Output->m_imageDescription)
            delete m_Output->m_imageDescription;
    });

    resource->setGetInformation([](CXxImageDescriptionV4* r, uint32_t id) { LOGM(LOG, "Get image information for image={}, id={} (FIXME: stub)", (void*)r, id); });
}

bool CColorManagementImageDescription::good() {
    return resource->resource();
}

wl_client* CColorManagementImageDescription::client() {
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

    LOGM(LOG, "New xx_color_manager at {:x}", (uintptr_t)RESOURCE.get());
}

void CColorManagementProtocol::destroyResource(CColorManager* resource) {
    std::erase_if(m_vManagers, [&](const auto& other) { return other.get() == resource; });
}

void CColorManagementProtocol::destroyResource(CColorManagementOutput* resource) {
    std::erase_if(m_vOutputs, [&](const auto& other) { return other.get() == resource; });
}
