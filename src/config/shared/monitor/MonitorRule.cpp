#include "MonitorRule.hpp"

#include <cstring>

using namespace Config;

eMonitorRuleComparisonResult CMonitorRule::compare(const CMonitorRule& other) const {
    // hard props
    const auto SAME_RES          = m_resolution == other.m_resolution;
    const auto SAME_REFRESH      = m_refreshRate == other.m_refreshRate;
    const auto SAME_SCALE        = m_scale == other.m_scale; // scale is hard due to checks
    const auto SAME_BITNESS      = other.m_enable10bit == m_enable10bit;
    const auto SAME_DRM_MODELINE = !std::memcmp(&m_drmMode, &other.m_drmMode, sizeof(m_drmMode));

    if (!SAME_RES || !SAME_REFRESH || !SAME_SCALE || !SAME_BITNESS || !SAME_DRM_MODELINE)
        return COMPARISON_NO_MATCH;

    // Soft props
    const auto SAME_CM = other.m_cmType == m_cmType && other.m_sdrSaturation == m_sdrSaturation && other.m_sdrBrightness == m_sdrBrightness &&
        other.m_sdrMinLuminance == m_sdrMinLuminance && other.m_sdrMaxLuminance == m_sdrMaxLuminance && other.m_supportsWideColor == m_supportsWideColor &&
        other.m_supportsHDR == m_supportsHDR && other.m_minLuminance == m_minLuminance && other.m_maxLuminance == m_maxLuminance && other.m_maxAvgLuminance == m_maxAvgLuminance &&
        other.m_iccFile == m_iccFile;
    const auto SAME_POS       = m_offset == other.m_offset;
    const auto SAME_TRANSFORM = m_transform == other.m_transform;
    const auto SAME_AUTO_DIR  = m_autoDir == other.m_autoDir;
    const auto SAME_RESERVED  = m_reservedArea == other.m_reservedArea;
    const auto SAME_MIRROR    = m_mirrorOf == other.m_mirrorOf;

    if (!SAME_CM || !SAME_POS || !SAME_TRANSFORM || !SAME_AUTO_DIR || !SAME_RESERVED || !SAME_MIRROR)
        return COMPARISON_SOFT_MISMATCH;

    return COMPARISON_FULL_MATCH;
}
