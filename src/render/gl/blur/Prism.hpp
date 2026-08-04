#pragma once

#include "Glass.hpp"

namespace Render::GL {
    class CPrismBlurMaterial final : public CGlassBlurMaterial {
      public:
        CPrismBlurMaterial();
    };

    class CPrismBlurProvider final : public CGlassBlurProvider {
      public:
        explicit CPrismBlurProvider(CHyprOpenGLImpl& impl);
    };
}
