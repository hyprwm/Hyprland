#include "LuaWindow.hpp"
#include "LuaWorkspace.hpp"
#include "LuaMonitor.hpp"
#include "LuaObjectHelpers.hpp"

#include "../../../desktop/view/Window.hpp"
#include "../../../desktop/Workspace.hpp"
#include "../../../desktop/state/FocusState.hpp"
#include "../../../layout/algorithm/Algorithm.hpp"
#include "../../../layout/algorithm/tiled/master/MasterAlgorithm.hpp"
#include "../../../layout/algorithm/tiled/scrolling/ScrollingAlgorithm.hpp"
#include "../../../layout/algorithm/tiled/dwindle/DwindleAlgorithm.hpp"
#include "../../../layout/space/Space.hpp"
#include "../../../layout/supplementary/WorkspaceAlgoMatcher.hpp"

#include <format>
#include <string_view>

static constexpr const char* MT = "HL.Window";

static int                   windowIndex(lua_State* L) {
    auto*      ref = static_cast<PHLWINDOWREF*>(luaL_checkudata(L, 1, MT));
    const auto w   = ref->lock();
    if (!w) {
        Log::logger->log(Log::DEBUG, "[lua] Tried to access an expired object");
        lua_pushnil(L);
        return 1;
    }

    const std::string_view key = luaL_checkstring(L, 2);

    if (key == "address")
        lua_pushstring(L, std::format("0x{:x}", reinterpret_cast<uintptr_t>(w.get())).c_str());
    else if (key == "mapped")
        lua_pushboolean(L, w->m_isMapped);
    else if (key == "hidden")
        lua_pushboolean(L, w->isHidden());
    else if (key == "at") {
        lua_newtable(L);
        lua_pushinteger(L, static_cast<int>(w->m_realPosition->goal().x));
        lua_setfield(L, -2, "x");
        lua_pushinteger(L, static_cast<int>(w->m_realPosition->goal().y));
        lua_setfield(L, -2, "y");
    } else if (key == "size") {
        lua_newtable(L);
        lua_pushinteger(L, static_cast<int>(w->m_realSize->goal().x));
        lua_setfield(L, -2, "x");
        lua_pushinteger(L, static_cast<int>(w->m_realSize->goal().y));
        lua_setfield(L, -2, "y");
    } else if (key == "workspace") {
        if (w->m_workspace)
            Config::Lua::Objects::CLuaWorkspace::push(L, w->m_workspace);
        else
            lua_pushnil(L);
    } else if (key == "floating")
        lua_pushboolean(L, w->m_isFloating);
    else if (key == "monitor") {
        const auto mon = w->m_monitor.lock();
        if (mon)
            Config::Lua::Objects::CLuaMonitor::push(L, mon);
        else
            lua_pushnil(L);
    } else if (key == "class")
        lua_pushstring(L, w->m_class.c_str());
    else if (key == "title")
        lua_pushstring(L, w->m_title.c_str());
    else if (key == "initial_class")
        lua_pushstring(L, w->m_initialClass.c_str());
    else if (key == "initial_title")
        lua_pushstring(L, w->m_initialTitle.c_str());
    else if (key == "pid")
        lua_pushinteger(L, static_cast<lua_Integer>(w->getPID()));
    else if (key == "xwayland")
        lua_pushboolean(L, w->m_isX11);
    else if (key == "pinned")
        lua_pushboolean(L, w->m_pinned);
    else if (key == "fullscreen")
        lua_pushinteger(L, static_cast<lua_Integer>(static_cast<uint8_t>(w->m_fullscreenState.internal)));
    else if (key == "fullscreen_client")
        lua_pushinteger(L, static_cast<lua_Integer>(static_cast<uint8_t>(w->m_fullscreenState.client)));
    else if (key == "over_fullscreen")
        lua_pushboolean(L, w->m_createdOverFullscreen);
    else if (key == "stable_id")
        lua_pushinteger(L, static_cast<lua_Integer>(w->m_stableID));
    else if (key == "layout") {
        const auto target = w->layoutTarget();
        if (!target || target->floating() || !w->m_workspace || !w->m_workspace->m_space) {
            lua_pushnil(L);
            return 1;
        }

        const auto& algo = w->m_workspace->m_space->algorithm();
        if (!algo || !algo->tiledAlgo()) {
            lua_pushnil(L);
            return 1;
        }

        const auto&       tiledAlgo = algo->tiledAlgo();
        const std::string name      = Layout::Supplementary::algoMatcher()->getNameForTiledAlgo(&typeid(*tiledAlgo.get()));

        lua_newtable(L);
        lua_pushstring(L, name.c_str());
        lua_setfield(L, -2, "name");

        if (const auto* master = dynamic_cast<Layout::Tiled::CMasterAlgorithm*>(tiledAlgo.get())) {
            const auto node = master->getNodeFromTarget(target);
            if (node) {
                lua_pushboolean(L, node->isMaster);
                lua_setfield(L, -2, "is_master");

                lua_pushnumber(L, node->percMaster);
                lua_setfield(L, -2, "perc_master");

                lua_pushnumber(L, node->percSize);
                lua_setfield(L, -2, "perc_size");
            }
        } else if (auto* scrolling = dynamic_cast<Layout::Tiled::CScrollingAlgorithm*>(tiledAlgo.get())) {
            const auto data = scrolling->dataFor(target);
            if (data) {
                const auto col = data->column.lock();
                if (col) {
                    const auto scrollingData = col->scrollingData.lock();

                    lua_newtable(L);

                    if (scrollingData) {
                        lua_pushinteger(L, static_cast<lua_Integer>(scrollingData->idx(col)));
                        lua_setfield(L, -2, "index");
                    }

                    lua_pushnumber(L, col->getColumnWidth());
                    lua_setfield(L, -2, "width");

                    lua_newtable(L);
                    int i = 1;
                    for (const auto& td : col->targetDatas) {
                        const auto t = td->target.lock();
                        if (t) {
                            const auto win = t->window();
                            if (win) {
                                Config::Lua::Objects::CLuaWindow::push(L, win);
                                lua_rawseti(L, -2, i++);
                            }
                        }
                    }
                    lua_setfield(L, -2, "windows");

                    lua_setfield(L, -2, "column");

                    lua_pushinteger(L, static_cast<lua_Integer>(col->idx(target)));
                    lua_setfield(L, -2, "index_in_column");
                }
            }
        }
    } else if (key == "active") {
        lua_pushboolean(L, w == Desktop::focusState()->window());
    } else
        lua_pushnil(L);

    return 1;
}

namespace Config::Lua::Objects {
    void CLuaWindow::setup(lua_State* L) {
        registerMetatable(L, MT, windowIndex, gcRef<PHLWINDOWREF>);
    }

    void CLuaWindow::push(lua_State* L, PHLWINDOW w) {
        new (lua_newuserdata(L, sizeof(PHLWINDOWREF))) PHLWINDOWREF(w ? w->m_self : nullptr);
        luaL_getmetatable(L, MT);
        lua_setmetatable(L, -2);
    }
}
