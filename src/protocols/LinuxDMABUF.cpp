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

using namespace Hyprutils::OS;

static std::optional<dev_t> devIDFromFD(int fd) {
    struct stat stat;
    if (fstat(fd, &stat) != 0)
        return {};
    return stat.st_rdev;
}

CDMABUFFormatTable::CDMABUFFormatTable(SDMABUFTranche _rendererTranche, std::vector<std::pair<PHLMONITORREF, SDMABUFTranche>> tranches_) :
    m_rendererTranche(_rendererTranche), m_monitorTranches(tranches_) {

    std::vector<SDMABUFFormatTableEntry>    formatsVec;
    std::set<std::pair<uint32_t, uint64_t>> formats;

    // insert formats into vec if they got inserted into set, meaning they're unique
    size_t i = 0;

    m_rendererTranche.indices.clear();
    for (auto const& fmt : m_rendererTranche.formats) {
        for (auto const& mod : fmt.modifiers) {
            auto format        = std::make_pair<>(fmt.drmFormat, mod);
            auto [_, inserted] = formats.insert(format);
            if (inserted) {
                // if it was inserted into set, then its unique and will have a new index in vec
                m_rendererTranche.indices.push_back(i++);
                formatsVec.push_back(SDMABUFFormatTableEntry{
                    .fmt      = fmt.drmFormat,
                    .modifier = mod,
                });
            } else {
                // if it wasn't inserted then find its index in vec
                auto it = std::ranges::find_if(formatsVec, [fmt, mod](const SDMABUFFormatTableEntry& oth) { return oth.fmt == fmt.drmFormat && oth.modifier == mod; });
                m_rendererTranche.indices.push_back(it - formatsVec.begin());
            }
        }
    }

    for (auto& [monitor, tranche] : m_monitorTranches) {
        tranche.indices.clear();
        for (auto const& fmt : tranche.formats) {
            for (auto const& mod : fmt.modifiers) {
                // apparently these can implode on planes, so don't use them
                if (mod == DRM_FORMAT_MOD_INVALID || mod == DRM_FORMAT_MOD_LINEAR)
                    continue;
                auto format        = std::make_pair<>(fmt.drmFormat, mod);
                auto [_, inserted] = formats.insert(format);
                if (inserted) {
                    tranche.indices.push_back(i++);
                    formatsVec.push_back(SDMABUFFormatTableEntry{
                        .fmt      = fmt.drmFormat,
                        .modifier = mod,
                    });
                } else {
                    auto it = std::ranges::find_if(formatsVec, [fmt, mod](const SDMABUFFormatTableEntry& oth) { return oth.fmt == fmt.drmFormat && oth.modifier == mod; });
                    tranche.indices.push_back(it - formatsVec.begin());
                }
            }
        }
    }

    m_tableSize = formatsVec.size() * sizeof(SDMABUFFormatTableEntry);

    CFileDescriptor fds[2];
    allocateSHMFilePair(m_tableSize, fds[0], fds[1]);

    auto arr = static_cast<SDMABUFFormatTableEntry*>(mmap(nullptr, m_tableSize, PROT_READ | PROT_WRITE, MAP_SHARED, fds[0].get(), 0));

    if (arr == MAP_FAILED) {
        LOGM(ERR, "mmap failed");
        return;
    }

    std::ranges::copy(formatsVec, arr);

    munmap(arr, m_tableSize);

    m_tableFD = std::move(fds[1]);
}

CLinuxDMABuffer::CLinuxDMABuffer(uint32_t id, wl_client* client, Aquamarine::SDMABUFAttrs attrs) {
    m_buffer = makeShared<CDMABuffer>(id, client, attrs);

    m_buffer->m_resource->m_buffer = m_buffer;

    m_listeners.bufferResourceDestroy = m_buffer->events.destroy.listen([this] {
        m_listeners.bufferResourceDestroy.reset();
        PROTO::linuxDma->destroyResource(this);
    });

    if (!m_buffer->m_success)
        LOGM(ERR, "Possibly compositor bug: buffer failed to create");
}

