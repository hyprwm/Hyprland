#include "LinuxDMABUF.hpp"
#include <algorithm>
#include <set>
#include <tuple>
#include "../helpers/MiscFunctions.hpp"
#include <sys/mman.h>
#include <xf86drm.h>
#include <fcntl.h>
#include <sys/stat.h>
#include "core/Compositor.hpp"
#include "types/DMABuffer.hpp"
#include "types/WLBuffer.hpp"
#include "../managers/HookSystemManager.hpp"
#include "../render/OpenGL.hpp"
#include "../Compositor.hpp"

#define LOGM PROTO::linuxDma->protoLog

static std::optional<dev_t> devIDFromFD(int fd) {
    struct stat stat;
    if (fstat(fd, &stat) != 0)
        return {};
    return stat.st_rdev;
}

CDMABUFFormatTable::CDMABUFFormatTable(SDMABUFTranche _rendererTranche, std::vector<std::pair<SP<CMonitor>, SDMABUFTranche>> tranches_) :
    rendererTranche(_rendererTranche), monitorTranches(tranches_) {
    // yes this is a waste of memory, oh well

    tableLen                   = 0;
    rendererTranche.firstIndex = 0;
    for (auto& fmt : rendererTranche.formats) {
        tableLen += fmt.modifiers.size();
    }
    rendererTranche.lastIndex = tableLen;

    for (auto& tranche : monitorTranches) {
        tranche.second.firstIndex = tableLen;
        for (auto& fmt : tranche.second.formats) {
            tableLen += fmt.modifiers.size();
        }
        tranche.second.lastIndex = tableLen;
    }

    tableSize = tableLen * sizeof(SDMABUFFeedbackTableEntry);

    int fds[2] = {0};
    allocateSHMFilePair(tableSize, &fds[0], &fds[1]);

    auto arr = (SDMABUFFeedbackTableEntry*)mmap(nullptr, tableSize, PROT_READ | PROT_WRITE, MAP_SHARED, fds[0], 0);

    if (arr == MAP_FAILED) {
        LOGM(ERR, "mmap failed");
        close(fds[0]);
        close(fds[1]);
        return;
    }

    close(fds[0]);

    size_t i = 0;

    for (auto& fmt : rendererTranche.formats) {
        for (auto& mod : fmt.modifiers) {
            LOGM(TRACE, "Feedback: format table index {}: fmt {} mod {}", i, FormatUtils::drmFormatName(fmt.drmFormat), mod);
            arr[i++] = SDMABUFFeedbackTableEntry{
                .fmt      = fmt.drmFormat,
                .modifier = mod,
            };
        }
    }

    for (auto& tranche : monitorTranches) {
        for (auto& fmt : tranche.second.formats) {
            for (auto& mod : fmt.modifiers) {
                LOGM(TRACE, "Feedback: format table index {}: fmt {} mod {}", i, FormatUtils::drmFormatName(fmt.drmFormat), mod);
                arr[i++] = SDMABUFFeedbackTableEntry{
                    .fmt      = fmt.drmFormat,
                    .modifier = mod,
                };
            }
        }
    }

    munmap(arr, tableSize);

    tableFD = fds[1];
}

CDMABUFFormatTable::~CDMABUFFormatTable() {
    close(tableFD);
}

CLinuxDMABuffer::CLinuxDMABuffer(uint32_t id, wl_client* client, Aquamarine::SDMABUFAttrs attrs) {
    buffer = makeShared<CDMABuffer>(id, client, attrs);

    buffer->resource->buffer = buffer;

    listeners.bufferResourceDestroy = buffer->events.destroy.registerListener([this](std::any d) {
        listeners.bufferResourceDestroy.reset();
        PROTO::linuxDma->destroyResource(this);
    });

    if (!buffer->success)
        LOGM(ERR, "Possibly compositor bug: buffer failed to create");
}

CLinuxDMABuffer::~CLinuxDMABuffer() {
    buffer.reset();
    listeners.bufferResourceDestroy.reset();
}

bool CLinuxDMABuffer::good() {
    return buffer && buffer->good();
}

