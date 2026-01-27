#pragma once

#include "./math/Math.hpp"
#include <array>

constexpr static int DAMAGE_RING_PREVIOUS_LEN = 3;

class CDamageRing {
  public:
    void    setSize(const Vector2D& size_);
    bool    damage(const CRegion& rg);
    void    damageEntire();
    void    rotate();
    CRegion getBufferDamage(int age);
    bool    hasChanged();

  private:
    Vector2D                                      m_size;
    CRegion                                       m_current;
    std::array<CRegion, DAMAGE_RING_PREVIOUS_LEN> m_previous;
    size_t                                        m_previousIdx = 0;
};
