#pragma once

#include "../../desktop/DesktopTypes.hpp"
#include "../../helpers/math/Math.hpp"
#include "../Framebuffer.hpp"

#include <optional>

namespace Render {
    class CRenderingContext;

    struct SBlurShape {
        CBox  box;
        float radius        = 0.F;
        float roundingPower = 2.F;
    };

    struct SBlurContext {
        std::optional<CBox>       patternBox;
        PHLWINDOWREF              owner;
        std::optional<SBlurShape> shape;
    };

    enum class eBlurType : uint8_t {
        BLUR_DUAL_KAWASE  = 0,
        BLUR_FROST        = 1,
        BLUR_RIPPLE       = 2,
        BLUR_DROPS        = 3,
        BLUR_WATER        = 4,
        BLUR_FLUID_JAR    = 5,
        BLUR_PRISM        = 6,
        BLUR_HEAT_SHIMMER = 7,
        BLUR_ACRYLIC      = 8,
        BLUR_AURORA       = 9,
        BLUR_HAZE         = 10,
    };

    // "Jaki kurwa provident????"
    class IBlurProvider {
      public:
        virtual ~IBlurProvider() = default;

        virtual eBlurType        type() const noexcept                                       = 0;
        virtual bool             isAnimated(const CRenderingContext& context) const noexcept = 0;
        virtual bool             requiresLiveBlur() const noexcept                           = 0;

        virtual void             expandDamage(CRegion& damage, float multiplier = 1.F) const = 0;
        virtual SP<IFramebuffer> blur(CRenderingContext& renderingContext, SP<IFramebuffer> source, float strength, const CRegion& originalDamage,
                                      const SBlurContext& blurContext = {})                  = 0;

      protected:
        IBlurProvider() = default;
    };
};
