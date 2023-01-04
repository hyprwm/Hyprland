#include "WobblyModel.hpp"

#include "../debug/Log.hpp"

CWobblyModel::CWobblyModel(CWindow* window)
{
    m_pWindow = window;
}

void CWobblyModel::notifyGrab(const Vector2D& position) {
    Debug::log(LOG, "notifyGrab: %f, %f", position.x, position.y);
}

void CWobblyModel::notifyUngrab() {
    Debug::log(LOG, "notifyUngrab");
}

void CWobblyModel::notifyMove(const Vector2D &delta) {
    Debug::log(LOG, "notifyMove: %f, %f", delta.x, delta.y);
}

void CWobblyModel::notifyResize(const Vector2D &delta) {
    Debug::log(LOG, "notifyResize: %f, %f", delta.x, delta.y);
}
