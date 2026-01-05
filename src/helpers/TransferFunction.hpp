#pragma once

#include <cstdint>
#include <string>

namespace NTransferFunction {
    enum eTF : uint8_t {
        TF_DEFAULT        = 0,
        TF_AUTO           = 1,
        TF_SRGB           = 2,
        TF_GAMMA22        = 3,
        TF_FORCED_GAMMA22 = 4,
    };

    eTF         fromString(const std::string tfName);
    std::string toString(eTF tf);

    eTF         fromConfig();
}
