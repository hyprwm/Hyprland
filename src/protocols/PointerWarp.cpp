#include "PointerWarp.hpp"
#include "core/Compositor.hpp"
#include "core/Seat.hpp"
#include "../desktop/WLSurface.hpp"
#include "../managers/SeatManager.hpp"
#include "../managers/PointerManager.hpp"

CPointerWarpProtocol::CPointerWarpProtocol(const wl_interface* iface, const int& ver, const std::string& name) : IWaylandProtocol(iface, ver, name) {
    ;
}

void CPointerWarpProtocol::bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id) {
    const auto& RESOURCE = m_managers.emplace_back(makeUnique<CWpPointerWarpV1>(client, ver, id));

    if UNLIKELY (!RESOURCE->resource()) {
        wl_client_post_no_memory(client);
        m_managers.pop_back();
        return;
    }

    RESOURCE->setOnDestroy([this](CWpPointerWarpV1* pMgr) { this->destroyManager(pMgr); });
    RESOURCE->setDestroy([this](CWpPointerWarpV1* pMgr) { this->destroyManager(pMgr); });

    RESOURCE->setWarpPointer([](CWpPointerWarpV1* pMgr, wl_resource* surface, wl_resource* pointer, wl_fixed_t x, wl_fixed_t y, uint32_t serial) {
        const auto PSURFACE = CWLSurfaceResource::fromResource(surface);
        if (g_pSeatManager->m_state.pointerFocus != PSURFACE)
            return;

        const auto SURFBOX   = CWLSurface::fromResource(PSURFACE)->getSurfaceBoxGlobal().value_or(CBox{});
        const auto LOCALPOS  = Vector2D{wl_fixed_to_double(x), wl_fixed_to_double(y)};
        const auto GLOBALPOS = LOCALPOS + SURFBOX.pos();
        if (!SURFBOX.containsPoint(GLOBALPOS))
            return;

        const auto PSEAT = CWLPointerResource::fromResource(pointer)->m_owner.lock();
        if (!g_pSeatManager->serialValid(PSEAT, serial))
            return;

        g_pPointerManager->warpTo(GLOBALPOS);
        g_pSeatManager->sendPointerMotion(Time::millis(Time::steadyNow()), LOCALPOS);
    });
}

void CPointerWarpProtocol::destroyManager(CWpPointerWarpV1* manager) {
    std::erase_if(m_managers, [&](const UP<CWpPointerWarpV1>& resource) { return resource.get() == manager; });
}
