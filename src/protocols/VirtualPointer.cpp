#include "VirtualPointer.hpp"

#define LOGM PROTO::virtualPointer->protoLog

static const wlr_pointer_impl pointerImpl = {
    .name = "virtual-pointer-v1",
};

CVirtualPointerV1Resource::CVirtualPointerV1Resource(SP<CZwlrVirtualPointerV1> resource_) : resource(resource_) {
    if (!good())
        return;

    resource->setDestroy([this](CZwlrVirtualPointerV1* r) {
        events.destroy.emit();
        PROTO::virtualPointer->destroyResource(this);
    });
    resource->setOnDestroy([this](CZwlrVirtualPointerV1* r) {
        events.destroy.emit();
        PROTO::virtualPointer->destroyResource(this);
    });

    wlr_pointer_init(&pointer, &pointerImpl, "CVirtualPointerV1Resource");

    resource->setMotion([this](CZwlrVirtualPointerV1* r, uint32_t timeMs, wl_fixed_t dx, wl_fixed_t dy) {
        wlr_pointer_motion_event event = {
            .pointer    = &pointer,
            .time_msec  = timeMs,
            .delta_x    = wl_fixed_to_double(dx),
            .delta_y    = wl_fixed_to_double(dy),
            .unaccel_dx = wl_fixed_to_double(dx),
            .unaccel_dy = wl_fixed_to_double(dy),
        };
        wl_signal_emit_mutable(&pointer.events.motion, &event);
    });

    resource->setMotionAbsolute([this](CZwlrVirtualPointerV1* r, uint32_t timeMs, uint32_t x, uint32_t y, uint32_t xExtent, uint32_t yExtent) {
        if (!xExtent || !yExtent)
            return;

        wlr_pointer_motion_absolute_event event = {
            .pointer   = &pointer,
            .time_msec = timeMs,
            .x         = (double)x / xExtent,
            .y         = (double)y / yExtent,
        };
        wl_signal_emit_mutable(&pointer.events.motion_absolute, &event);
    });

    resource->setButton([this](CZwlrVirtualPointerV1* r, uint32_t timeMs, uint32_t button, uint32_t state) {
        struct wlr_pointer_button_event event = {
            .pointer   = &pointer,
            .time_msec = timeMs,
            .button    = button,
            .state     = (wl_pointer_button_state)state,
        };
        wl_signal_emit_mutable(&pointer.events.button, &event);
    });

    resource->setAxis([this](CZwlrVirtualPointerV1* r, uint32_t timeMs, uint32_t axis_, wl_fixed_t value) {
        if (axis > WL_POINTER_AXIS_HORIZONTAL_SCROLL) {
            r->error(ZWLR_VIRTUAL_POINTER_V1_ERROR_INVALID_AXIS, "Invalid axis");
            return;
        }

        axis             = axis_;
        axisEvents[axis] = wlr_pointer_axis_event{.pointer = &pointer, .time_msec = timeMs, .orientation = (wl_pointer_axis)axis, .delta = wl_fixed_to_double(value)};
    });

    resource->setFrame([this](CZwlrVirtualPointerV1* r) {
        for (auto& e : axisEvents) {
            if (!e.pointer)
                continue;
            wl_signal_emit_mutable(&pointer.events.axis, &e);
            e.pointer = nullptr;
        }

        wl_signal_emit_mutable(&pointer.events.frame, &pointer);
    });

    resource->setAxisSource([this](CZwlrVirtualPointerV1* r, uint32_t source) { axisEvents[axis].source = (wl_pointer_axis_source)source; });

    resource->setAxisStop([this](CZwlrVirtualPointerV1* r, uint32_t timeMs, uint32_t axis_) {
        if (axis > WL_POINTER_AXIS_HORIZONTAL_SCROLL) {
            r->error(ZWLR_VIRTUAL_POINTER_V1_ERROR_INVALID_AXIS, "Invalid axis");
            return;
        }

        axis                            = axis_;
        axisEvents[axis].pointer        = &pointer;
        axisEvents[axis].time_msec      = timeMs;
        axisEvents[axis].orientation    = (wl_pointer_axis)axis;
        axisEvents[axis].delta          = 0;
        axisEvents[axis].delta_discrete = 0;
    });

    resource->setAxisDiscrete([this](CZwlrVirtualPointerV1* r, uint32_t timeMs, uint32_t axis_, wl_fixed_t value, int32_t discrete) {
        if (axis > WL_POINTER_AXIS_HORIZONTAL_SCROLL) {
            r->error(ZWLR_VIRTUAL_POINTER_V1_ERROR_INVALID_AXIS, "Invalid axis");
            return;
        }

        axis                            = axis_;
        axisEvents[axis].pointer        = &pointer;
        axisEvents[axis].time_msec      = timeMs;
        axisEvents[axis].orientation    = (wl_pointer_axis)axis;
        axisEvents[axis].delta          = wl_fixed_to_double(value);
        axisEvents[axis].delta_discrete = discrete * 120;
    });
}

CVirtualPointerV1Resource::~CVirtualPointerV1Resource() {
    wlr_pointer_finish(&pointer);
    events.destroy.emit();
}

bool CVirtualPointerV1Resource::good() {
    return resource->resource();
}

wlr_pointer* CVirtualPointerV1Resource::wlr() {
    return &pointer;
}

wl_client* CVirtualPointerV1Resource::client() {
    return resource->client();
}

CVirtualPointerProtocol::CVirtualPointerProtocol(const wl_interface* iface, const int& ver, const std::string& name) : IWaylandProtocol(iface, ver, name) {
    ;
}

void CVirtualPointerProtocol::bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id) {
    const auto RESOURCE = m_vManagers.emplace_back(std::make_unique<CZwlrVirtualPointerManagerV1>(client, ver, id)).get();
    RESOURCE->setOnDestroy([this](CZwlrVirtualPointerManagerV1* p) { this->onManagerResourceDestroy(p->resource()); });
    RESOURCE->setDestroy([this](CZwlrVirtualPointerManagerV1* p) { this->onManagerResourceDestroy(p->resource()); });

    RESOURCE->setCreateVirtualPointer([this](CZwlrVirtualPointerManagerV1* pMgr, wl_resource* seat, uint32_t id) { this->onCreatePointer(pMgr, seat, id); });
    RESOURCE->setCreateVirtualPointerWithOutput([this](CZwlrVirtualPointerManagerV1* pMgr, wl_resource* seat, wl_resource* output, uint32_t id) {
        LOGM(WARN, "TODO: CreateWithOutput is not supported yet. Ignoring for now.");
        this->onCreatePointer(pMgr, seat, id);
    });
}

void CVirtualPointerProtocol::onManagerResourceDestroy(wl_resource* res) {
    std::erase_if(m_vManagers, [&](const auto& other) { return other->resource() == res; });
}

void CVirtualPointerProtocol::destroyResource(CVirtualPointerV1Resource* pointer) {
    std::erase_if(m_vPointers, [&](const auto& other) { return other.get() == pointer; });
}

void CVirtualPointerProtocol::onCreatePointer(CZwlrVirtualPointerManagerV1* pMgr, wl_resource* seat, uint32_t id) {

    const auto RESOURCE = m_vPointers.emplace_back(makeShared<CVirtualPointerV1Resource>(makeShared<CZwlrVirtualPointerV1>(pMgr->client(), pMgr->version(), id)));

    if (!RESOURCE->good()) {
        pMgr->noMemory();
        m_vPointers.pop_back();
        return;
    }

    LOGM(LOG, "New VPointer at id {}", id);

    events.newPointer.emit(RESOURCE);
}