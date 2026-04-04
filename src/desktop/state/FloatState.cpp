#include "FloatState.hpp"

using namespace Desktop;

void CFloatStateCache::remember(PHLWINDOW window, const Vector2D& size) {
    Log::logger->log(Log::DEBUG, "[floatStateCache] storing floating size {}x{} for window {}::{}", size.x, size.y, window->m_initialClass, window->m_initialTitle);
    // true -> use m_initialClass and m_initialTitle
    SFloatCacheKey id{window, true};
    m_storedSizes[id] = size;
}

std::optional<Vector2D> CFloatStateCache::get(PHLWINDOW window) {
    // At startup, m_initialClass and m_initialTitle are undefined
    // and m_class and m_title are just "initial" ones.
    // false -> use m_class and m_title
    SFloatCacheKey id{window, false};
    Log::logger->log(Log::DEBUG, "[floatStateCache] Hash for window {}::{} = {}", window->m_class, window->m_title, id.hash);

    if (m_storedSizes.contains(id)) {
        Log::logger->log(Log::DEBUG, "[floatStateCache] got stored size {}x{} for window {}::{}", m_storedSizes[id].x, m_storedSizes[id].y, window->m_class, window->m_title);
        return m_storedSizes[id];
    }

    return std::nullopt;
}

UP<CFloatStateCache>& Desktop::floatState() {
    static UP<CFloatStateCache> p = makeUnique<CFloatStateCache>();
    return p;
}