CLinuxDMABuffer::~CLinuxDMABuffer() {
    if (m_buffer && m_buffer->m_resource)
        m_buffer->m_resource->sendRelease();

    m_buffer.reset();
    m_listeners.bufferResourceDestroy.reset();
}

bool CLinuxDMABuffer::good() {
    return m_buffer && m_buffer->good();
}

CLinuxDMABUFParamsResource::CLinuxDMABUFParamsResource(UP<CZwpLinuxBufferParamsV1>&& resource_) : m_resource(std::move(resource_)) {
    if UNLIKELY (!good())
        return;

    m_resource->setOnDestroy([this](CZwpLinuxBufferParamsV1* r) { PROTO::linuxDma->destroyResource(this); });
    m_resource->setDestroy([this](CZwpLinuxBufferParamsV1* r) { PROTO::linuxDma->destroyResource(this); });

    m_attrs = makeShared<Aquamarine::SDMABUFAttrs>();

    m_attrs->success = true;

    m_resource->setAdd([this](CZwpLinuxBufferParamsV1* r, int32_t fd, uint32_t plane, uint32_t offset, uint32_t stride, uint32_t modHi, uint32_t modLo) {
        if (m_used) {
            r->error(ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_ALREADY_USED, "Already used");
            return;
        }

        if (plane > 3) {
            r->error(ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_PLANE_IDX, "plane > 3");
            return;
        }

        if (m_attrs->fds.at(plane) != -1) {
            r->error(ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_PLANE_IDX, "plane used");
            return;
        }

        m_attrs->fds[plane]     = fd;
        m_attrs->strides[plane] = stride;
        m_attrs->offsets[plane] = offset;
        m_attrs->modifier       = (static_cast<uint64_t>(modHi) << 32) | modLo;
    });

    m_resource->setCreate([this](CZwpLinuxBufferParamsV1* r, int32_t w, int32_t h, uint32_t fmt, zwpLinuxBufferParamsV1Flags flags) {
        if (m_used) {
            r->error(ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_ALREADY_USED, "Already used");
            return;
        }

        if (flags > 0) {
            r->sendFailed();
            LOGM(ERR, "DMABUF flags are not supported");
            return;
        }

        m_attrs->size   = {w, h};
        m_attrs->format = fmt;
        m_attrs->planes = 4 - std::ranges::count(m_attrs->fds, -1);

        create(0);
    });

    m_resource->setCreateImmed([this](CZwpLinuxBufferParamsV1* r, uint32_t id, int32_t w, int32_t h, uint32_t fmt, zwpLinuxBufferParamsV1Flags flags) {
        if (m_used) {
            r->error(ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_ALREADY_USED, "Already used");
            return;
        }

        if (flags > 0) {
            r->sendFailed();
            LOGM(ERR, "DMABUF flags are not supported");
            return;
        }

        m_attrs->size   = {w, h};
        m_attrs->format = fmt;
        m_attrs->planes = 4 - std::ranges::count(m_attrs->fds, -1);

        create(id);
    });
}

bool CLinuxDMABUFParamsResource::good() {
    return m_resource->resource();
}

void CLinuxDMABUFParamsResource::create(uint32_t id) {
    m_used = true;

    if UNLIKELY (!verify()) {
        LOGM(ERR, "Failed creating a dmabuf: verify() said no");
        return; // if verify failed, we errored the resource.
    }

    if UNLIKELY (!commence()) {
        LOGM(ERR, "Failed creating a dmabuf: commence() said no");
        m_resource->sendFailed();
        return;
    }

    LOGM(LOG, "Creating a dmabuf, with id {}: size {}, fmt {}, planes {}", id, m_attrs->size, NFormatUtils::drmFormatName(m_attrs->format), m_attrs->planes);
    for (int i = 0; i < m_attrs->planes; ++i) {
        LOGM(LOG, " | plane {}: mod {} fd {} stride {} offset {}", i, m_attrs->modifier, m_attrs->fds[i], m_attrs->strides[i], m_attrs->offsets[i]);
    }

    auto& buf = PROTO::linuxDma->m_buffers.emplace_back(makeUnique<CLinuxDMABuffer>(id, m_resource->client(), *m_attrs));

    if UNLIKELY (!buf->good() || !buf->m_buffer->m_success) {
        m_resource->sendFailed();
        PROTO::linuxDma->m_buffers.pop_back();
        return;
    }

    if (!id)
        m_resource->sendCreated(buf->m_buffer->m_resource->getResource());

    m_createdBuffer = buf;
}

