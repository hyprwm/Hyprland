#include "DamageRing.hpp"

void CDamageRing::setSize(const Vector2D& size_) {
    if (size_ == size)
        return;

    size = size_;

    damageEntire();
}

bool CDamageRing::damage(const CRegion& rg) {
    CRegion clipped = rg.copy().intersect(CBox{{}, size});
    if (clipped.empty())
        return false;

    current.add(clipped);
    return true;
}

void CDamageRing::damageEntire() {
    damage(CBox{{}, size});
}

void CDamageRing::rotate() {
    previousIdx = (previousIdx + DAMAGE_RING_PREVIOUS_LEN - 1) % DAMAGE_RING_PREVIOUS_LEN;

    previous[previousIdx] = current;
    current.clear();
}

CRegion CDamageRing::getBufferDamage(int age) {
    if (age <= 0 || age > DAMAGE_RING_PREVIOUS_LEN + 1)
        return CBox{{}, size};

    CRegion damage = current;

    for (int i = 0; i < age - 1; ++i) {
        int j = (previousIdx + i) % DAMAGE_RING_PREVIOUS_LEN;
        damage.add(previous.at(j));
    }

    // don't return a ludicrous amount of rects
    if (damage.getRects().size() > 8)
        return damage.getExtents();

    return damage;
}

bool CDamageRing::hasChanged() {
    return !current.empty();
}