CLinuxDMABBUFParamsResource::CLinuxDMABBUFParamsResource(SP<CZwpLinuxBufferParamsV1> resource_) : resource(resource_) {
    if (!good())
        return;

    resource->setOnDestroy([this](CZwpLinuxBufferParamsV1* r) { PROTO::linuxDma->destroyResource(this); });
    resource->setDestroy([this](CZwpLinuxBufferParamsV1* r) { PROTO::linuxDma->destroyResource(this); });

    attrs = makeShared<Aquamarine::SDMABUFAttrs>();

    attrs->success = true;

    resource->setAdd([this](CZwpLinuxBufferParamsV1* r, int32_t fd, uint32_t plane, uint32_t offset, uint32_t stride, uint32_t modHi, uint32_t modLo) {
        if (used) {
            r->error(ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_ALREADY_USED, "Already used");
            return;
        }

        if (plane > 3) {
            r->error(ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_PLANE_IDX, "plane > 3");
            return;
        }

        if (attrs->fds.at(plane) != -1) {
            r->error(ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_PLANE_IDX, "plane used");
            return;
        }

        attrs->fds[plane]     = fd;
        attrs->strides[plane] = stride;
        attrs->offsets[plane] = offset;
        attrs->modifier       = ((uint64_t)modHi << 32) | modLo;
    });

    resource->setCreate([this](CZwpLinuxBufferParamsV1* r, int32_t w, int32_t h, uint32_t fmt, zwpLinuxBufferParamsV1Flags flags) {
        if (used) {
            r->error(ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_ALREADY_USED, "Already used");
            return;
        }

        if (flags > 0) {
            r->sendFailed();
            LOGM(ERR, "DMABUF flags are not supported");
            return;
        }

        attrs->size   = {w, h};
        attrs->format = fmt;
        attrs->planes = 4 - std::count(attrs->fds.begin(), attrs->fds.end(), -1);

        create(0);
    });

    resource->setCreateImmed([this](CZwpLinuxBufferParamsV1* r, uint32_t id, int32_t w, int32_t h, uint32_t fmt, zwpLinuxBufferParamsV1Flags flags) {
        if (used) {
            r->error(ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_ALREADY_USED, "Already used");
            return;
        }

        if (flags > 0) {
            r->sendFailed();
            LOGM(ERR, "DMABUF flags are not supported");
            return;
        }

        attrs->size   = {w, h};
        attrs->format = fmt;
        attrs->planes = 4 - std::count(attrs->fds.begin(), attrs->fds.end(), -1);

        create(id);
    });
}

CLinuxDMABBUFParamsResource::~CLinuxDMABBUFParamsResource() {
    ;
}

bool CLinuxDMABBUFParamsResource::good() {
    return resource->resource();
}

void CLinuxDMABBUFParamsResource::create(uint32_t id) {
    used = true;

    if (!verify()) {
        LOGM(ERR, "Failed creating a dmabuf: verify() said no");
        return; // if verify failed, we errored the resource.
    }

    if (!commence()) {
        LOGM(ERR, "Failed creating a dmabuf: commence() said no");
        resource->sendFailed();
        return;
    }

    LOGM(LOG, "Creating a dmabuf, with id {}: size {}, fmt {}, planes {}", id, attrs->size, FormatUtils::drmFormatName(attrs->format), attrs->planes);
    for (int i = 0; i < attrs->planes; ++i) {
        LOGM(LOG, " | plane {}: mod {} fd {} stride {} offset {}", i, attrs->modifier, attrs->fds[i], attrs->strides[i], attrs->offsets[i]);
    }

    auto buf = PROTO::linuxDma->m_vBuffers.emplace_back(makeShared<CLinuxDMABuffer>(id, resource->client(), *attrs));

    if (!buf->good() || !buf->buffer->success) {
        resource->sendFailed();
        return;
    }

    if (!id)
        resource->sendCreated(PROTO::linuxDma->m_vBuffers.back()->buffer->resource->getResource());

    createdBuffer = buf;
}

bool CLinuxDMABBUFParamsResource::commence() {
    if (PROTO::linuxDma->mainDeviceFD < 0)
        return true;

    for (int i = 0; i < attrs->planes; i++) {
        uint32_t handle = 0;

        if (drmPrimeFDToHandle(PROTO::linuxDma->mainDeviceFD, attrs->fds.at(i), &handle)) {
            LOGM(ERR, "Failed to import dmabuf fd");
            return false;
        }

        if (drmCloseBufferHandle(PROTO::linuxDma->mainDeviceFD, handle)) {
            LOGM(ERR, "Failed to close dmabuf handle");
            return false;
        }
    }

    return true;
}

