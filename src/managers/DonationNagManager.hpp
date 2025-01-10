#pragma once

#include <memory>

class CDonationNagManager {
  public:
    CDonationNagManager();

    // whether the donation nag was shown this boot.
    bool fired();

  private:
    bool m_bFired = false;
};

inline std::unique_ptr<CDonationNagManager> g_pDonationNagManager;