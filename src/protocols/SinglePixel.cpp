#include "SinglePixel.hpp"
#include "../desktop/LayerSurface.hpp"
#include <limits>
#include "render/Renderer.hpp"

CSinglePixelBuffer::CSinglePixelBuffer(uint32_t id, wl_client* client, CHyprColor col_) {
    LOGM(LOG, "New single-pixel buffer with color 0x{:x}", col_.getAsHex());

    color = col_.getAsHex();

    g_pHyprRenderer->makeEGLCurrent();

    opaque = col_.a >= 1.F;

    texture = makeShared<CTexture>(DRM_FORMAT_ARGB8888, (uint8_t*)&color, 4, Vector2D{1, 1});

    resource = CWLBufferResource::create(makeShared<CWlBuffer>(client, 1, id));

    success = texture->m_iTexID;

    size = {1, 1};

    if (!success)
        Debug::log(ERR, "Failed creating a single pixel texture: null texture id");
}

CSinglePixelBuffer::~CSinglePixelBuffer() {
    ;
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
    return {(uint8_t*)&color, DRM_FORMAT_ARGB8888, 4};
}

void CSinglePixelBuffer::endDataPtr() {
    ;
}

bool CSinglePixelBuffer::good() {
    return resource->good();
}

CSinglePixelBufferResource::CSinglePixelBufferResource(uint32_t id, wl_client* client, CHyprColor color) {
    buffer = makeShared<CSinglePixelBuffer>(id, client, color);

    if UNLIKELY (!buffer->good())
        return;

    buffer->resource->buffer = buffer;

    listeners.bufferResourceDestroy = buffer->events.destroy.registerListener([this](std::any d) {
        listeners.bufferResourceDestroy.reset();
        PROTO::singlePixel->destroyResource(this);
    });
}

CSinglePixelBufferResource::~CSinglePixelBufferResource() {
    ;
}

bool CSinglePixelBufferResource::good() {
    return buffer->good();
}

CSinglePixelBufferManagerResource::CSinglePixelBufferManagerResource(SP<CWpSinglePixelBufferManagerV1> resource_) : resource(resource_) {
    if UNLIKELY (!good())
        return;

    resource->setDestroy([this](CWpSinglePixelBufferManagerV1* r) { PROTO::singlePixel->destroyResource(this); });
    resource->setOnDestroy([this](CWpSinglePixelBufferManagerV1* r) { PROTO::singlePixel->destroyResource(this); });

    resource->setCreateU32RgbaBuffer([this](CWpSinglePixelBufferManagerV1* res, uint32_t id, uint32_t r, uint32_t g, uint32_t b, uint32_t a) {
        CHyprColor color{r / (float)std::numeric_limits<uint32_t>::max(), g / (float)std::numeric_limits<uint32_t>::max(), b / (float)std::numeric_limits<uint32_t>::max(),
                         a / (float)std::numeric_limits<uint32_t>::max()};
        const auto RESOURCE = PROTO::singlePixel->m_vBuffers.emplace_back(makeShared<CSinglePixelBufferResource>(id, resource->client(), color));

        if UNLIKELY (!RESOURCE->good()) {
            res->noMemory();
            PROTO::singlePixel->m_vBuffers.pop_back();
            return;
        }
    });
}

bool CSinglePixelBufferManagerResource::good() {
    return resource->resource();
}

CSinglePixelProtocol::CSinglePixelProtocol(const wl_interface* iface, const int& ver, const std::string& name) : IWaylandProtocol(iface, ver, name) {
    ;
}

void CSinglePixelProtocol::bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id) {
    const auto RESOURCE = m_vManagers.emplace_back(makeShared<CSinglePixelBufferManagerResource>(makeShared<CWpSinglePixelBufferManagerV1>(client, ver, id)));

    if UNLIKELY (!RESOURCE->good()) {
        wl_client_post_no_memory(client);
        m_vManagers.pop_back();
        return;
    }
}

void CSinglePixelProtocol::destroyResource(CSinglePixelBufferManagerResource* res) {
    std::erase_if(m_vManagers, [&](const auto& other) { return other.get() == res; });
}

void CSinglePixelProtocol::destroyResource(CSinglePixelBufferResource* surf) {
    std::erase_if(m_vBuffers, [&](const auto& other) { return other.get() == surf; });
}