bool CLinuxDMABUFParamsResource::commence() {
    if (!PROTO::linuxDma->m_mainDeviceFD.isValid())
        return true;

    for (int i = 0; i < m_attrs->planes; i++) {
        uint32_t handle = 0;

        if (drmPrimeFDToHandle(PROTO::linuxDma->m_mainDeviceFD.get(), m_attrs->fds.at(i), &handle)) {
            LOGM(ERR, "Failed to import dmabuf fd");
            return false;
        }

        if (drmCloseBufferHandle(PROTO::linuxDma->m_mainDeviceFD.get(), handle)) {
            LOGM(ERR, "Failed to close dmabuf handle");
            return false;
        }
    }

    return true;
}

bool CLinuxDMABUFParamsResource::verify() {
    if UNLIKELY (m_attrs->planes <= 0) {
        m_resource->error(ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_INCOMPLETE, "No planes added");
        return false;
    }

    if UNLIKELY (m_attrs->fds.at(0) < 0) {
        m_resource->error(ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_INCOMPLETE, "No plane 0");
        return false;
    }

    bool empty = false;
    for (auto const& plane : m_attrs->fds) {
        if (empty && plane != -1) {
            m_resource->error(ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_INVALID_FORMAT, "Gap in planes");
            return false;
        }

        if (plane == -1) {
            empty = true;
            continue;
        }
    }

    if UNLIKELY (m_attrs->size.x < 1 || m_attrs->size.y < 1) {
        m_resource->error(ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_INVALID_DIMENSIONS, "x/y < 1");
        return false;
    }

    for (size_t i = 0; i < static_cast<size_t>(m_attrs->planes); ++i) {
        if (static_cast<uint64_t>(m_attrs->offsets.at(i)) + static_cast<uint64_t>(m_attrs->strides.at(i)) * m_attrs->size.y > UINT32_MAX) {
            m_resource->error(ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_OUT_OF_BOUNDS,
                              std::format("size overflow on plane {}: offset {} + stride {} * height {} = {}, overflows UINT32_MAX", i, static_cast<uint64_t>(m_attrs->offsets.at(i)),
                                          static_cast<uint64_t>(m_attrs->strides.at(i)), m_attrs->size.y, static_cast<uint64_t>(m_attrs->offsets.at(i)) + static_cast<uint64_t>(m_attrs->strides.at(i))));
            return false;
        }
    }

    return true;
}

CLinuxDMABUFFeedbackResource::CLinuxDMABUFFeedbackResource(UP<CZwpLinuxDmabufFeedbackV1>&& resource_, SP<CWLSurfaceResource> surface_) :
    m_surface(surface_), m_resource(std::move(resource_)) {
    if UNLIKELY (!good())
        return;

    m_resource->setOnDestroy([this](CZwpLinuxDmabufFeedbackV1* r) { PROTO::linuxDma->destroyResource(this); });
    m_resource->setDestroy([this](CZwpLinuxDmabufFeedbackV1* r) { PROTO::linuxDma->destroyResource(this); });

    auto& formatTable = PROTO::linuxDma->m_formatTable;
    m_resource->sendFormatTable(formatTable->m_tableFD.get(), formatTable->m_tableSize);
    sendDefaultFeedback();
}

bool CLinuxDMABUFFeedbackResource::good() {
    return m_resource->resource();
}

