#pragma once

#include "./math/Math.hpp"
#include <array>

constexpr static int DAMAGE_RING_PREVIOUS_LEN = 2;

class CDamageRing {
  public:
    void    setSize(const Vector2D& size_);
    bool    damage(const CRegion& rg);
    void    damageEntire();
    void    rotate();
    CRegion getBufferDamage(int age);
    bool    hasChanged();

  private:
    Vector2D                                      size;
    CRegion                                       current;
    std::array<CRegion, DAMAGE_RING_PREVIOUS_LEN> previous;
    size_t                                        previousIdx = 0;
};
