#include "DamageRing.hpp"
#include "../config/ConfigManager.hpp"

CRegion CDamageRing::getDamage(int age) {
    if (age <= 0 || age - 1 > DAMAGE_RING_LENGTH)
        return CRegion{0, 0, m_vBounds.x, m_vBounds.y};

    CRegion toReturn = m_aBlurDamageFrames[0];
    toReturn.add(m_aBlurDamageFrames[1]);

    static auto* const PBLURENABLED = &g_pConfigManager->getConfigValuePtr("decoration:blur:enabled")->intValue;
    static auto* const PBLURSIZE    = &g_pConfigManager->getConfigValuePtr("decoration:blur:size")->intValue;
    static auto* const PBLURPASSES  = &g_pConfigManager->getConfigValuePtr("decoration:blur:passes")->intValue;
    const auto         BLURRADIUS   = *PBLURPASSES > 10 ? pow(2, 15) : std::clamp(*PBLURSIZE, (int64_t)1, (int64_t)40) * pow(2, *PBLURPASSES);

    if (*PBLURENABLED)
        wlr_region_expand(toReturn.pixman(), toReturn.pixman(), BLURRADIUS);

    toReturn.add(m_aDamageFrames[0]);
    toReturn.add(m_aDamageFrames[1]);

    return toReturn;
}

void CDamageRing::setBounds(const Vector2D& max) {
    m_vBounds = max;
}

void CDamageRing::damageEntire() {
    m_aDamageFrames[0].add({0, 0, m_vBounds.x, m_vBounds.y});
}

void CDamageRing::addDamage(const CRegion& damage, bool blur) {
    if (blur)
        m_aBlurDamageFrames[0].add(damage);
    else
        m_aDamageFrames[0].add(damage);
}

void CDamageRing::rotate() {
    m_aBlurDamageFrames[1] = m_aBlurDamageFrames[0];
    m_aDamageFrames[1]     = m_aDamageFrames[0];

    m_aBlurDamageFrames[0].clear();
    m_aDamageFrames[0].clear();
}

bool CDamageRing::empty() {
    return m_aBlurDamageFrames[0].empty() && m_aBlurDamageFrames[1].empty() && m_aDamageFrames[0].empty() && m_aDamageFrames[1].empty();
}