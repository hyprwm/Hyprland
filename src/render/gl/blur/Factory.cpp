#include "Factory.hpp"

#include "Drops.hpp"
#include "FluidJar.hpp"
#include "Glass.hpp"
#include "HeatShimmer.hpp"
#include "Kawase.hpp"
#include "Prism.hpp"
#include "Ripple.hpp"
#include "Water.hpp"
#include "../../ShaderLoader.hpp"
#include "../../../debug/log/Logger.hpp"

using namespace Render;
using namespace Render::GL;

UP<IGLBlurProvider> Render::GL::createBlurProvider(eBlurType type, CHyprOpenGLImpl& impl) {
    switch (type) {
        case eBlurType::BLUR_DUAL_KAWASE: return makeUnique<CDualKawaseBlurProvider>(impl);
        case eBlurType::BLUR_FROST: return makeUnique<CGlassBlurProvider>(impl, type, SH_FRAG_FROSTFINISH);
        case eBlurType::BLUR_RIPPLE: return makeUnique<CRippleBlurProvider>(impl);
        case eBlurType::BLUR_DROPS: return makeUnique<CDropsBlurProvider>(impl);
        case eBlurType::BLUR_WATER: return makeUnique<CWaterBlurProvider>(impl);
        case eBlurType::BLUR_FLUID_JAR: return makeUnique<CFluidJarBlurProvider>(impl);
        case eBlurType::BLUR_PRISM: return makeUnique<CPrismBlurProvider>(impl);
        case eBlurType::BLUR_HEAT_SHIMMER: return makeUnique<CHeatShimmerBlurProvider>(impl);
    }

    Log::logger->log(Log::ERR, "Unknown blur provider {}, falling back to dual Kawase", sc<uint8_t>(type));
    return makeUnique<CDualKawaseBlurProvider>(impl);
}
