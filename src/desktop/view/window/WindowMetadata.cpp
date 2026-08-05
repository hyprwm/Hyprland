#include "WindowMetadata.hpp"

using namespace Desktop::View;

static uint64_t windowIDCounter = 0x18000000;

CWindowMetadata::CWindowMetadata() : m_stableID(windowIDCounter++) {
    ;
}

const std::string& CWindowMetadata::title() const {
    return m_title;
}

const std::string& CWindowMetadata::appID() const {
    return m_appID;
}

const std::string& CWindowMetadata::initialTitle() const {
    return m_initialTitle;
}

const std::string& CWindowMetadata::initialAppID() const {
    return m_initialAppID;
}

uint64_t CWindowMetadata::stableID() const {
    return m_stableID;
}

void CWindowMetadata::initializeOnFirstMap(const std::string& title, const std::string& appID) {
    m_title        = title;
    m_initialTitle = m_title;
    m_initialAppID = appID;
}

bool CWindowMetadata::updateTitle(const std::string& title) {
    if (m_title == title)
        return false;

    m_title = title;
    return true;
}

bool CWindowMetadata::updateAppID(const std::string& appID) {
    if (m_appID == appID)
        return false;

    m_appID = appID;
    return true;
}
