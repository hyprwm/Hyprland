#pragma once

class CPointerConstraintState {
  public:
    enum eLifetime {
        LIFETIME_PERSISTENT,
        LIFETIME_ONESHOT,
    };

    explicit CPointerConstraintState(eLifetime lifetime = LIFETIME_PERSISTENT);

    bool activate();
    bool deactivate();
    bool active() const;
    bool exhausted() const;

  private:
    eLifetime m_lifetime;
    bool      m_active    = false;
    bool      m_exhausted = false;
};
