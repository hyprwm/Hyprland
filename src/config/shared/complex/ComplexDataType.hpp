#pragma once

#include <cstdint>
#include <string>

namespace Config {
    enum eConfigValueDataTypes : int8_t {
        CVD_TYPE_INVALID     = -1,
        CVD_TYPE_GRADIENT    = 0,
        CVD_TYPE_CSS_VALUE   = 1,
        CVD_TYPE_FONT_WEIGHT = 2,
    };

    class IComplexConfigValue {
      public:
        virtual ~IComplexConfigValue() = default;

        virtual eConfigValueDataTypes getDataType() = 0;

        virtual std::string           toString() const = 0;
    };
}