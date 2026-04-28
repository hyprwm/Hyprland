#include "LuaNotification.hpp"

#include "../../../helpers/MiscFunctions.hpp"

#include <array>
#include <algorithm>
#include <cctype>
#include <optional>
#include <string_view>

using namespace Config::Lua;

static constexpr const char* MT = "HL.Notification";

namespace {
    struct SNotificationRef {
        WP<Notification::CNotification> notification;
        bool                            paused = false;
    };

    std::optional<CHyprColor> parseColor(lua_State* L, int idx) {
        if (lua_isnumber(L, idx))
            return CHyprColor(sc<uint64_t>(lua_tonumber(L, idx)));

        if (lua_isstring(L, idx)) {
            auto parsed = configStringToInt(lua_tostring(L, idx));
            if (!parsed)
                return std::nullopt;

            return CHyprColor(sc<uint64_t>(*parsed));
        }

        return std::nullopt;
    }

    std::optional<eIcons> iconFromString(std::string iconName) {
        std::ranges::transform(iconName, iconName.begin(), [](const unsigned char c) { return std::tolower(c); });

        static constexpr std::array<std::pair<const char*, eIcons>, 10> ICON_NAMES = {
            std::pair{"warning", ICON_WARNING}, std::pair{"warn", ICON_WARNING}, std::pair{"info", ICON_INFO},         std::pair{"hint", ICON_HINT},
            std::pair{"error", ICON_ERROR},     std::pair{"err", ICON_ERROR},    std::pair{"confused", ICON_CONFUSED}, std::pair{"question", ICON_CONFUSED},
            std::pair{"ok", ICON_OK},           std::pair{"none", ICON_NONE},
        };

        for (const auto& [name, icon] : ICON_NAMES) {
            if (name == iconName)
                return icon;
        }

        return std::nullopt;
    }

    std::optional<eIcons> parseIcon(lua_State* L, int idx) {
        if (lua_isnumber(L, idx)) {
            const auto raw = sc<int>(lua_tonumber(L, idx));
            if (raw >= ICON_WARNING && raw <= ICON_NONE)
                return sc<eIcons>(raw);

            return std::nullopt;
        }

        if (lua_isstring(L, idx))
            return iconFromString(lua_tostring(L, idx));

        return std::nullopt;
    }
}

static int notificationEq(lua_State* L) {
    const auto* lhs = sc<SNotificationRef*>(luaL_checkudata(L, 1, MT));
    const auto* rhs = sc<SNotificationRef*>(luaL_checkudata(L, 2, MT));

    lua_pushboolean(L, lhs->notification.lock() == rhs->notification.lock());
    return 1;
}

static int notificationToString(lua_State* L) {
    const auto* ref          = sc<SNotificationRef*>(luaL_checkudata(L, 1, MT));
    const auto  notification = ref->notification.lock();

    if (!notification)
        lua_pushstring(L, "HL.Notification(expired)");
    else
        lua_pushfstring(L, "HL.Notification(%p)", notification.get());

    return 1;
}

static int notificationGC(lua_State* L) {
    auto* ref = sc<SNotificationRef*>(luaL_checkudata(L, 1, MT));

    if (ref->paused) {
        if (const auto notification = ref->notification.lock(); notification)
            notification->unlock();
    }

    ref->~SNotificationRef();
    return 0;
}

static int notificationPause(lua_State* L) {
    auto*      ref = sc<SNotificationRef*>(luaL_checkudata(L, 1, MT));

    const auto notification = ref->notification.lock();
    if (!notification)
        return 0;

    if (ref->paused)
        return 0;

    notification->lock();
    ref->paused = true;
    return 0;
}

static int notificationResume(lua_State* L) {
    auto*      ref = sc<SNotificationRef*>(luaL_checkudata(L, 1, MT));

    const auto notification = ref->notification.lock();
    if (!notification)
        return 0;

    if (!ref->paused)
        return 0;

    notification->unlock();
    ref->paused = false;
    return 0;
}

