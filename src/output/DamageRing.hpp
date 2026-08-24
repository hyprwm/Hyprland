#pragma once

#include "../helpers/math/Math.hpp"
#include <array>

namespace Monitor {
    constexpr static int   DAMAGE_RING_PREVIOUS_LEN   = 3;
    constexpr static int   DAMAGE_RING_MAX_RECTS      = 8;
    constexpr static float DAMAGE_RING_EXTENTS_FACTOR = 2.F;

    class CDamageRing {
      public:
        class CTransaction {
          public:
            CTransaction(const CTransaction&)            = delete;
            CTransaction& operator=(const CTransaction&) = delete;
            CTransaction(CTransaction&& other) noexcept;
            CTransaction& operator=(CTransaction&& other) noexcept;
            ~CTransaction();

            CRegion getBufferDamage(int age);
            void    commit();
            void    rollback();

          private:
            friend class CDamageRing;

            CTransaction(CDamageRing* ring, CRegion&& damage);

            CDamageRing* m_ring = nullptr;
            CRegion      m_damage;
        };

        void         setSize(const Vector2D& size_);
        bool         damage(const CBox& box);
        bool         damage(const CRegion& rg);
        void         damageEntire();
        CTransaction beginTransaction();
        void         rotate();
        CRegion      getBufferDamage(int age);
        bool         hasChanged();

      private:
        void                                          rotateDamage(const CRegion& damage);
        CRegion                                       getBufferDamageFor(const CRegion& current, int age);

        Vector2D                                      m_size;
        CRegion                                       m_current;
        std::array<CRegion, DAMAGE_RING_PREVIOUS_LEN> m_previous;
        size_t                                        m_previousIdx = 0;
    };
}
