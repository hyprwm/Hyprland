#pragma once

#include <chrono>

class CTimer {
  public:
    void                                         reset();
    float                                        getSeconds();
    int                                          getMillis();
    const std::chrono::steady_clock::time_point& chrono() const;

  private:
    std::chrono::steady_clock::time_point m_tpLastReset;

    std::chrono::steady_clock::duration   getDuration();
};