bool CLinuxDMABBUFParamsResource::verify() {
    if (attrs->planes <= 0) {
        resource->error(ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_INCOMPLETE, "No planes added");
        return false;
    }

    if (attrs->fds.at(0) < 0) {
        resource->error(ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_INCOMPLETE, "No plane 0");
        return false;
    }

    bool empty = false;
    for (auto& plane : attrs->fds) {
        if (empty && plane != -1) {
            resource->error(ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_INVALID_FORMAT, "Gap in planes");
            return false;
        }

        if (plane == -1) {
            empty = true;
            continue;
        }
    }

    if (attrs->size.x < 1 || attrs->size.y < 1) {
        resource->error(ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_INVALID_DIMENSIONS, "x/y < 1");
        return false;
    }

    for (size_t i = 0; i < (size_t)attrs->planes; ++i) {
        if ((uint64_t)attrs->offsets.at(i) + (uint64_t)attrs->strides.at(i) * attrs->size.y > UINT32_MAX) {
            resource->error(ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_OUT_OF_BOUNDS,
                            std::format("size overflow on plane {}: offset {} + stride {} * height {} = {}, overflows UINT32_MAX", i, (uint64_t)attrs->offsets.at(i),
                                        (uint64_t)attrs->strides.at(i), attrs->size.y, (uint64_t)attrs->offsets.at(i) + (uint64_t)attrs->strides.at(i)));
            return false;
        }
    }

    return true;
}

CLinuxDMABUFFeedbackResource::CLinuxDMABUFFeedbackResource(SP<CZwpLinuxDmabufFeedbackV1> resource_, SP<CWLSurfaceResource> surface_) : surface(surface_), resource(resource_) {
    if (!good())
        return;

    resource->setOnDestroy([this](CZwpLinuxDmabufFeedbackV1* r) { PROTO::linuxDma->destroyResource(this); });
    resource->setDestroy([this](CZwpLinuxDmabufFeedbackV1* r) { PROTO::linuxDma->destroyResource(this); });

    auto& formatTable = PROTO::linuxDma->formatTable;

    resource->sendFormatTable(formatTable->tableFD, formatTable->tableSize);

    // send default feedback
    sendDefault();
}

CLinuxDMABUFFeedbackResource::~CLinuxDMABUFFeedbackResource() {
    ;
}

bool CLinuxDMABUFFeedbackResource::good() {
    return resource->resource();
}

// default tranche is based on renderer (egl)
void CLinuxDMABUFFeedbackResource::sendDefault() {
    auto  mainDevice  = PROTO::linuxDma->mainDevice;
    auto& formatTable = PROTO::linuxDma->formatTable;

    //
    struct wl_array deviceArr = {
        .size = sizeof(mainDevice),
        .data = (void*)&mainDevice,
    };
    resource->sendMainDevice(&deviceArr);

    // Main tranche
    resource->sendTrancheTargetDevice(&deviceArr);

    // we aren't attempting direct scanout by default
    resource->sendTrancheFlags((zwpLinuxDmabufFeedbackV1TrancheFlags)0);

    wl_array indices;
    wl_array_init(&indices);
    for (size_t i = formatTable->rendererTranche.firstIndex; i < formatTable->rendererTranche.lastIndex; ++i) {
        *((uint16_t*)wl_array_add(&indices, sizeof(uint16_t))) = i;
    }
    resource->sendTrancheFormats(&indices);
    wl_array_release(&indices);
    resource->sendTrancheDone();

    resource->sendDone();

    lastFeedbackWasScanout = false;
}

CLinuxDMABUFResource::CLinuxDMABUFResource(SP<CZwpLinuxDmabufV1> resource_) : resource(resource_) {
    if (!good())
        return;

    resource->setOnDestroy([this](CZwpLinuxDmabufV1* r) { PROTO::linuxDma->destroyResource(this); });
    resource->setDestroy([this](CZwpLinuxDmabufV1* r) { PROTO::linuxDma->destroyResource(this); });

    resource->setGetDefaultFeedback([](CZwpLinuxDmabufV1* r, uint32_t id) {
        const auto RESOURCE =
            PROTO::linuxDma->m_vFeedbacks.emplace_back(makeShared<CLinuxDMABUFFeedbackResource>(makeShared<CZwpLinuxDmabufFeedbackV1>(r->client(), r->version(), id), nullptr));

        if (!RESOURCE->good()) {
            r->noMemory();
            PROTO::linuxDma->m_vFeedbacks.pop_back();
            return;
        }
    });

    resource->setGetSurfaceFeedback([](CZwpLinuxDmabufV1* r, uint32_t id, wl_resource* surf) {
        const auto RESOURCE = PROTO::linuxDma->m_vFeedbacks.emplace_back(
            makeShared<CLinuxDMABUFFeedbackResource>(makeShared<CZwpLinuxDmabufFeedbackV1>(r->client(), r->version(), id), CWLSurfaceResource::fromResource(surf)));

        if (!RESOURCE->good()) {
            r->noMemory();
            PROTO::linuxDma->m_vFeedbacks.pop_back();
            return;
        }
    });

    resource->setCreateParams([](CZwpLinuxDmabufV1* r, uint32_t id) {
        const auto RESOURCE = PROTO::linuxDma->m_vParams.emplace_back(makeShared<CLinuxDMABBUFParamsResource>(makeShared<CZwpLinuxBufferParamsV1>(r->client(), r->version(), id)));

        if (!RESOURCE->good()) {
            r->noMemory();
            PROTO::linuxDma->m_vParams.pop_back();
            return;
        }
    });

    if (resource->version() < 4)
        sendMods();
}

