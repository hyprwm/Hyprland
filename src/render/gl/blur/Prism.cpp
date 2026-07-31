#include "Prism.hpp"

#include "../../ShaderLoader.hpp"

using namespace Render;
using namespace Render::GL;

CPrismBlurMaterial::CPrismBlurMaterial() : CGlassBlurMaterial(eBlurType::BLUR_PRISM, SH_FRAG_PRISMFINISH, true) {
    ;
}

CPrismBlurProvider::CPrismBlurProvider(CHyprOpenGLImpl& impl) : CGlassBlurProvider(impl, makeUnique<CPrismBlurMaterial>()) {
    ;
}
