#include "CornersData.hpp"

#include <format>

using namespace Config;

CCornersData::CCornersData(int64_t all) : CCornersData(all, all, all, all) {
    ;
}

CCornersData::CCornersData(int64_t topLeft, int64_t topRight, int64_t bottomRight, int64_t bottomLeft) :
    m_topLeft(topLeft), m_topRight(topRight), m_bottomRight(bottomRight), m_bottomLeft(bottomLeft) {
    ;
}

eConfigValueDataTypes CCornersData::getDataType() {
    return CVD_TYPE_CORNERS;
}

bool CCornersData::isUniform() const {
    return m_topLeft == m_topRight && m_topLeft == m_bottomRight && m_topLeft == m_bottomLeft;
}

std::string CCornersData::toString() const {
    return isUniform() ? std::to_string(m_topLeft) : std::format("{} {} {} {}", m_topLeft, m_topRight, m_bottomRight, m_bottomLeft);
}

std::string CCornersData::toJson() const {
    return isUniform() ? std::to_string(m_topLeft) : std::format("[{},{},{},{}]", m_topLeft, m_topRight, m_bottomRight, m_bottomLeft);
}