bool CLinuxDMABUFResource::good() {
    return resource->resource();
}

void CLinuxDMABUFResource::sendMods() {
    for (auto& fmt : PROTO::linuxDma->formatTable->rendererTranche.formats) {
        for (auto& mod : fmt.modifiers) {
            if (resource->version() < 3) {
                if (mod == DRM_FORMAT_MOD_INVALID || mod == DRM_FORMAT_MOD_LINEAR)
                    resource->sendFormat(fmt.drmFormat);
                continue;
            }

            // TODO: https://gitlab.freedesktop.org/xorg/xserver/-/issues/1166

            resource->sendModifier(fmt.drmFormat, mod >> 32, mod & 0xFFFFFFFF);
        }
    }
}

CLinuxDMABufV1Protocol::CLinuxDMABufV1Protocol(const wl_interface* iface, const int& ver, const std::string& name) : IWaylandProtocol(iface, ver, name) {
    static auto P = g_pHookSystem->hookDynamic("ready", [this](void* self, SCallbackInfo& info, std::any d) {
        int  rendererFD = g_pCompositor->m_iDRMFD;
        auto dev        = devIDFromFD(rendererFD);

        if (!dev.has_value()) {
            LOGM(ERR, "failed to get drm dev");
            PROTO::linuxDma.reset();
            return;
        }

        mainDevice = *dev;

        SDMABUFTranche eglTranche = {
            .device  = *dev,
            .formats = g_pHyprOpenGL->getDRMFormats(),
        };

        std::vector<std::pair<SP<CMonitor>, SDMABUFTranche>> tches;

        if (g_pCompositor->m_pAqBackend->hasSession()) {
            for (auto& mon : g_pCompositor->m_vMonitors) {
                auto tranche = SDMABUFTranche{
                    .device  = *dev,
                    .formats = mon->output->getRenderFormats(),
                };
                tches.push_back(std::make_pair<>(mon, tranche));
            }

            static auto monitorAdded = g_pHookSystem->hookDynamic("monitorAdded", [this](void* self, SCallbackInfo& info, std::any param) {
                auto pMonitor = std::any_cast<CMonitor*>(param);
                auto mon      = pMonitor->self.lock();
                auto tranche  = SDMABUFTranche{
                     .device  = mainDevice,
                     .formats = mon->output->getRenderFormats(),
                };
                formatTable->monitorTranches.push_back(std::make_pair<>(mon, tranche));
                resetFormatTable();
            });

            static auto monitorRemoved = g_pHookSystem->hookDynamic("monitorRemoved", [this](void* self, SCallbackInfo& info, std::any param) {
                auto pMonitor = std::any_cast<CMonitor*>(param);
                auto mon      = pMonitor->self.lock();
                std::erase_if(formatTable->monitorTranches, [mon](std::pair<SP<CMonitor>, SDMABUFTranche> pair) { return pair.first == mon; });
                resetFormatTable();
            });
        }

        formatTable = std::make_unique<CDMABUFFormatTable>(eglTranche, tches);

        drmDevice* device = nullptr;
        if (drmGetDeviceFromDevId(mainDevice, 0, &device) != 0) {
            LOGM(ERR, "failed to get drm dev");
            PROTO::linuxDma.reset();
            return;
        }

        if (device->available_nodes & (1 << DRM_NODE_RENDER)) {
            const char* name = device->nodes[DRM_NODE_RENDER];
            mainDeviceFD     = open(name, O_RDWR | O_CLOEXEC);
            drmFreeDevice(&device);
            if (mainDeviceFD < 0) {
                LOGM(ERR, "failed to open drm dev");
                PROTO::linuxDma.reset();
                return;
            }
        } else {
            LOGM(ERR, "DRM device {} has no render node!!", device->nodes[DRM_NODE_PRIMARY]);
            drmFreeDevice(&device);
        }
    });
}

