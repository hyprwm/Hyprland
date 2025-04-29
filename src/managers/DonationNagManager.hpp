#pragma once

#include "../helpers/memory/Memory.hpp"

class CDonationNagManager {
  public:
    CDonationNagManager();

    // whether the donation nag was shown this boot.
    bool fired();

  private:
    struct SStateData {
        uint64_t epoch = 0;
        uint64_t major = 0;
    };

    SStateData getState();
    void       writeState(const SStateData& s);
    void       fire();

    bool       m_bFired = false;
};

inline UP<CDonationNagManager> g_pDonationNagManager;