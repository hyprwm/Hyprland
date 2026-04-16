#pragma once

#include "./math/Math.hpp"
#include <array>
#include <chrono>

constexpr static int DAMAGE_RING_PREVIOUS_LEN = 3;

class CDamageRing {
  public:
    using hrc = std::chrono::high_resolution_clock;

    void            setSize(const Vector2D& size_);
    bool            damage(const CRegion& rg);
    void            damageEntire();
    void            rotate();
    CRegion         getBufferDamage(int age);
    bool            hasChanged();
    hrc::time_point lastDamageTime() const;
    hrc::time_point lastRotationTime() const;

  private:
    Vector2D                                      m_size;
    CRegion                                       m_current;
    std::array<CRegion, DAMAGE_RING_PREVIOUS_LEN> m_previous;
    size_t                                        m_previousIdx = 0;
    hrc::time_point                               m_lastDamageTime;
    hrc::time_point                               m_lastRotationTime;
};
