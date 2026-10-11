#include "PointerConstraintState.hpp"

CPointerConstraintState::CPointerConstraintState(eLifetime lifetime) : m_lifetime(lifetime) {
    ;
}

bool CPointerConstraintState::activate() {
    if (m_active || m_exhausted)
        return false;

    m_active = true;
    return true;
}

bool CPointerConstraintState::deactivate() {
    if (!m_active)
        return false;

    m_active    = false;
    m_exhausted = m_lifetime == LIFETIME_ONESHOT;
    return true;
}

bool CPointerConstraintState::active() const {
    return m_active;
}

bool CPointerConstraintState::exhausted() const {
    return m_exhausted;
}
