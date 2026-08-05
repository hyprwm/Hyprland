#include "WindowBackend.hpp"

#include <algorithm>

using namespace Desktop::View;

IWindowBackend::IWindowBackend() = default;

IWindowBackend::~IWindowBackend() = default;

bool IWindowBackend::isX11() const {
    return type() == eBackendType::WINDOW_BACKEND_X11;
}

void CWindowConfigureAckTracker::add(uint32_t serial, const Vector2D& size) {
    m_pending.emplace_back(serial, size);
}

std::optional<Vector2D> CWindowConfigureAckTracker::acknowledge(uint32_t serial) {
    const auto ACK = std::find_if(m_pending.rbegin(), m_pending.rend(), [serial](const auto& configure) { return configure.first <= serial; });
    if (ACK == m_pending.rend())
        return std::nullopt;

    const auto ACKED_SERIAL = ACK->first;
    const auto ACKED_SIZE   = ACK->second;
    std::erase_if(m_pending, [ACKED_SERIAL](const auto& configure) { return configure.first <= ACKED_SERIAL; });
    return ACKED_SIZE;
}

bool CWindowConfigureAckTracker::empty() const {
    return m_pending.empty();
}
