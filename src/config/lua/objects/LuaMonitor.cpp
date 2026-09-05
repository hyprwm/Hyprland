#include "LuaMonitor.hpp"
#include "LuaWorkspace.hpp"
#include "../../../helpers/string/StringUtils.hpp"
#include "../../../state/WorkspaceState.hpp"
#include "LuaObjectHelpers.hpp"

#include "../bindings/LuaBindingsInternal.hpp"
#include "../../../output/Monitor.hpp"
#include "../../../desktop/state/FocusState.hpp"
#include "../../../state/WorkspacePlacementController.hpp"

#include <string_view>

using namespace Config::Lua;
using namespace Config::Lua::Bindings;

static constexpr const char* MT = "HL.Monitor";

//
static int monitorEq(lua_State* L) {
    const auto* lhs = sc<PHLMONITORREF*>(luaL_checkudata(L, 1, MT));
    const auto* rhs = sc<PHLMONITORREF*>(luaL_checkudata(L, 2, MT));

    lua_pushboolean(L, lhs->lock() == rhs->lock());
    return 1;
}

static int monitorToString(lua_State* L) {
    const auto* ref = sc<PHLMONITORREF*>(luaL_checkudata(L, 1, MT));
    const auto  mon = ref->lock();

    if (!mon)
        lua_pushstring(L, "HL.Monitor(expired)");
    else
        lua_pushfstring(L, "HL.Monitor(%d:%s)", mon->m_id, mon->m_name.c_str());

    return 1;
}

static int monitorSetWorkspace(lua_State* L) {
    auto*      ref      = sc<PHLMONITORREF*>(luaL_checkudata(L, 1, MT));
    const auto selector = Internal::workspaceSelectorFromLuaSelectorOrObject(L, 2, "HLMonitor.set_workspace");

    if (!selector)
        return 0;

    const auto& [id, name, _] = getWorkspaceIDNameFromString(*selector, ref->lock());
    if (id == WORKSPACE_INVALID || State::workspaceState()->isSpecial(id))
        return 0;

    auto ws = State::workspaceState()->query().id(id).run();
    if (!ws)
        ws = State::workspaceState()->create(id, (*ref)->m_id, name);

    State::workspacePlacementController()->moveWorkspaceToMonitor(ws, ref->lock(), true, false);
    (*ref)->changeWorkspace(ws, false, true, Desktop::focusState()->monitor() != *ref);

    return 0;
}

static int monitorSetSpecialWorkspace(lua_State* L) {
    auto*                      ref = sc<PHLMONITORREF*>(luaL_checkudata(L, 1, MT));
    std::optional<std::string> selector;
    if (lua_isstring(L, 2) || lua_isnumber(L, 2))
        selector = std::format("special:{}", Internal::argStr(L, 2));
    else
        selector = Internal::workspaceSelectorFromLuaSelectorOrObject(L, 2, "HLMonitor.set_special_workspace");

    if (!selector) {
        (*ref)->setSpecialWorkspace(WORKSPACE_INVALID, true);
        return 0;
    }

    const auto& [id, name, _] = getWorkspaceIDNameFromString(*selector, ref->lock());
    if (id == WORKSPACE_INVALID || !State::workspaceState()->isSpecial(id))
        return 0;

    auto ws = State::workspaceState()->query().id(id).run();
    if (!ws)
        ws = State::workspaceState()->create(id, (*ref)->m_id, name);

    (*ref)->setSpecialWorkspace(ws->m_id, true);

    return 0;
}

