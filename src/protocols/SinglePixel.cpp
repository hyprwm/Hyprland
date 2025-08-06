#include "SinglePixel.hpp"
#include "../desktop/LayerSurface.hpp"
#include <limits>
#include "render/Renderer.hpp"

CSinglePixelBuffer::CSinglePixelBuffer(uint32_t id, wl_client* client, CHyprColor col_) {
    LOGM(LOG, "New single-pixel buffer with color 0x{:x}", col_.getAsHex());

    m_color = col_.getAsHex();

    g_pHyprRenderer->makeEGLCurrent();

    m_opaque = col_.a >= 1.F;

    m_texture = makeShared<CTexture>(DRM_FORMAT_ARGB8888, reinterpret_cast<uint8_t*>(&m_color), 4, Vector2D{1, 1});

    m_resource = CWLBufferResource::create(makeShared<CWlBuffer>(client, 1, id));

    m_success = m_texture->m_texID;

    size = {1, 1};

    if (!m_success)
        Debug::log(ERR, "Failed creating a single pixel texture: null texture id");
}

CSinglePixelBuffer::~CSinglePixelBuffer() {
    if (m_resource)
        m_resource->sendRelease();
}

Aquamarine::eBufferCapability CSinglePixelBuffer::caps() {
    return Aquamarine::eBufferCapability::BUFFER_CAPABILITY_DATAPTR;
}

Aquamarine::eBufferType CSinglePixelBuffer::type() {
    return Aquamarine::eBufferType::BUFFER_TYPE_SHM;
}

bool CSinglePixelBuffer::isSynchronous() {
    return true;
}

void CSinglePixelBuffer::update(const CRegion& damage) {
    ;
}

Aquamarine::SDMABUFAttrs CSinglePixelBuffer::dmabuf() {
    return {.success = false};
}

std::tuple<uint8_t*, uint32_t, size_t> CSinglePixelBuffer::beginDataPtr(uint32_t flags) {
    return {reinterpret_cast<uint8_t*>(&m_color), DRM_FORMAT_ARGB8888, 4};
}

void CSinglePixelBuffer::endDataPtr() {
    ;
}

bool CSinglePixelBuffer::good() {
    return m_resource->good();
}

CSinglePixelBufferResource::CSinglePixelBufferResource(uint32_t id, wl_client* client, CHyprColor color) {
    m_buffer = makeShared<CSinglePixelBuffer>(id, client, color);

    if UNLIKELY (!m_buffer->good())
        return;

    m_buffer->m_resource->m_buffer = m_buffer;

    m_listeners.bufferResourceDestroy = m_buffer->events.destroy.listen([this] {
        m_listeners.bufferResourceDestroy.reset();
        PROTO::singlePixel->destroyResource(this);
    });
}

bool CSinglePixelBufferResource::good() {
    return m_buffer->good();
}

CSinglePixelBufferManagerResource::CSinglePixelBufferManagerResource(UP<CWpSinglePixelBufferManagerV1>&& resource_) : m_resource(std::move(resource_)) {
    if UNLIKELY (!good())
        return;

    m_resource->setDestroy([this](CWpSinglePixelBufferManagerV1* r) { PROTO::singlePixel->destroyResource(this); });
    m_resource->setOnDestroy([this](CWpSinglePixelBufferManagerV1* r) { PROTO::singlePixel->destroyResource(this); });

    m_resource->setCreateU32RgbaBuffer([this](CWpSinglePixelBufferManagerV1* res, uint32_t id, uint32_t r, uint32_t g, uint32_t b, uint32_t a) {
        CHyprColor  color{r / static_cast<float>(std::numeric_limits<uint32_t>::max()), g / static_cast<float>(std::numeric_limits<uint32_t>::max()), b / static_cast<float>(std::numeric_limits<uint32_t>::max()),
                         a / static_cast<float>(std::numeric_limits<uint32_t>::max())};
        const auto& RESOURCE = PROTO::singlePixel->m_buffers.emplace_back(makeUnique<CSinglePixelBufferResource>(id, m_resource->client(), color));

        if UNLIKELY (!RESOURCE->good()) {
            res->noMemory();
            PROTO::singlePixel->m_buffers.pop_back();
            return;
        }
    });
}

bool CSinglePixelBufferManagerResource::good() {
    return m_resource->resource();
}

CSinglePixelProtocol::CSinglePixelProtocol(const wl_interface* iface, const int& ver, const std::string& name) : IWaylandProtocol(iface, ver, name) {
    ;
}

void CSinglePixelProtocol::bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id) {
    const auto& RESOURCE = m_managers.emplace_back(makeUnique<CSinglePixelBufferManagerResource>(makeUnique<CWpSinglePixelBufferManagerV1>(client, ver, id)));

    if UNLIKELY (!RESOURCE->good()) {
        wl_client_post_no_memory(client);
        m_managers.pop_back();
        return;
    }
}

void CSinglePixelProtocol::destroyResource(CSinglePixelBufferManagerResource* res) {
    std::erase_if(m_managers, [&](const auto& other) { return other.get() == res; });
}

void CSinglePixelProtocol::destroyResource(CSinglePixelBufferResource* surf) {
    std::erase_if(m_buffers, [&](const auto& other) { return other.get() == surf; });
}
