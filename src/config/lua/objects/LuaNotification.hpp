#pragma once

#include "LuaObjectHelpers.hpp"
#include "../../../notification/NotificationOverlay.hpp"

namespace Config::Lua::Objects {
    class CLuaNotification : public ILuaObjectWrapper {
      public:
        void        setup(lua_State* L) override;
        static void push(lua_State* L, const SP<Notification::CNotification>& notification);
    };
}
