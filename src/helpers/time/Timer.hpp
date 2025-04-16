#pragma once

#include "Time.hpp"

class CTimer {
  public:
    void                   reset();
    float                  getSeconds();
    float                  getMillis();
    const Time::steady_tp& chrono() const;

  private:
    Time::steady_tp  m_lastReset;

    Time::steady_dur getDuration();
};