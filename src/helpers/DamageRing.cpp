#include "DamageRing.hpp"

void CDamageRing::setSize(const Vector2D& size_) {
    if (size_ == m_size)
        return;

    m_size = size_;

    damageEntire();
}

bool CDamageRing::damage(const CRegion& rg) {
    CRegion clipped = rg.copy().intersect(CBox{{}, m_size});
    if (clipped.empty())
        return false;

    m_current.add(clipped);
    return true;
}

void CDamageRing::damageEntire() {
    damage(CBox{{}, m_size});
}

void CDamageRing::rotate() {
    m_previousIdx = (m_previousIdx + DAMAGE_RING_PREVIOUS_LEN - 1) % DAMAGE_RING_PREVIOUS_LEN;

    m_previous[m_previousIdx] = m_current;
    m_current.clear();
}

CRegion CDamageRing::getBufferDamage(int age) {
    if (age <= 0 || age > DAMAGE_RING_PREVIOUS_LEN + 1)
        return CBox{{}, m_size};

    CRegion damage = m_current;

    for (int i = 0; i < age - 1; ++i) {
        int j = (m_previousIdx + i) % DAMAGE_RING_PREVIOUS_LEN;
        damage.add(m_previous.at(j));
    }

    // don't return a ludicrous amount of rects
    if (damage.getRects().size() > 8)
        return damage.getExtents();

    return damage;
}

bool CDamageRing::hasChanged() {
    return !m_current.empty();
}
