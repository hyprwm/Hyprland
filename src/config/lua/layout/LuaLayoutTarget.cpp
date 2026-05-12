#include "LuaLayoutTarget.hpp"

#include "LuaLayoutContext.hpp"
#include "../bindings/LuaBindingsInternal.hpp"
#include "../objects/LuaObjectHelpers.hpp"
#include "../objects/LuaWindow.hpp"

#include "../../../layout/target/Target.hpp"

#include <string_view>

using namespace Config::Lua;
using namespace Config::Lua::Layouts;

static constexpr const char* TARGET_MT = "HL.LayoutTarget";

namespace {
    struct SLuaLayoutTargetRef {
        WP<Layout::ITarget> target;
        size_t              index = 0;
    };
}

static int layoutTargetPlace(lua_State* L) {
    auto* ref    = sc<SLuaLayoutTargetRef*>(luaL_checkudata(L, 1, TARGET_MT));
    auto  target = ref->target.lock();
    if (!target)
        return 0;

    CBox box;
    if (!boxFromTable(L, 2, box))
        return Bindings::Internal::configError(L, "HL.LayoutTarget: place expects a box table { x, y, w, h }");

    target->setPositionGlobal(box.noNegativeSize());
    return 0;
}

static int layoutTargetIndex(lua_State* L) {
    auto*                  ref = sc<SLuaLayoutTargetRef*>(luaL_checkudata(L, 1, TARGET_MT));
    const std::string_view key = luaL_checkstring(L, 2);

    auto                   target = ref->target.lock();
    if (!target) {
        lua_pushnil(L);
        return 1;
    }

    if (key == "index")
        lua_pushinteger(L, sc<lua_Integer>(ref->index));
    else if (key == "window") {
        const auto window = target->window();
        if (window)
            Objects::CLuaWindow::push(L, window);
        else
            lua_pushnil(L);
    } else if (key == "box")
        pushBox(L, target->position());
    else if (key == "place" || key == "set_box")
        lua_pushcfunction(L, layoutTargetPlace);
    else
        lua_pushnil(L);

    return 1;
}

static int layoutTargetGc(lua_State* L) {
    sc<SLuaLayoutTargetRef*>(lua_touserdata(L, 1))->~SLuaLayoutTargetRef();
    return 0;
}

static int layoutTargetToString(lua_State* L) {
    auto* ref    = sc<SLuaLayoutTargetRef*>(luaL_checkudata(L, 1, TARGET_MT));
    auto  target = ref->target.lock();
    if (!target)
        lua_pushstring(L, "HL.LayoutTarget(expired)");
    else
        lua_pushfstring(L, "HL.LayoutTarget(%d)", sc<int>(ref->index));
    return 1;
}

void Config::Lua::Layouts::setupLayoutTarget(lua_State* L) {
    luaL_newmetatable(L, TARGET_MT);
    lua_pushcfunction(L, layoutTargetIndex);
    lua_setfield(L, -2, "__index");
    lua_pushcfunction(L, Objects::readOnlyNewIndex);
    lua_setfield(L, -2, "__newindex");
    lua_pushcfunction(L, layoutTargetGc);
    lua_setfield(L, -2, "__gc");
    lua_pushcfunction(L, layoutTargetToString);
    lua_setfield(L, -2, "__tostring");
    lua_pop(L, 1);
}

void Config::Lua::Layouts::pushLayoutTarget(lua_State* L, const SP<Layout::ITarget>& target, size_t index) {
    new (lua_newuserdata(L, sizeof(SLuaLayoutTargetRef))) SLuaLayoutTargetRef{target, index};
    luaL_getmetatable(L, TARGET_MT);
    lua_setmetatable(L, -2);
}
