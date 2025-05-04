#include "VirtualPointer.hpp"
#include "core/Output.hpp"

CVirtualPointerV1Resource::CVirtualPointerV1Resource(SP<CZwlrVirtualPointerV1> resource_, PHLMONITORREF boundOutput_) : m_boundOutput(boundOutput_), m_resource(resource_) {
    if UNLIKELY (!good())
        return;

    m_resource->setDestroy([this](CZwlrVirtualPointerV1* r) {
        m_events.destroy.emit();
        PROTO::virtualPointer->destroyResource(this);
    });
    m_resource->setOnDestroy([this](CZwlrVirtualPointerV1* r) {
        m_events.destroy.emit();
        PROTO::virtualPointer->destroyResource(this);
    });

    m_resource->setMotion([this](CZwlrVirtualPointerV1* r, uint32_t timeMs, wl_fixed_t dx, wl_fixed_t dy) {
        m_events.move.emit(IPointer::SMotionEvent{
            .timeMs  = timeMs,
            .delta   = {wl_fixed_to_double(dx), wl_fixed_to_double(dy)},
            .unaccel = {wl_fixed_to_double(dx), wl_fixed_to_double(dy)},
            .device  = nullptr,
        });
    });

    m_resource->setMotionAbsolute([this](CZwlrVirtualPointerV1* r, uint32_t timeMs, uint32_t x, uint32_t y, uint32_t xExtent, uint32_t yExtent) {
        if (!xExtent || !yExtent)
            return;

        m_events.warp.emit(IPointer::SMotionAbsoluteEvent{
            .timeMs   = timeMs,
            .absolute = {(double)x / xExtent, (double)y / yExtent},
        });
    });

    m_resource->setButton([this](CZwlrVirtualPointerV1* r, uint32_t timeMs, uint32_t button, uint32_t state) {
        m_events.button.emit(IPointer::SButtonEvent{
            .timeMs = timeMs,
            .button = button,
            .state  = (wl_pointer_button_state)state,
        });
    });

    m_resource->setAxis([this](CZwlrVirtualPointerV1* r, uint32_t timeMs, uint32_t axis_, wl_fixed_t value) {
        if UNLIKELY (m_axis > WL_POINTER_AXIS_HORIZONTAL_SCROLL) {
            r->error(ZWLR_VIRTUAL_POINTER_V1_ERROR_INVALID_AXIS, "Invalid axis");
            return;
        }

        m_axis               = axis_;
        m_axisEvents[m_axis] = IPointer::SAxisEvent{.timeMs = timeMs, .axis = (wl_pointer_axis)m_axis, .delta = wl_fixed_to_double(value)};
    });

    m_resource->setFrame([this](CZwlrVirtualPointerV1* r) {
        for (auto& e : m_axisEvents) {
            if (!e.timeMs)
                continue;
            m_events.axis.emit(e);
            e.timeMs = 0;
        }

        m_events.frame.emit();
    });

    m_resource->setAxisSource([this](CZwlrVirtualPointerV1* r, uint32_t source) { m_axisEvents[m_axis].source = (wl_pointer_axis_source)source; });

    m_resource->setAxisStop([this](CZwlrVirtualPointerV1* r, uint32_t timeMs, uint32_t axis_) {
        if UNLIKELY (m_axis > WL_POINTER_AXIS_HORIZONTAL_SCROLL) {
            r->error(ZWLR_VIRTUAL_POINTER_V1_ERROR_INVALID_AXIS, "Invalid axis");
            return;
        }

        m_axis                             = axis_;
        m_axisEvents[m_axis].timeMs        = timeMs;
        m_axisEvents[m_axis].axis          = (wl_pointer_axis)m_axis;
        m_axisEvents[m_axis].delta         = 0;
        m_axisEvents[m_axis].deltaDiscrete = 0;
    });

    m_resource->setAxisDiscrete([this](CZwlrVirtualPointerV1* r, uint32_t timeMs, uint32_t axis_, wl_fixed_t value, int32_t discrete) {
        if UNLIKELY (m_axis > WL_POINTER_AXIS_HORIZONTAL_SCROLL) {
            r->error(ZWLR_VIRTUAL_POINTER_V1_ERROR_INVALID_AXIS, "Invalid axis");
            return;
        }

        m_axis                             = axis_;
        m_axisEvents[m_axis].timeMs        = timeMs;
        m_axisEvents[m_axis].axis          = (wl_pointer_axis)m_axis;
        m_axisEvents[m_axis].delta         = wl_fixed_to_double(value);
        m_axisEvents[m_axis].deltaDiscrete = discrete * 120;
    });
}

CVirtualPointerV1Resource::~CVirtualPointerV1Resource() {
    m_events.destroy.emit();
}

bool CVirtualPointerV1Resource::good() {
    return m_resource->resource();
}

wl_client* CVirtualPointerV1Resource::client() {
    return m_resource->client();
}

CVirtualPointerProtocol::CVirtualPointerProtocol(const wl_interface* iface, const int& ver, const std::string& name) : IWaylandProtocol(iface, ver, name) {
    ;
}

void CVirtualPointerProtocol::bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id) {
    const auto RESOURCE = m_managers.emplace_back(makeUnique<CZwlrVirtualPointerManagerV1>(client, ver, id)).get();
    RESOURCE->setOnDestroy([this](CZwlrVirtualPointerManagerV1* p) { this->onManagerResourceDestroy(p->resource()); });
    RESOURCE->setDestroy([this](CZwlrVirtualPointerManagerV1* p) { this->onManagerResourceDestroy(p->resource()); });

    RESOURCE->setCreateVirtualPointer([this](CZwlrVirtualPointerManagerV1* pMgr, wl_resource* seat, uint32_t id) { this->onCreatePointer(pMgr, seat, id, {}); });
    RESOURCE->setCreateVirtualPointerWithOutput([this](CZwlrVirtualPointerManagerV1* pMgr, wl_resource* seat, wl_resource* output, uint32_t id) {
        if (output) {
            auto RES = CWLOutputResource::fromResource(output);
            if (!RES) {
                this->onCreatePointer(pMgr, seat, id, {});
                return;
            }

            this->onCreatePointer(pMgr, seat, id, RES->m_monitor);
        } else
            this->onCreatePointer(pMgr, seat, id, {});
    });
}

void CVirtualPointerProtocol::onManagerResourceDestroy(wl_resource* res) {
    std::erase_if(m_managers, [&](const auto& other) { return other->resource() == res; });
}

void CVirtualPointerProtocol::destroyResource(CVirtualPointerV1Resource* pointer) {
    std::erase_if(m_pointers, [&](const auto& other) { return other.get() == pointer; });
}

void CVirtualPointerProtocol::onCreatePointer(CZwlrVirtualPointerManagerV1* pMgr, wl_resource* seat, uint32_t id, PHLMONITORREF output) {

    const auto RESOURCE = m_pointers.emplace_back(makeShared<CVirtualPointerV1Resource>(makeShared<CZwlrVirtualPointerV1>(pMgr->client(), pMgr->version(), id), output));

    if UNLIKELY (!RESOURCE->good()) {
        pMgr->noMemory();
        m_pointers.pop_back();
        return;
    }

    LOGM(LOG, "New VPointer at id {}", id);

    m_events.newPointer.emit(RESOURCE);
}