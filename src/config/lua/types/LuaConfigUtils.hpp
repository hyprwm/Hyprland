#pragma once

#include "LuaConfigValue.hpp"
#include "../../../helpers/memory/Memory.hpp"
#include "../../values/types/IValue.hpp"

namespace Config::Lua {
    SP<ILuaConfigValue> fromGenericValue(SP<Config::Values::IValue> v);
};