static int monitorIndex(lua_State* L) {
    auto*      ref = sc<PHLMONITORREF*>(luaL_checkudata(L, 1, MT));
    const auto mon = ref->lock();
    if (!mon) {
        LOG(Log::DEBUG, "[lua] Tried to access an expired object");
        lua_pushnil(L);
        return 1;
    }

    const std::string_view key = luaL_checkstring(L, 2);

    if (key == "id")
        lua_pushinteger(L, sc<lua_Integer>(mon->m_id));
    else if (key == "name")
        lua_pushstring(L, mon->m_name.c_str());
    else if (key == "description")
        lua_pushstring(L, mon->m_shortDescription.c_str());
    else if (key == "serial")
        lua_pushstring(L, mon->m_output->serial.c_str());
    else if (key == "width")
        lua_pushinteger(L, sc<int>(mon->m_pixelSize.x));
    else if (key == "height")
        lua_pushinteger(L, sc<int>(mon->m_pixelSize.y));
    else if (key == "physical_width")
        lua_pushinteger(L, sc<int>(mon->m_output->physicalSize.x));
    else if (key == "physical_height")
        lua_pushinteger(L, sc<int>(mon->m_output->physicalSize.y));
    else if (key == "refresh_rate")
        lua_pushnumber(L, mon->m_refreshRate);
    else if (key == "x")
        lua_pushinteger(L, sc<int>(mon->m_position.x));
    else if (key == "y")
        lua_pushinteger(L, sc<int>(mon->m_position.y));
    else if (key == "active_workspace") {
        if (mon->m_activeWorkspace)
            Objects::CLuaWorkspace::push(L, mon->m_activeWorkspace);
        else
            lua_pushnil(L);
    } else if (key == "active_special_workspace") {
        if (mon->m_activeSpecialWorkspace)
            Objects::CLuaWorkspace::push(L, mon->m_activeSpecialWorkspace);
        else
            lua_pushnil(L);
    } else if (key == "position") {
        lua_newtable(L);
        lua_pushinteger(L, sc<int>(mon->m_position.x));
        lua_setfield(L, -2, "x");
        lua_pushinteger(L, sc<int>(mon->m_position.y));
        lua_setfield(L, -2, "y");
    } else if (key == "size") {
        lua_newtable(L);
        lua_pushinteger(L, sc<int>(mon->m_pixelSize.x));
        lua_setfield(L, -2, "width");
        lua_pushinteger(L, sc<int>(mon->m_pixelSize.y));
        lua_setfield(L, -2, "height");
    } else if (key == "mode") {
        lua_newtable(L);
        lua_pushinteger(L, sc<int>(mon->m_size.x));
        lua_setfield(L, -2, "width");
        lua_pushinteger(L, sc<int>(mon->m_size.y));
        lua_setfield(L, -2, "height");
        lua_pushnumber(L, sc<float>(mon->m_refreshRate));
        lua_setfield(L, -2, "refresh_rate");
    } else if (key == "scale")
        lua_pushnumber(L, mon->m_scale);
    else if (key == "transform")
        lua_pushinteger(L, sc<int>(mon->m_transform));
    else if (key == "dpms_status")
        lua_pushboolean(L, mon->m_dpmsStatus);
    else if (key == "enabled")
        lua_pushboolean(L, mon->enabled());
    else if (key == "vrr_active")
        lua_pushboolean(L, mon->m_vrrActive);
    else if (key == "is_mirror")
        lua_pushboolean(L, mon->isMirror());
    else if (key == "mirrors") {
        lua_newtable(L);

        int i = 1;
        for (const auto& mirrorRef : mon->m_mirrors) {
            const auto mirror = mirrorRef.lock();
            if (!mirror)
                continue;

            Objects::CLuaMonitor::push(L, mirror);
            lua_rawseti(L, -2, i++);
        }
    } else if (key == "available_modes") {
        lua_newtable(L);

        int i = 1;
        for (const auto& mode : mon->m_output->modes) {
            if (!mode)
                continue;

            lua_newtable(L);
            lua_pushinteger(L, sc<int>(mode->pixelSize.x));
            lua_setfield(L, -2, "width");
            lua_pushinteger(L, sc<int>(mode->pixelSize.y));
            lua_setfield(L, -2, "height");
            lua_pushnumber(L, mode->refreshRate / 1000.0);
            lua_setfield(L, -2, "refresh_rate");
            lua_pushboolean(L, mode->preferred);
            lua_setfield(L, -2, "preferred");
            lua_rawseti(L, -2, i++);
        }
    } else if (key == "focused")
        lua_pushboolean(L, mon == Desktop::focusState()->monitor());
    else if (key == "cm")
        lua_pushstring(L, NCMType::toString(mon->m_cmType).c_str());
    else if (key == "reserved") {
        lua_newtable(L);
        lua_pushnumber(L, mon->m_reservedArea.top());
        lua_setfield(L, -2, "top");
        lua_pushnumber(L, mon->m_reservedArea.right());
        lua_setfield(L, -2, "right");
        lua_pushnumber(L, mon->m_reservedArea.bottom());
        lua_setfield(L, -2, "bottom");
        lua_pushnumber(L, mon->m_reservedArea.left());
        lua_setfield(L, -2, "left");
    } else if (key == "hardware_details") {
        lua_newtable(L);
        lua_pushstring(L, StringUtils::backendStr(mon->m_output->getBackend()->type()).c_str());
        lua_setfield(L, -2, "backend");
        lua_pushboolean(L, mon->m_output->parsedEDID.hdrMetadata.has_value());
        lua_setfield(L, -2, "hdr");
        lua_pushboolean(L, mon->m_output->parsedEDID.chromaticityCoords.has_value());
        lua_setfield(L, -2, "chroma");
        lua_pushboolean(L, mon->m_output->parsedEDID.supportsBT2020);
        lua_setfield(L, -2, "bt2020");
        lua_pushboolean(L, mon->m_output->vrrCapable);
        lua_setfield(L, -2, "vrr_capable");
    }

    // Fns

    else if (key == "set_workspace")
        lua_pushcfunction(L, monitorSetWorkspace);
    else if (key == "set_special_workspace")
        lua_pushcfunction(L, monitorSetSpecialWorkspace);
    else
        lua_pushnil(L);

    return 1;
}

void Objects::CLuaMonitor::setup(lua_State* L) {
    registerMetatable(L, MT, monitorIndex, gcRef<PHLMONITORREF>, monitorEq, monitorToString);
}

void Objects::CLuaMonitor::push(lua_State* L, PHLMONITORREF mon) {
    if (!mon) {
        lua_pushnil(L);
        return;
    }

    new (lua_newuserdata(L, sizeof(PHLMONITORREF))) PHLMONITORREF(mon ? mon->m_self : nullptr);
    luaL_getmetatable(L, MT);
    lua_setmetatable(L, -2);
}
