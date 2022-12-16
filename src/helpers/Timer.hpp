#pragma once

#include "../defines.hpp"

class CTimer {
  public:
    void  reset();
    float getSeconds();
    int   getMillis();

  private:
    std::chrono::system_clock::time_point m_tpLastReset;

    std::chrono::system_clock::duration   getDuration();
};