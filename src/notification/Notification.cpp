#include "Notification.hpp"
#include "../debug/log/Logger.hpp"

using namespace Notification;

CNotification::CNotification(std::string&& text, float timeout, CHyprColor color, eIcons icon, float fontSize) :
    m_createdAt(Time::steadyNow()), m_text(std::move(text)), m_color(color == CHyprColor(0) ? ICONS_COLORS[icon] : color), m_icon(icon), m_fontSize(fontSize),
    m_timeMs(std::max(timeout, 1.F)) {
    ;
}

void CNotification::setText(std::string&& text) {
    m_cache.textTex.reset();
    m_text = std::move(text);
}

void CNotification::setColor(CHyprColor color) {
    m_color = color;
}

void CNotification::setIcon(eIcons icon) {
    m_cache.iconTex.reset();
    m_icon = icon;
}

void CNotification::setFontSize(float fontSize) {
    m_cache    = {};
    m_fontSize = fontSize;
}

void CNotification::resetTimeout(float timeMs) {
    m_timeMs    = timeMs;
    m_startedAt = Time::steadyNow();
}

const std::string& CNotification::text() const {
    return m_text;
}

const CHyprColor& CNotification::color() const {
    return m_color;
}

eIcons CNotification::icon() const {
    return m_icon;
}

float CNotification::fontSize() const {
    return m_fontSize;
}

float CNotification::timeMs() const {
    return m_timeMs;
}

float CNotification::timeElapsedSinceCreationMs() const {
    return std::chrono::duration_cast<std::chrono::milliseconds>(Time::steadyNow() - m_createdAt).count();
}

float CNotification::timeElapsedMs() const {
    if (isLocked())
        return m_lastPauseElapsed;
    return std::chrono::duration_cast<std::chrono::milliseconds>(Time::steadyNow() - m_startedAt).count();
}

bool CNotification::isLocked() const {
    return m_pauseLocks > 0;
}

void CNotification::lock() {
    m_pauseLocks++;
    if (m_pauseLocks == 1)
        m_lastPauseElapsed = timeElapsedMs();
}

void CNotification::unlock() {
    if (m_pauseLocks == 0) {
        Log::logger->log(Log::ERR, "CNotification: BUG THIS: unlock on 0 locks??");
        return;
    }

    m_pauseLocks--;

    if (m_pauseLocks == 0)
        m_startedAt = Time::steadyNow() - std::chrono::milliseconds(sc<uint64_t>(m_lastPauseElapsed));
}

bool CNotification::gone() const {
    return !isLocked() && Time::steadyNow() > m_startedAt + std::chrono::milliseconds(sc<uint64_t>(m_timeMs));
}
