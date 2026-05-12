#include "LuaBindingsInternal.hpp"

#include "../objects/LuaNotification.hpp"
#include "../../shared/parserUtils/ParserUtils.hpp"

#include "../../../helpers/MiscFunctions.hpp"
#include "../../../notification/NotificationOverlay.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <optional>
#include <string>

using namespace Config;
using namespace Config::Lua;
using namespace Config::Lua::Bindings;

static std::optional<eIcons> iconFromString(std::string iconName) {
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

static std::optional<eIcons> parseIconArg(lua_State* L, int idx) {
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

static std::optional<CHyprColor> parseColorArg(lua_State* L, int idx) {
    if (lua_isnumber(L, idx))
        return CHyprColor(sc<uint64_t>(lua_tonumber(L, idx)));

    if (lua_isstring(L, idx)) {
        auto parsed = ParserUtils::parseColor(lua_tostring(L, idx));
        if (!parsed)
            return std::nullopt;

        return CHyprColor(sc<uint64_t>(*parsed));
    }

    return std::nullopt;
}

static int hlNotificationCreate(lua_State* L) {
    if (!lua_istable(L, 1))
        return Internal::configError(L, "hl.notification.create: expected a table { text, duration, icon?, color?, font_size? }");

    const auto            text = Internal::requireTableFieldStr(L, 1, "text", "hl.notification.create");

    std::optional<double> duration = Internal::tableOptNum(L, 1, "duration");
    if (!duration)
        duration = Internal::tableOptNum(L, 1, "timeout");
    if (!duration)
        duration = Internal::tableOptNum(L, 1, "time");

    if (!duration)
        return Internal::configError(L, "hl.notification.create: 'duration' (or 'timeout' / 'time') is required");
    if (*duration < 0)
        return Internal::configError(L, "hl.notification.create: duration must be >= 0");

    eIcons icon = ICON_NONE;
    lua_getfield(L, 1, "icon");
    if (!lua_isnil(L, -1)) {
        const auto parsedIcon = parseIconArg(L, -1);
        if (!parsedIcon)
            return Internal::configError(L, "hl.notification.create: invalid 'icon' (expected icon name or number)");

        icon = *parsedIcon;
    }
    lua_pop(L, 1);

    CHyprColor color = CHyprColor(0);
    lua_getfield(L, 1, "color");
    if (!lua_isnil(L, -1)) {
        const auto parsedColor = parseColorArg(L, -1);
        if (!parsedColor)
            return Internal::configError(L, "hl.notification.create: invalid 'color' (expected color string or number)");

        color = *parsedColor;
    }
    lua_pop(L, 1);

    float fontSize = 13.F;
    if (const auto parsedFontSize = Internal::tableOptNum(L, 1, "font_size"); parsedFontSize.has_value()) {
        if (*parsedFontSize <= 0)
            return Internal::configError(L, "hl.notification.create: 'font_size' must be > 0");

        fontSize = sc<float>(*parsedFontSize);
    }

    auto notification = Notification::overlay()->addNotification(text, color, sc<float>(*duration), icon, fontSize);
    Objects::CLuaNotification::push(L, notification);
    return 1;
}

static int hlGetNotifications(lua_State* L) {
    lua_newtable(L);

    const auto notifications = Notification::overlay()->getNotifications();
    int        i             = 1;
    for (const auto& notification : notifications) {
        Objects::CLuaNotification::push(L, notification);
        lua_rawseti(L, -2, i++);
    }

    return 1;
}

void Internal::registerNotificationBindings(lua_State* L) {
    lua_newtable(L);
    Internal::setFn(L, "create", hlNotificationCreate);
    Internal::setFn(L, "get", hlGetNotifications);
    lua_setfield(L, -2, "notification");
}
