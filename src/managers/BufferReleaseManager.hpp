#pragma once

#include "../defines.hpp"
#include "protocols/types/Buffer.hpp"

class CBufferReleaseManager {
  public:
    CBufferReleaseManager() = default;
    bool addBuffer(PHLMONITORREF monitor, const CHLBufferReference& buf);
    void addFence(PHLMONITORREF monitor);
    void dropBuffers(PHLMONITORREF monitor);
    void destroy(PHLMONITORREF monitor);

  private:
    std::unordered_map<PHLMONITORREF, std::vector<CHLBufferReference>> m_buffers;
};

inline UP<CBufferReleaseManager> g_pBufferReleaseManager;
