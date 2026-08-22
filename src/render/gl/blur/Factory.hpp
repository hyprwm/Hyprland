#pragma once

#include "../../../helpers/memory/Memory.hpp"
#include "../../blur/Provider.hpp"

namespace Render::GL {
    class CHyprOpenGLImpl;
    class IGLBlurProvider;

    UP<IGLBlurProvider> createBlurProvider(eBlurType type, CHyprOpenGLImpl& impl);
}