void CLinuxDMABUFFeedbackResource::sendTranche(SDMABUFTranche& tranche) {
    struct wl_array deviceArr = {
        .size = sizeof(tranche.device),
        .data = static_cast<void*>(&tranche.device),
    };
    m_resource->sendTrancheTargetDevice(&deviceArr);

    m_resource->sendTrancheFlags(static_cast<zwpLinuxDmabufFeedbackV1TrancheFlags>(tranche.flags));

    wl_array indices = {
        .size = tranche.indices.size() * sizeof(tranche.indices.at(0)),
        .data = tranche.indices.data(),
    };
    m_resource->sendTrancheFormats(&indices);
    m_resource->sendTrancheDone();
}

// default tranche is based on renderer (egl)
void CLinuxDMABUFFeedbackResource::sendDefaultFeedback() {
    auto            mainDevice  = PROTO::linuxDma->m_mainDevice;
    auto&           formatTable = PROTO::linuxDma->m_formatTable;

    struct wl_array deviceArr = {
        .size = sizeof(mainDevice),
        .data = static_cast<void*>(&mainDevice),
    };
    m_resource->sendMainDevice(&deviceArr);

    sendTranche(formatTable->m_rendererTranche);

    m_resource->sendDone();

    m_lastFeedbackWasScanout = false;
}

CLinuxDMABUFResource::CLinuxDMABUFResource(UP<CZwpLinuxDmabufV1>&& resource_) : m_resource(std::move(resource_)) {
    if UNLIKELY (!good())
        return;

    m_resource->setOnDestroy([this](CZwpLinuxDmabufV1* r) { PROTO::linuxDma->destroyResource(this); });
    m_resource->setDestroy([this](CZwpLinuxDmabufV1* r) { PROTO::linuxDma->destroyResource(this); });

    m_resource->setGetDefaultFeedback([](CZwpLinuxDmabufV1* r, uint32_t id) {
        const auto& RESOURCE =
            PROTO::linuxDma->m_feedbacks.emplace_back(makeUnique<CLinuxDMABUFFeedbackResource>(makeUnique<CZwpLinuxDmabufFeedbackV1>(r->client(), r->version(), id), nullptr));

        if UNLIKELY (!RESOURCE->good()) {
            r->noMemory();
            PROTO::linuxDma->m_feedbacks.pop_back();
            return;
        }
    });

    m_resource->setGetSurfaceFeedback([](CZwpLinuxDmabufV1* r, uint32_t id, wl_resource* surf) {
        const auto& RESOURCE = PROTO::linuxDma->m_feedbacks.emplace_back(
            makeUnique<CLinuxDMABUFFeedbackResource>(makeUnique<CZwpLinuxDmabufFeedbackV1>(r->client(), r->version(), id), CWLSurfaceResource::fromResource(surf)));

        if UNLIKELY (!RESOURCE->good()) {
            r->noMemory();
            PROTO::linuxDma->m_feedbacks.pop_back();
            return;
        }
    });

    m_resource->setCreateParams([](CZwpLinuxDmabufV1* r, uint32_t id) {
        const auto& RESOURCE = PROTO::linuxDma->m_params.emplace_back(makeUnique<CLinuxDMABUFParamsResource>(makeUnique<CZwpLinuxBufferParamsV1>(r->client(), r->version(), id)));

        if UNLIKELY (!RESOURCE->good()) {
            r->noMemory();
            PROTO::linuxDma->m_params.pop_back();
            return;
        }
    });

    if (m_resource->version() < 4)
        sendMods();
}

bool CLinuxDMABUFResource::good() {
    return m_resource->resource();
}

void CLinuxDMABUFResource::sendMods() {
    for (auto const& fmt : PROTO::linuxDma->m_formatTable->m_rendererTranche.formats) {
        for (auto const& mod : fmt.modifiers) {
            if (m_resource->version() < 3) {
                if (mod == DRM_FORMAT_MOD_INVALID || mod == DRM_FORMAT_MOD_LINEAR)
                    m_resource->sendFormat(fmt.drmFormat);
                continue;
            }

            // TODO: https://gitlab.freedesktop.org/xorg/xserver/-/issues/1166

            m_resource->sendModifier(fmt.drmFormat, mod >> 32, mod & 0xFFFFFFFF);
        }
    }
}