static int notificationSetPaused(lua_State* L) {
    luaL_checkudata(L, 1, MT);
    luaL_checktype(L, 2, LUA_TBOOLEAN);

    if (lua_toboolean(L, 2))
        return notificationPause(L);

    return notificationResume(L);
}

static int notificationIsPaused(lua_State* L) {
    auto*      ref = sc<SNotificationRef*>(luaL_checkudata(L, 1, MT));

    const auto notification = ref->notification.lock();
    if (!notification) {
        lua_pushnil(L);
        return 1;
    }

    lua_pushboolean(L, notification->isLocked());
    return 1;
}

static int notificationSetText(lua_State* L) {
    auto*       ref  = sc<SNotificationRef*>(luaL_checkudata(L, 1, MT));
    const auto* text = luaL_checkstring(L, 2);

    if (const auto notification = ref->notification.lock(); notification)
        notification->setText(text);

    return 0;
}

static int notificationSetTimeout(lua_State* L) {
    auto*      ref = sc<SNotificationRef*>(luaL_checkudata(L, 1, MT));

    const auto timeoutMs = sc<float>(luaL_checknumber(L, 2));
    if (timeoutMs < 0.F)
        return Config::Lua::Bindings::Internal::configError(L, "HL.Notification:set_timeout: timeout must be >= 0");

    if (const auto notification = ref->notification.lock(); notification)
        notification->resetTimeout(timeoutMs);

    return 0;
}

static int notificationSetColor(lua_State* L) {
    auto*      ref = sc<SNotificationRef*>(luaL_checkudata(L, 1, MT));

    const auto color = parseColor(L, 2);
    if (!color)
        return Config::Lua::Bindings::Internal::configError(L, "HL.Notification:set_color: expected a color string or number");

    if (const auto notification = ref->notification.lock(); notification)
        notification->setColor(*color);

    return 0;
}

static int notificationSetIcon(lua_State* L) {
    auto*      ref = sc<SNotificationRef*>(luaL_checkudata(L, 1, MT));

    const auto icon = parseIcon(L, 2);
    if (!icon)
        return Config::Lua::Bindings::Internal::configError(L, "HL.Notification:set_icon: expected an icon name or number");

    if (const auto notification = ref->notification.lock(); notification)
        notification->setIcon(*icon);

    return 0;
}

static int notificationSetFontSize(lua_State* L) {
    auto*      ref = sc<SNotificationRef*>(luaL_checkudata(L, 1, MT));

    const auto fontSize = sc<float>(luaL_checknumber(L, 2));
    if (fontSize <= 0.F)
        return Config::Lua::Bindings::Internal::configError(L, "HL.Notification:set_font_size: font size must be > 0");

    if (const auto notification = ref->notification.lock(); notification)
        notification->setFontSize(fontSize);

    return 0;
}

static int notificationDismiss(lua_State* L) {
    auto* ref = sc<SNotificationRef*>(luaL_checkudata(L, 1, MT));

    if (const auto notification = ref->notification.lock(); notification)
        Notification::overlay()->dismissNotification(notification);

    return 0;
}

static int notificationGetText(lua_State* L) {
    auto*      ref = sc<SNotificationRef*>(luaL_checkudata(L, 1, MT));

    const auto notification = ref->notification.lock();
    if (!notification) {
        lua_pushnil(L);
        return 1;
    }

    lua_pushstring(L, notification->text().c_str());
    return 1;
}

static int notificationGetTimeout(lua_State* L) {
    auto*      ref = sc<SNotificationRef*>(luaL_checkudata(L, 1, MT));

    const auto notification = ref->notification.lock();
    if (!notification) {
        lua_pushnil(L);
        return 1;
    }

    lua_pushnumber(L, notification->timeMs());
    return 1;
}

static int notificationGetColor(lua_State* L) {
    auto*      ref = sc<SNotificationRef*>(luaL_checkudata(L, 1, MT));

    const auto notification = ref->notification.lock();
    if (!notification) {
        lua_pushnil(L);
        return 1;
    }

    lua_pushinteger(L, sc<lua_Integer>(notification->color().getAsHex()));
    return 1;
}