void CLinuxDMABufV1Protocol::resetFormatTable() {
    if (!formatTable)
        return;

    LOGM(LOG, "Resetting format table");

    // this might be a big copy
    auto newFormatTable = std::make_unique<CDMABUFFormatTable>(formatTable->rendererTranche, formatTable->monitorTranches);

    for (auto& feedback : m_vFeedbacks) {
        feedback->resource->sendFormatTable(newFormatTable->tableFD, newFormatTable->tableSize);
        if (feedback->lastFeedbackWasScanout) {
            auto pWindow = g_pCompositor->getWindowFromSurface(feedback->surface);
            auto mon = std::find_if(g_pCompositor->m_vMonitors.begin(), g_pCompositor->m_vMonitors.end(), [pWindow](SP<CMonitor> mon) { return mon->ID == pWindow->m_iMonitorID; });
            if (mon == g_pCompositor->m_vMonitors.end()) {
                feedback->sendDefault();
                continue;
            }

            updateScanoutTranche(feedback->surface, *mon);
        } else {
            feedback->sendDefault();
        }
    }

    // delete old table after we sent new one
    formatTable = std::move(newFormatTable);
}

CLinuxDMABufV1Protocol::~CLinuxDMABufV1Protocol() {
    if (mainDeviceFD >= 0)
        close(mainDeviceFD);
}

void CLinuxDMABufV1Protocol::bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id) {
    const auto RESOURCE = m_vManagers.emplace_back(makeShared<CLinuxDMABUFResource>(makeShared<CZwpLinuxDmabufV1>(client, ver, id)));

    if (!RESOURCE->good()) {
        wl_client_post_no_memory(client);
        m_vManagers.pop_back();
        return;
    }
}

void CLinuxDMABufV1Protocol::destroyResource(CLinuxDMABUFResource* resource) {
    std::erase_if(m_vManagers, [&](const auto& other) { return other.get() == resource; });
}

void CLinuxDMABufV1Protocol::destroyResource(CLinuxDMABUFFeedbackResource* resource) {
    std::erase_if(m_vFeedbacks, [&](const auto& other) { return other.get() == resource; });
}

void CLinuxDMABufV1Protocol::destroyResource(CLinuxDMABBUFParamsResource* resource) {
    std::erase_if(m_vParams, [&](const auto& other) { return other.get() == resource; });
}

void CLinuxDMABufV1Protocol::destroyResource(CLinuxDMABuffer* resource) {
    std::erase_if(m_vBuffers, [&](const auto& other) { return other.get() == resource; });
}

void CLinuxDMABufV1Protocol::updateScanoutTranche(SP<CWLSurfaceResource> surface, SP<CMonitor> pMonitor) {
    SP<CLinuxDMABUFFeedbackResource> feedbackResource;
    for (auto& f : m_vFeedbacks) {
        if (f->surface != surface)
            continue;

        feedbackResource = f;
        break;
    }

    if (!feedbackResource) {
        LOGM(LOG, "updateScanoutTranche: surface has no dmabuf_feedback");
        return;
    }

    if (!pMonitor) {
        LOGM(LOG, "updateScanoutTranche: resetting feedback");
        feedbackResource->sendDefault();
        return;
    }

    const auto& monitorTranchePair = std::find_if(formatTable->monitorTranches.begin(), formatTable->monitorTranches.end(),
                                                  [pMonitor](std::pair<SP<CMonitor>, SDMABUFTranche> pair) { return pair.first == pMonitor; });

    if (monitorTranchePair == formatTable->monitorTranches.end()) {
        LOGM(LOG, "updateScanoutTranche: monitor has no tranche");
        return;
    }

    const auto& monitorTranche = (*monitorTranchePair).second;

    LOGM(LOG, "updateScanoutTranche: sending a scanout tranche");

    // send a dedicated scanout tranche that contains formats that:
    //  - match the format of the output
    //  - are not linear or implicit

    struct wl_array deviceArr = {
        .size = sizeof(mainDevice),
        .data = (void*)&mainDevice,
    };
    feedbackResource->resource->sendMainDevice(&deviceArr);
    feedbackResource->resource->sendTrancheTargetDevice(&deviceArr);
    feedbackResource->resource->sendTrancheFlags(ZWP_LINUX_DMABUF_FEEDBACK_V1_TRANCHE_FLAGS_SCANOUT);

    wl_array indices;
    wl_array_init(&indices);
    for (size_t i = monitorTranche.firstIndex; i < monitorTranche.lastIndex; ++i) {
        *((uint16_t*)wl_array_add(&indices, sizeof(uint16_t))) = i;
    }
    feedbackResource->resource->sendTrancheFormats(&indices);
    wl_array_release(&indices);
    feedbackResource->resource->sendTrancheDone();

    feedbackResource->resource->sendDone();

    feedbackResource->lastFeedbackWasScanout = true;
}
