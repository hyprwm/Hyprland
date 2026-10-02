#include "Provider.hpp"

#include "../GLFramebuffer.hpp"
#include "../../../debug/log/Logger.hpp"

using namespace Render;
using namespace Render::GL;

SP<IFramebuffer> IGLBlurProvider::blur(CRenderContext& ctx, SP<IFramebuffer> source, float strength, const CRegion& originalDamage, const SBlurContext& context) {
    const auto glSource = dynamicPointerCast<CGLFramebuffer>(source);
    RASSERT(glSource, "Tried to use a GL blur provider with a non-GL framebuffer");
    return blurGL(ctx, glSource, strength, originalDamage, context);
}
