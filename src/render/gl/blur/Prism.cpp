#include "Prism.hpp"

#include "../../ShaderLoader.hpp"

using namespace Render;
using namespace Render::GL;

CPrismBlurProvider::CPrismBlurProvider(CHyprOpenGLImpl& impl) : CGlassBlurProvider(impl, eBlurType::BLUR_PRISM, SH_FRAG_PRISMFINISH) {
    ;
}

bool CPrismBlurProvider::requiresPreparedInput() const noexcept {
    return true;
}
