#include "Provider.hpp"

#include "../GLFramebuffer.hpp"
#include "../../../debug/log/Logger.hpp"

using namespace Render;
using namespace Render::GL;

SP<IFramebuffer> IGLBlurProvider::blur(CRenderingContext& renderingContext, SP<IFramebuffer> source, float strength, const CRegion& originalDamage,
                                       const SBlurContext& blurContext) {
    const auto glSource = dynamicPointerCast<CGLFramebuffer>(source);
    RASSERT(glSource, "Tried to use a GL blur provider with a non-GL framebuffer");
    return blurGL(renderingContext, glSource, strength, originalDamage, blurContext);
}
