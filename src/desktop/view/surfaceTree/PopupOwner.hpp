#pragma once

#include "../../../helpers/memory/Memory.hpp"
#include "../../../helpers/math/Math.hpp"

#include <cstddef>

namespace Desktop::View {
    class CPopup;

    class CPopupOwner {
      public:
        virtual ~CPopupOwner();

        const SP<CPopup>& popupHead() const;
        size_t            popupTreeSize() const;
        size_t            popupMappedTreeSize() const;
        bool              hasPopupAt(const Vector2D& vec) const;

      protected:
        CPopupOwner();

        void setPopupHead(SP<CPopup> head);
        void resetPopupHead();

      private:
        SP<CPopup> m_popupHead;
    };
}
