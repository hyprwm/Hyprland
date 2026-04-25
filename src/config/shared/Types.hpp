#pragma once

#include "../../helpers/math/Math.hpp"

#include <string>

namespace Config {
    class ICustomConfigValueData;

    typedef int64_t                 INTEGER;
    typedef float                   FLOAT;
    typedef Vector2D                VEC2;
    typedef std::string             STRING;
    typedef ICustomConfigValueData* COMPLEX;
};