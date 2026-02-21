#include "Renderbuffer.hpp"
#include "Framebuffer.hpp"
#include "render/Renderer.hpp"
#include <hyprutils/memory/SharedPtr.hpp>
#include <hyprutils/signal/Listener.hpp>
#include <hyprutils/signal/Signal.hpp>

#include <dlfcn.h>

IRenderbuffer::IRenderbuffer(SP<Aquamarine::IBuffer> buffer, uint32_t format) : m_hlBuffer(buffer) {
    m_listeners.destroyBuffer = buffer->events.destroy.listen([this] { g_pHyprRenderer->onRenderbufferDestroy(this); });
}

bool IRenderbuffer::good() {
    return m_good;
}

SP<IFramebuffer> IRenderbuffer::getFB() {
    return m_framebuffer;
}
