#include "DamageRing.hpp"

#include <hyprutils/memory/Casts.hpp>

#include <utility>

using namespace Monitor;
using namespace Hyprutils::Memory;

CDamageRing::CTransaction::CTransaction(CDamageRing* ring, CRegion&& damage) : m_ring(ring), m_damage(std::move(damage)) {}

CDamageRing::CTransaction::CTransaction(CTransaction&& other) noexcept : m_ring(std::exchange(other.m_ring, nullptr)), m_damage(std::move(other.m_damage)) {}

CDamageRing::CTransaction& CDamageRing::CTransaction::operator=(CTransaction&& other) noexcept {
    if (this == &other)
        return *this;

    rollback();
    m_ring   = std::exchange(other.m_ring, nullptr);
    m_damage = std::move(other.m_damage);
    return *this;
}

CDamageRing::CTransaction::~CTransaction() {
    rollback();
}

CRegion CDamageRing::CTransaction::getBufferDamage(int age) {
    if (!m_ring)
        return {};

    return m_ring->getBufferDamageFor(m_damage, age);
}

void CDamageRing::CTransaction::commit() {
    if (!m_ring)
        return;

    m_ring->rotateDamage(m_damage);
    m_ring = nullptr;
}

void CDamageRing::CTransaction::rollback() {
    if (!m_ring)
        return;

    m_ring->damage(m_damage);
    m_ring = nullptr;
}

void CDamageRing::setSize(const Vector2D& size_) {
    if (size_ == m_size)
        return;

    m_size = size_;

    damageEntire();
}

bool CDamageRing::damage(const CBox& box) {
    if (m_size.x <= 0 || m_size.y <= 0 || box.w <= 0 || box.h <= 0)
        return false;

    return damage(CRegion{box});
}

bool CDamageRing::damage(const CRegion& rg) {
    if (m_size.x <= 0 || m_size.y <= 0)
        return false;

    CRegion clipped = rg.copy().intersect(CBox{{}, m_size});
    if (clipped.empty())
        return false;

    m_current.add(clipped);
    return true;
}

void CDamageRing::damageEntire() {
    damage(CBox{{}, m_size});
}

CDamageRing::CTransaction CDamageRing::beginTransaction() {
    CRegion captured = std::move(m_current);
    m_current.clear();
    return {this, std::move(captured)};
}

void CDamageRing::rotate() {
    rotateDamage(m_current);
    m_current.clear();
}

void CDamageRing::rotateDamage(const CRegion& damage) {
    m_previousIdx = (m_previousIdx + DAMAGE_RING_PREVIOUS_LEN - 1) % DAMAGE_RING_PREVIOUS_LEN;

    m_previous[m_previousIdx] = damage;
}

CRegion CDamageRing::getBufferDamage(int age) {
    return getBufferDamageFor(m_current, age);
}

CRegion CDamageRing::getBufferDamageFor(const CRegion& current, int age) {
    if (m_size.x <= 0 || m_size.y <= 0)
        return {};

    if (age <= 0 || age > DAMAGE_RING_PREVIOUS_LEN + 1)
        return CBox{{}, m_size};

    CRegion damage = current;

    for (int i = 0; i < age - 1; ++i) {
        int j = (m_previousIdx + i) % DAMAGE_RING_PREVIOUS_LEN;
        damage.add(m_previous.at(j));
    }

    // don't return a ludicrous amount of rects
    if (pixman_region32_n_rects(damage.pixman()) > DAMAGE_RING_MAX_RECTS) {
        const auto EXTENTS = damage.getExtents();

        // but only when the bounding box is not much bigger than the rects it replaces.
        // scattered damage would otherwise shade most of the screen to save a few draws.
        double rectsArea = 0;
        damage.forEachRect([&rectsArea](const auto& RECT) { rectsArea += sc<double>(RECT.x2 - RECT.x1) * (RECT.y2 - RECT.y1); });

        if (EXTENTS.w * EXTENTS.h <= rectsArea * DAMAGE_RING_EXTENTS_FACTOR)
            return EXTENTS;
    }

    return damage;
}

bool CDamageRing::hasChanged() {
    return !m_current.empty();
}
