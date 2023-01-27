#include "FractionalScale.hpp"

#include "../Compositor.hpp"

#define FRACTIONAL_SCALE_VERSION 1

static void bindManagerInt(wl_client* client, void* data, uint32_t version, uint32_t id) {
    g_pProtocolManager->m_pFractionalScaleProtocolManager->bindManager(client, data, version, id);
}

static void handleDisplayDestroy(struct wl_listener* listener, void* data) {
    g_pProtocolManager->m_pFractionalScaleProtocolManager->displayDestroy();
}

void CFractionalScaleProtocolManager::displayDestroy() {
    wl_global_destroy(m_pGlobal);
}

static void handleDestroy(wl_client* client, wl_resource* resource) {
    wl_resource_destroy(resource);
}

void handleGetFractionalScale(wl_client* client, wl_resource* resource, uint32_t id, wl_resource* surface) {
    g_pProtocolManager->m_pFractionalScaleProtocolManager->getFractionalScale(client, resource, id, surface);
}

CFractionalScaleProtocolManager::CFractionalScaleProtocolManager() {
    m_pGlobal = wl_global_create(g_pCompositor->m_sWLDisplay, &wp_fractional_scale_manager_v1_interface, FRACTIONAL_SCALE_VERSION, this, bindManagerInt);

    if (!m_pGlobal) {
        Debug::log(ERR, "FractionalScaleManager could not start! Fractional scaling will not work!");
        return;
    }

    m_liDisplayDestroy.notify = handleDisplayDestroy;
    wl_display_add_destroy_listener(g_pCompositor->m_sWLDisplay, &m_liDisplayDestroy);

    Debug::log(LOG, "FractionalScaleManager started successfully!");
}

static const struct wp_fractional_scale_manager_v1_interface fractionalScaleManagerImpl = {
    .destroy              = handleDestroy,
    .get_fractional_scale = handleGetFractionalScale,
};

void CFractionalScaleProtocolManager::bindManager(wl_client* client, void* data, uint32_t version, uint32_t id) {
    const auto RESOURCE = wl_resource_create(client, &wp_fractional_scale_manager_v1_interface, version, id);
    wl_resource_set_implementation(RESOURCE, &fractionalScaleManagerImpl, this, nullptr);

    Debug::log(LOG, "FractionalScaleManager bound successfully!");
}

static void handleDestroyScaleAddon(wl_client* client, wl_resource* resource);
//

static const struct wp_fractional_scale_v1_interface fractionalScaleAddonImpl { .destroy = handleDestroyScaleAddon };

//
SFractionalScaleAddon* addonFromResource(wl_resource* resource) {
    ASSERT(wl_resource_instance_of(resource, &wp_fractional_scale_v1_interface, &fractionalScaleAddonImpl));
    return (SFractionalScaleAddon*)wl_resource_get_user_data(resource);
}

static void handleDestroyScaleAddon(wl_client* client, wl_resource* resource) {
    wl_resource_destroy(resource);
}

static void handleAddonDestroy(wl_resource* resource) {
    const auto PADDON = addonFromResource(resource);
    if (PADDON->pResource) {
        wl_resource_set_user_data(PADDON->pResource, nullptr);
    }

    g_pProtocolManager->m_pFractionalScaleProtocolManager->removeAddon(PADDON->pSurface);
}

void CFractionalScaleProtocolManager::getFractionalScale(wl_client* client, wl_resource* resource, uint32_t id, wl_resource* surface) {
    const auto PSURFACE = wlr_surface_from_resource(surface);
    const auto PADDON   = getAddonForSurface(PSURFACE);

    PADDON->pResource = wl_resource_create(client, &wp_fractional_scale_v1_interface, wl_resource_get_version(resource), id);
    wl_resource_set_implementation(PADDON->pResource, &fractionalScaleAddonImpl, PADDON, handleAddonDestroy);

    wp_fractional_scale_v1_send_preferred_scale(PADDON->pResource, (uint32_t)std::round(PADDON->preferredScale * 120.0));
}

SFractionalScaleAddon* CFractionalScaleProtocolManager::getAddonForSurface(wlr_surface* surface) {
    const auto IT = std::find_if(m_vFractionalScaleAddons.begin(), m_vFractionalScaleAddons.end(), [&](const auto& other) { return other->pSurface == surface; });

    if (IT != m_vFractionalScaleAddons.end())
        return IT->get();

    m_vFractionalScaleAddons.emplace_back(std::make_unique<SFractionalScaleAddon>());

    m_vFractionalScaleAddons.back()->pSurface = surface;

    return m_vFractionalScaleAddons.back().get();
}

void CFractionalScaleProtocolManager::setPreferredScaleForSurface(wlr_surface* surface, double scale) {
    const auto PADDON = getAddonForSurface(surface);

    PADDON->preferredScale = scale;

    if (PADDON->pResource)
        wp_fractional_scale_v1_send_preferred_scale(PADDON->pResource, (uint32_t)std::round(scale * 120.0));
}

void CFractionalScaleProtocolManager::removeAddon(wlr_surface* surface) {
    std::erase_if(m_vFractionalScaleAddons, [&](const auto& other) { return other->pSurface == surface; });
}