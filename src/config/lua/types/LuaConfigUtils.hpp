#pragma once

#include "LuaConfigValue.hpp"
#include "../../../helpers/memory/Memory.hpp"
#include "../../values/types/IValue.hpp"

namespace Config::Lua {
    UP<ILuaConfigValue> fromGenericValue(SP<Config::Values::IValue> v);
};