CLinuxDMABufV1Protocol::CLinuxDMABufV1Protocol(const wl_interface* iface, const int& ver, const std::string& name) : IWaylandProtocol(iface, ver, name) {
    static auto P = g_pHookSystem->hookDynamic("ready", [this](void* self, SCallbackInfo& info, std::any d) {
        int  rendererFD = g_pCompositor->m_drmFD;
        auto dev        = devIDFromFD(rendererFD);

        if (!dev.has_value()) {
            LOGM(ERR, "failed to get drm dev, disabling linux dmabuf");
            removeGlobal();
            return;
        }

        m_mainDevice = *dev;

        SDMABUFTranche eglTranche = {
            .device  = m_mainDevice,
            .flags   = 0, // renderer isn't for ds so don't set flag.
            .formats = g_pHyprOpenGL->getDRMFormats(),
        };

        std::vector<std::pair<PHLMONITORREF, SDMABUFTranche>> tches;

        if (g_pCompositor->m_aqBackend->hasSession()) {
            // this assumes there's only 1 device used for both scanout and rendering
            // also that each monitor never changes its primary plane

            for (auto const& mon : g_pCompositor->m_monitors) {
                auto tranche = SDMABUFTranche{
                    .device  = m_mainDevice,
                    .flags   = ZWP_LINUX_DMABUF_FEEDBACK_V1_TRANCHE_FLAGS_SCANOUT,
                    .formats = mon->m_output->getRenderFormats(),
                };
                tches.emplace_back(std::make_pair<>(mon, tranche));
            }

            static auto monitorAdded = g_pHookSystem->hookDynamic("monitorAdded", [this](void* self, SCallbackInfo& info, std::any param) {
                auto pMonitor = std::any_cast<PHLMONITOR>(param);
                auto tranche  = SDMABUFTranche{
                     .device  = m_mainDevice,
                     .flags   = ZWP_LINUX_DMABUF_FEEDBACK_V1_TRANCHE_FLAGS_SCANOUT,
                     .formats = pMonitor->m_output->getRenderFormats(),
                };
                m_formatTable->m_monitorTranches.emplace_back(std::make_pair<>(pMonitor, tranche));
                resetFormatTable();
            });

            static auto monitorRemoved = g_pHookSystem->hookDynamic("monitorRemoved", [this](void* self, SCallbackInfo& info, std::any param) {
                auto pMonitor = std::any_cast<PHLMONITOR>(param);
                std::erase_if(m_formatTable->m_monitorTranches, [pMonitor](std::pair<PHLMONITORREF, SDMABUFTranche> pair) { return pair.first == pMonitor; });
                resetFormatTable();
            });
        }

        m_formatTable = makeUnique<CDMABUFFormatTable>(eglTranche, tches);

        drmDevice* device = nullptr;
        if (drmGetDeviceFromDevId(m_mainDevice, 0, &device) != 0) {
            LOGM(ERR, "failed to get drm dev, disabling linux dmabuf");
            removeGlobal();
            return;
        }

        if (device->available_nodes & (1 << DRM_NODE_RENDER)) {
            const char* name = device->nodes[DRM_NODE_RENDER];
            m_mainDeviceFD   = CFileDescriptor{open(name, O_RDWR | O_CLOEXEC)};
            drmFreeDevice(&device);
            if (!m_mainDeviceFD.isValid()) {
                LOGM(ERR, "failed to open drm dev, disabling linux dmabuf");
                removeGlobal();
                return;
            }
        } else {
            LOGM(ERR, "DRM device {} has no render node, disabling linux dmabuf checks", device->nodes[DRM_NODE_PRIMARY] ? device->nodes[DRM_NODE_PRIMARY] : "null");
            drmFreeDevice(&device);
        }
    });
}

