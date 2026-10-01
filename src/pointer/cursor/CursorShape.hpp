#pragma once

#include "../../helpers/math/Math.hpp"
#include "../../helpers/memory/Memory.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace Hyprcursor {
    struct SCursorShapeData;
}

namespace Pointer::Cursor {
    class CCursorBuffer;

    class CCursorShape {
      public:
        struct SFrame {
            SP<CCursorBuffer>         buffer;
            Vector2D                  hotspot;
            std::chrono::milliseconds delay{0};
        };

        explicit CCursorShape(const Hyprcursor::SCursorShapeData& data);

        bool                                     valid() const;
        const SFrame&                            frame(size_t index) const;
        size_t                                   frameAt(std::chrono::milliseconds elapsed) const;
        std::optional<std::chrono::milliseconds> timeUntilNextFrame(std::chrono::milliseconds elapsed) const;

      private:
        uint64_t              cyclePosition(std::chrono::milliseconds elapsed) const;

        std::vector<SFrame>   m_frames;
        std::vector<uint64_t> m_frameEnds;
        uint64_t              m_cycleDuration = 0;
    };
}
