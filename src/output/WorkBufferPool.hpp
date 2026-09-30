#pragma once

#include "../helpers/Format.hpp"
#include "../helpers/cm/ColorManagement.hpp"
#include "../helpers/memory/Memory.hpp"
#include "../helpers/time/Time.hpp"
#include <functional>
#include <optional>
#include <vector>

class CEventLoopTimer;
namespace Render {
    class IFramebuffer;
}

namespace Monitor {
    class CWorkBufferPool {
      public:
        static constexpr uint64_t BYTE_LIMIT = 256ULL * 1024ULL * 1024ULL;

        CWorkBufferPool(std::function<SP<Render::IFramebuffer>()> createBuffer, std::function<void()> prepareRelease, uint64_t byteLimit = BYTE_LIMIT);
        ~CWorkBufferPool();

        CWorkBufferPool(const CWorkBufferPool&)               = delete;
        CWorkBufferPool& operator=(const CWorkBufferPool&)    = delete;
        CWorkBufferPool(CWorkBufferPool&&)                    = delete;
        CWorkBufferPool&         operator=(CWorkBufferPool&&) = delete;

        SP<Render::IFramebuffer> acquire(const Vector2D& size, DRMFormat format, NColorManagement::PImageDescription imageDescription);
        void                     setImageDescription(NColorManagement::PImageDescription imageDescription);
        void                     forEachUnused(const std::function<void(SP<Render::IFramebuffer>)>& callback);

      private:
        struct SResource {
            SP<Render::IFramebuffer>       buffer;
            uint64_t                       bytes = 0;
            std::optional<Time::steady_tp> idleSince;
        };

        void                                      armTimer();
        void                                      expire();

        std::function<SP<Render::IFramebuffer>()> m_createBuffer;
        std::function<void()>                     m_prepareRelease;
        uint64_t                                  m_byteLimit = BYTE_LIMIT;
        std::vector<SResource>                    m_resources;
        SP<CEventLoopTimer>                       m_expiryTimer;

        friend class CWorkBufferPoolTest;
    };
}