static int notificationGetIcon(lua_State* L) {
    auto*      ref = sc<SNotificationRef*>(luaL_checkudata(L, 1, MT));

    const auto notification = ref->notification.lock();
    if (!notification) {
        lua_pushnil(L);
        return 1;
    }

    lua_pushinteger(L, sc<lua_Integer>(notification->icon()));
    return 1;
}

static int notificationGetFontSize(lua_State* L) {
    auto*      ref = sc<SNotificationRef*>(luaL_checkudata(L, 1, MT));

    const auto notification = ref->notification.lock();
    if (!notification) {
        lua_pushnil(L);
        return 1;
    }

    lua_pushnumber(L, notification->fontSize());
    return 1;
}

static int notificationGetElapsed(lua_State* L) {
    auto*      ref = sc<SNotificationRef*>(luaL_checkudata(L, 1, MT));

    const auto notification = ref->notification.lock();
    if (!notification) {
        lua_pushnil(L);
        return 1;
    }

    lua_pushnumber(L, notification->timeElapsedMs());
    return 1;
}

static int notificationGetElapsedSinceCreation(lua_State* L) {
    auto*      ref = sc<SNotificationRef*>(luaL_checkudata(L, 1, MT));

    const auto notification = ref->notification.lock();
    if (!notification) {
        lua_pushnil(L);
        return 1;
    }

    lua_pushnumber(L, notification->timeElapsedSinceCreationMs());
    return 1;
}

static int notificationIsAlive(lua_State* L) {
    auto* ref = sc<SNotificationRef*>(luaL_checkudata(L, 1, MT));

    lua_pushboolean(L, ref->notification.lock().get() != nullptr);
    return 1;
}

static int notificationIndex(lua_State* L) {
    luaL_checkudata(L, 1, MT);
    const std::string_view key = luaL_checkstring(L, 2);

    if (key == "pause")
        lua_pushcfunction(L, notificationPause);
    else if (key == "resume")
        lua_pushcfunction(L, notificationResume);
    else if (key == "set_paused")
        lua_pushcfunction(L, notificationSetPaused);
    else if (key == "is_paused")
        lua_pushcfunction(L, notificationIsPaused);
    else if (key == "set_text")
        lua_pushcfunction(L, notificationSetText);
    else if (key == "set_timeout")
        lua_pushcfunction(L, notificationSetTimeout);
    else if (key == "set_color")
        lua_pushcfunction(L, notificationSetColor);
    else if (key == "set_icon")
        lua_pushcfunction(L, notificationSetIcon);
    else if (key == "set_font_size")
        lua_pushcfunction(L, notificationSetFontSize);
    else if (key == "dismiss")
        lua_pushcfunction(L, notificationDismiss);
    else if (key == "get_text")
        lua_pushcfunction(L, notificationGetText);
    else if (key == "get_timeout")
        lua_pushcfunction(L, notificationGetTimeout);
    else if (key == "get_color")
        lua_pushcfunction(L, notificationGetColor);
    else if (key == "get_icon")
        lua_pushcfunction(L, notificationGetIcon);
    else if (key == "get_font_size")
        lua_pushcfunction(L, notificationGetFontSize);
    else if (key == "get_elapsed")
        lua_pushcfunction(L, notificationGetElapsed);
    else if (key == "get_elapsed_since_creation")
        lua_pushcfunction(L, notificationGetElapsedSinceCreation);
    else if (key == "is_alive")
        lua_pushcfunction(L, notificationIsAlive);
    else
        lua_pushnil(L);

    return 1;
}

void Objects::CLuaNotification::setup(lua_State* L) {
    registerMetatable(L, MT, notificationIndex, notificationGC, notificationEq, notificationToString);
}

void Objects::CLuaNotification::push(lua_State* L, const SP<Notification::CNotification>& notification) {
    new (lua_newuserdata(L, sizeof(SNotificationRef))) SNotificationRef{.notification = WP<Notification::CNotification>(notification)};
    luaL_getmetatable(L, MT);
    lua_setmetatable(L, -2);
}
