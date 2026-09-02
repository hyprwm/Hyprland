#include "FloatState.hpp"

using namespace Desktop;

void CFloatStateCache::remember(PHLWINDOW window, const Vector2D& size) {
    LOG(Log::DEBUG, "[floatStateCache] storing floating size {}x{} for window {}::{}", size.x, size.y, window->metadata().initialAppID(), window->metadata().initialTitle());
    // true -> use initial app ID and title
    SFloatCacheKey id{window, true};
    m_storedSizes[id] = size;
}

std::optional<Vector2D> CFloatStateCache::get(PHLWINDOW window) {
    // At startup, initial app ID and title are undefined
    // and current app ID and title are just "initial" ones.
    // false -> use current app ID and title
    SFloatCacheKey id{window, false};
    LOG(Log::DEBUG, "[floatStateCache] Hash for window {}::{} = {}", window->metadata().appID(), window->metadata().title(), id.hash);

    if (m_storedSizes.contains(id)) {
        LOG(Log::DEBUG, "[floatStateCache] got stored size {}x{} for window {}::{}", m_storedSizes[id].x, m_storedSizes[id].y, window->metadata().appID(),
            window->metadata().title());
        return m_storedSizes[id];
    }

    return std::nullopt;
}

UP<CFloatStateCache>& Desktop::floatState() {
    static UP<CFloatStateCache> p = makeUnique<CFloatStateCache>();
    return p;
}
