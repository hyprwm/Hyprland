#include "PopupOwner.hpp"

#include "../Popup.hpp"

#include <utility>

using namespace Desktop::View;

CPopupOwner::CPopupOwner() {
    ;
}

CPopupOwner::~CPopupOwner() = default;

const SP<CPopup>& CPopupOwner::popupHead() const {
    return m_popupHead;
}

void CPopupOwner::setPopupHead(SP<CPopup> head) {
    m_popupHead = std::move(head);
}

void CPopupOwner::resetPopupHead() {
    m_popupHead.reset();
}

size_t CPopupOwner::popupTreeSize() const {
    return m_popupHead ? m_popupHead->allChildrenCount() : 0;
}

size_t CPopupOwner::popupMappedTreeSize() const {
    return m_popupHead ? m_popupHead->allMappedChildrenCount() : 0;
}

bool CPopupOwner::hasPopupAt(const Vector2D& vec) const {
    if (popupMappedTreeSize() == 0)
        return false;

    auto popup = popupHead()->at(vec);

    return popup && popup->wlSurface() && popup->wlSurface()->resource();
}
