#pragma once

#include "../defines.hpp"
#include "protocols/types/Buffer.hpp"

class CBufferReleaseManager {
  public:
    CBufferReleaseManager() = default;
    void add(PHLMONITORREF monitor, const CHLBufferReference& buf);
    void pageFlip(PHLMONITORREF monitor);
    void destroy(PHLMONITORREF monitor);

  private:
    std::unordered_map<PHLMONITORREF, std::vector<CHLBufferReference>> m_buffers;
};

inline UP<CBufferReleaseManager> g_pBufferReleaseManager;
