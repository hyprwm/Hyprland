#pragma once

#include "../../types/MultiAnimatedVariable.hpp"

#include <cstdint>
#include <optional>

namespace Desktop::View {
    class IAlphaModifiable {
      public:
        virtual ~IAlphaModifiable() = default;

        enum eAlphaModifiableProp : uint8_t {
            ALPHA_MODIFIABLE_FADE = 0,

            ALPHA_MODIFIABLE_LAST,
        };

        virtual Types::CMultiAVarContainer<float, uint8_t>& alpha()                                   = 0;
        virtual std::optional<uint8_t>                      alphaGenericToKey(eAlphaModifiableProp p) = 0;

      protected:
        IAlphaModifiable() = default;
    };
};
