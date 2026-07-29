#pragma once

#include "../../desktop/DesktopTypes.hpp"
#include "../../helpers/math/Math.hpp"
#include "../Framebuffer.hpp"

#include <optional>

namespace Render {
    struct SBlurContext {
        std::optional<CBox> patternBox;
        PHLWINDOWREF        owner;
    };

    enum class eBlurType : uint8_t {
        BLUR_DUAL_KAWASE = 0,
        BLUR_FROST       = 1,
        BLUR_RIPPLE      = 2,
        BLUR_DROPS       = 3,
        BLUR_WATER       = 4,
        BLUR_FLUID_JAR   = 5,
    };

    // "Jaki kurwa provident????"
    class IBlurProvider {
      public:
        virtual ~IBlurProvider() = default;

        virtual eBlurType        type() const noexcept             = 0;
        virtual bool             isAnimated() const noexcept       = 0;
        virtual bool             requiresLiveBlur() const noexcept = 0;

        virtual void             expandDamage(CRegion& damage, float multiplier = 1.F) const                                                    = 0;
        virtual SP<IFramebuffer> blur(SP<IFramebuffer> source, float strength, const CRegion& originalDamage, const SBlurContext& context = {}) = 0;

      protected:
        IBlurProvider() = default;
    };
};