void CLinuxDMABufV1Protocol::resetFormatTable() {
    if (!m_formatTable)
        return;

    LOGM(LOG, "Resetting format table");

    // this might be a big copy
    auto newFormatTable = makeUnique<CDMABUFFormatTable>(m_formatTable->m_rendererTranche, m_formatTable->m_monitorTranches);

    for (auto const& feedback : m_feedbacks) {
        feedback->m_resource->sendFormatTable(newFormatTable->m_tableFD.get(), newFormatTable->m_tableSize);
        if (feedback->m_lastFeedbackWasScanout) {
            PHLMONITOR mon;
            auto       HLSurface = CWLSurface::fromResource(feedback->m_surface);
            if (auto w = HLSurface->getWindow(); w)
                if (auto m = w->m_monitor.lock(); m)
                    mon = m->m_self.lock();

            if (!mon) {
                feedback->sendDefaultFeedback();
                return;
            }

            updateScanoutTranche(feedback->m_surface, mon);
        } else {
            feedback->sendDefaultFeedback();
        }
    }

    // delete old table after we sent new one
    m_formatTable = std::move(newFormatTable);
}

void CLinuxDMABufV1Protocol::bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id) {
    const auto& RESOURCE = m_managers.emplace_back(makeUnique<CLinuxDMABUFResource>(makeUnique<CZwpLinuxDmabufV1>(client, ver, id)));

    if UNLIKELY (!RESOURCE->good()) {
        wl_client_post_no_memory(client);
        m_managers.pop_back();
        return;
    }
}

void CLinuxDMABufV1Protocol::destroyResource(CLinuxDMABUFResource* resource) {
    std::erase_if(m_managers, [&](const auto& other) { return other.get() == resource; });
}

void CLinuxDMABufV1Protocol::destroyResource(CLinuxDMABUFFeedbackResource* resource) {
    std::erase_if(m_feedbacks, [&](const auto& other) { return other.get() == resource; });
}

void CLinuxDMABufV1Protocol::destroyResource(CLinuxDMABUFParamsResource* resource) {
    std::erase_if(m_params, [&](const auto& other) { return other.get() == resource; });
}

void CLinuxDMABufV1Protocol::destroyResource(CLinuxDMABuffer* resource) {
    std::erase_if(m_buffers, [&](const auto& other) { return other.get() == resource; });
}

void CLinuxDMABufV1Protocol::updateScanoutTranche(SP<CWLSurfaceResource> surface, PHLMONITOR pMonitor) {
    WP<CLinuxDMABUFFeedbackResource> feedbackResource;
    for (auto const& f : m_feedbacks) {
        if (f->m_surface != surface)
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
        feedbackResource->sendDefaultFeedback();
        return;
    }

    const auto& monitorTranchePair =
        std::ranges::find_if(m_formatTable->m_monitorTranches, [pMonitor](std::pair<PHLMONITORREF, SDMABUFTranche> pair) { return pair.first == pMonitor; });

    if (monitorTranchePair == m_formatTable->m_monitorTranches.end()) {
        LOGM(LOG, "updateScanoutTranche: monitor has no tranche");
        return;
    }

    auto& monitorTranche = (*monitorTranchePair).second;

    LOGM(LOG, "updateScanoutTranche: sending a scanout tranche");

    struct wl_array deviceArr = {
        .size = sizeof(m_mainDevice),
        .data = static_cast<void*>(&m_mainDevice),
    };
    feedbackResource->m_resource->sendMainDevice(&deviceArr);

    // prioritize scnaout tranche but have renderer fallback tranche
    // also yes formats can be duped here because different tranche flags (ds and no ds)
    feedbackResource->sendTranche(monitorTranche);
    feedbackResource->sendTranche(m_formatTable->m_rendererTranche);

    feedbackResource->m_resource->sendDone();

    feedbackResource->m_lastFeedbackWasScanout = true;
}
