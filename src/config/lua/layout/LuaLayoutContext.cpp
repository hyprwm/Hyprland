#include "LuaLayoutContext.hpp"

#include "LuaLayoutTarget.hpp"
#include "../bindings/LuaBindingsInternal.hpp"
#include "../bindings/Check.hpp"

#include <algorithm>
#include <cmath>
#include <string_view>

using namespace Config::Lua::Layouts;
using namespace Config::Lua;

void Config::Lua::Layouts::pushBox(lua_State* L, const CBox& box) {
    lua_newtable(L);
    lua_pushnumber(L, box.x);
    lua_setfield(L, -2, "x");
    lua_pushnumber(L, box.y);
    lua_setfield(L, -2, "y");
    lua_pushnumber(L, box.w);
    lua_setfield(L, -2, "w");
    lua_pushnumber(L, box.h);
    lua_setfield(L, -2, "h");
}

bool Config::Lua::Layouts::boxFromTable(lua_State* L, int idx, CBox& box) {
    idx = lua_absindex(L, idx);
    if (!lua_istable(L, idx))
        return false;

    auto getNum = [&](const char* key, double& out) -> bool {
        lua_getfield(L, idx, key);
        if (!lua_isnumber(L, -1)) {
            lua_pop(L, 1);
            return false;
        }
        out = lua_tonumber(L, -1);
        lua_pop(L, 1);
        return true;
    };

    return getNum("x", box.x) && getNum("y", box.y) && getNum("w", box.w) && getNum("h", box.h);
}

static CBox areaFromContext(lua_State* L, int idx) {
    idx = lua_absindex(L, idx);
    CBox area;
    lua_getfield(L, idx, "area");
    boxFromTable(L, -1, area);
    lua_pop(L, 1);
    return area;
}

static size_t targetCountFromContext(lua_State* L, int idx) {
    idx          = lua_absindex(L, idx);
    size_t count = 0;
    lua_getfield(L, idx, "targets");
    if (lua_istable(L, -1))
        count = lua_rawlen(L, -1);
    lua_pop(L, 1);
    return count;
}

static int ctxGridCell(lua_State* L) {
    const auto AREA = areaFromContext(L, 1);
    const auto i    = Bindings::Check::integer(L, 2);
    if (!i)
        return Bindings::Internal::configError(L, std::format("grid_cell: bad argument 2: {}", i.error()));

    const auto cols = Bindings::Check::integer(L, 3);
    if (!cols)
        return Bindings::Internal::configError(L, std::format("grid_cell: bad argument 3: {}", cols.error()));

    const int index = std::max(1, sc<int>(*i));
    const int width = std::max(1, sc<int>(*cols));
    int       rows  = 0;

    if (lua_gettop(L) >= 4 && lua_isnumber(L, 4))
        rows = std::max(1, sc<int>(lua_tointeger(L, 4)));
    else {
        const auto count = std::max<size_t>(1, targetCountFromContext(L, 1));
        rows             = std::max(1, sc<int>(std::ceil(sc<double>(count) / sc<double>(width))));
    }

    const int row = (index - 1) / width;
    const int col = (index - 1) % width;

    pushBox(L, CBox{AREA.x + AREA.w * col / width, AREA.y + AREA.h * row / rows, AREA.w / width, AREA.h / rows}.noNegativeSize());
    return 1;
}

static int ctxColumn(lua_State* L) {
    const auto AREA = areaFromContext(L, 1);
    const auto i    = Bindings::Check::integer(L, 2);
    if (!i)
        return Bindings::Internal::configError(L, std::format("column: bad argument 2: {}", i.error()));

    const auto n = Bindings::Check::integer(L, 3);
    if (!n)
        return Bindings::Internal::configError(L, std::format("column: bad argument 3: {}", n.error()));

    const int index = std::max(1, sc<int>(*i));
    const int count = std::max(1, sc<int>(*n));

    pushBox(L, CBox{AREA.x + AREA.w * (index - 1) / count, AREA.y, AREA.w / count, AREA.h}.noNegativeSize());
    return 1;
}

static int ctxRow(lua_State* L) {
    const auto AREA = areaFromContext(L, 1);
    const auto i    = Bindings::Check::integer(L, 2);
    if (!i)
        return Bindings::Internal::configError(L, std::format("row: bad argument 2: {}", i.error()));

    const auto n = Bindings::Check::integer(L, 3);
    if (!n)
        return Bindings::Internal::configError(L, std::format("row: bad argument 3: {}", n.error()));

    const int index = std::max(1, sc<int>(*i));
    const int count = std::max(1, sc<int>(*n));

    pushBox(L, CBox{AREA.x, AREA.y + AREA.h * (index - 1) / count, AREA.w, AREA.h / count}.noNegativeSize());
    return 1;
}

static int ctxSplit(lua_State* L) {
    CBox area;
    if (!boxFromTable(L, 2, area))
        return Bindings::Internal::configError(L, "ctx:split expects a box table as first argument");

    const auto side = Bindings::Check::string(L, 3);
    if (!side)
        return Bindings::Internal::configError(L, std::format("split: bad argument 1: {}", side.error()));
    const auto ratio = Bindings::Check::number(L, 4);
    if (!ratio)
        return Bindings::Internal::configError(L, std::format("split: bad argument 4: {}", ratio.error()));

    const double clampedRatio = std::clamp(*ratio, 0.0, 1.0);

    if (*side == "left")
        pushBox(L, CBox{area.x, area.y, area.w * clampedRatio, area.h}.noNegativeSize());
    else if (*side == "right")
        pushBox(L, CBox{area.x + area.w * (1.0 - clampedRatio), area.y, area.w * clampedRatio, area.h}.noNegativeSize());
    else if (*side == "top" || *side == "up")
        pushBox(L, CBox{area.x, area.y, area.w, area.h * clampedRatio}.noNegativeSize());
    else if (*side == "bottom" || *side == "down")
        pushBox(L, CBox{area.x, area.y + area.h * (1.0 - clampedRatio), area.w, area.h * clampedRatio}.noNegativeSize());
    else
        return Bindings::Internal::configError(L, "ctx:split side must be left, right, top, or bottom");

    return 1;
}

void Config::Lua::Layouts::pushLayoutContext(lua_State* L, const std::vector<SP<Layout::ITarget>>& targets, const CBox& area) {
    lua_newtable(L);

    pushBox(L, area);
    lua_setfield(L, -2, "area");

    lua_newtable(L);
    int i = 1;
    for (const auto& target : targets) {
        pushLayoutTarget(L, target, i);
        lua_rawseti(L, -2, i++);
    }
    lua_setfield(L, -2, "targets");

    lua_pushcfunction(L, ctxGridCell);
    lua_setfield(L, -2, "grid_cell");
    lua_pushcfunction(L, ctxColumn);
    lua_setfield(L, -2, "column");
    lua_pushcfunction(L, ctxRow);
    lua_setfield(L, -2, "row");
    lua_pushcfunction(L, ctxSplit);
    lua_setfield(L, -2, "split");
}
