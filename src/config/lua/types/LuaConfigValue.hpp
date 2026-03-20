#pragma once

#include <cstdint>
#include <typeinfo>

#include "../../shared/Types.hpp"

extern "C" {
#include <lua.h>
}

namespace Config::Lua {
    enum eParseError : uint8_t {
        PARSE_ERROR_OK = 0,
        PARSE_ERROR_OUT_OF_RANGE,
        PARSE_ERROR_BAD_VALUE,
        PARSE_ERROR_BAD_TYPE,
    };

    struct SParseError {
        eParseError errorCode = PARSE_ERROR_OK;
        std::string message;
    };

    class ILuaConfigValue {
      protected:
        ILuaConfigValue() = default;
        bool m_bSetByUser = false;

      public:
        virtual ~ILuaConfigValue() = default;

        virtual SParseError           parse(lua_State* s) = 0;
        virtual const std::type_info* underlying()        = 0;
        virtual void const*           data()              = 0;
        virtual std::string           toString()          = 0;

        virtual void                  resetSetByUser();
        virtual bool                  setByUser();
    };
};