#pragma once

#include "Glass.hpp"

namespace Render::GL {
    class CPrismBlurProvider final : public CGlassBlurProvider {
      public:
        explicit CPrismBlurProvider(CHyprOpenGLImpl& impl);

      protected:
        bool requiresPreparedInput() const noexcept override;
    };
}
