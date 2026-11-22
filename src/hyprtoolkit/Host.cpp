#include "Host.hpp"
#include "EventLoop.hpp"

#include <hyprtoolkit/core/EmbeddedBackend.hpp>

#include "../Compositor.hpp"
#include "../debug/log/Logger.hpp"
#include "../render/OpenGL.hpp"
#include "../render/Renderer.hpp"

using namespace Toolkit;

UP<CHost>& Toolkit::host() {
    static UP<CHost> p = makeUnique<CHost>();
    return p;
}

CHost::CHost() = default;

CHost::~CHost() {
    destroy();
}

void CHost::init() {
    if (m_backend)
        return;

    if (!g_pHyprRenderer || g_pHyprRenderer->type() != Render::IHyprRenderer::RT_GL) {
        LOG(Log::WARN, "[hyprtoolkit] the embedded backend requires the GL renderer, toolkit decorations are unavailable");
        return;
    }

    // the backend borrows the current context
    Render::GL::g_pHyprOpenGL->makeEGLCurrent();

    Hyprtoolkit::IBackend::SBackendCreationData common;
    common.pLogConnection = makeShared<Hyprutils::CLI::CLoggerConnection>(Log::logger->hu());
    common.pLogConnection->setLogLevel(Log::DEBUG);
    common.pLogConnection->setName("hyprtoolkit");

    m_eventLoop = makeShared<CEventLoop>(g_pCompositor->m_wlEventLoop);
    m_backend   = Hyprtoolkit::IEmbeddedBackend::create({.common = std::move(common), .eventLoop = m_eventLoop});

    if (!m_backend) {
        LOG(Log::ERR, "[hyprtoolkit] failed to create the embedded backend, toolkit decorations are unavailable");
        m_eventLoop.reset();
        return;
    }

    LOG(Log::DEBUG, "[hyprtoolkit] embedded backend ready");

    m_events.ready.emit();
}

void CHost::destroy() {
    if (!m_backend)
        return;

    m_events.destroy.emit();

    if (Render::GL::g_pHyprOpenGL)
        Render::GL::g_pHyprOpenGL->makeEGLCurrent();

    m_backend->destroy();
    m_backend.reset();
    m_eventLoop.reset();
}

SP<Hyprtoolkit::IEmbeddedBackend> CHost::backend() const {
    return m_backend;
}
