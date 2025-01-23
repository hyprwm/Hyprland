#pragma once

#include "../helpers/memory/Memory.hpp"

class CDonationNagManager {
  public:
    CDonationNagManager();

    // whether the donation nag was shown this boot.
    bool fired();

  private:
    bool m_bFired = false;
};

inline UP<CDonationNagManager> g_pDonationNagManager;