#include "LuaLayoutProvider.hpp"

#include "LuaLayoutContext.hpp"
#include "LuaLayoutTarget.hpp"
#include "../ConfigManager.hpp"
#include "../bindings/LuaBindingsInternal.hpp"

#include "../../../debug/log/Logger.hpp"
#include "../../../layout/algorithm/Algorithm.hpp"
#include "../../../layout/space/Space.hpp"
#include "../../../layout/target/Target.hpp"
#include "../../../layout/supplementary/WorkspaceAlgoMatcher.hpp"

#include <algorithm>
#include <cmath>
#include <format>

using namespace Config::Lua;
using namespace Config::Lua::Layouts;

static std::string normalizeLuaLayoutName(std::string name) {
    if (!name.starts_with("lua:"))
        name = "lua:" + name;
    return name;
}

CLuaTiledAlgorithm::CLuaTiledAlgorithm(SP<SLuaLayoutProvider> provider) : m_provider(std::move(provider)) {
    ;
}

void CLuaTiledAlgorithm::newTarget(SP<Layout::ITarget> target) {
    m_targets.emplace_back(target);
    recalculate();
}

void CLuaTiledAlgorithm::movedTarget(SP<Layout::ITarget> target, std::optional<Vector2D> focalPoint) {
    newTarget(target);
}

void CLuaTiledAlgorithm::removeTarget(SP<Layout::ITarget> target) {
    std::erase_if(m_targets, [&target](const auto& t) { return !t || t.lock() == target; });
    recalculate();
}

void CLuaTiledAlgorithm::resizeTarget(const Vector2D& Δ, SP<Layout::ITarget> target, Layout::eRectCorner corner) {
    recalculate();
}

void CLuaTiledAlgorithm::recalculate(Layout::eRecalculateReason reason) {
    auto targets = liveTargets();
    if (targets.empty())
        return;

    if (!callRecalculate(targets))
        applyDefaultGrid(targets);
}

void CLuaTiledAlgorithm::swapTargets(SP<Layout::ITarget> a, SP<Layout::ITarget> b) {
    auto ia = std::ranges::find_if(m_targets, [&a](const auto& t) { return t.lock() == a; });
    auto ib = std::ranges::find_if(m_targets, [&b](const auto& t) { return t.lock() == b; });

    if (ia != m_targets.end() && ib != m_targets.end())
        std::iter_swap(ia, ib);
    else {
        if (ia != m_targets.end())
            *ia = b;
        if (ib != m_targets.end())
            *ib = a;
    }

    recalculate();
}

void CLuaTiledAlgorithm::moveTargetInDirection(SP<Layout::ITarget> t, Math::eDirection dir, bool silent) {
    auto it = std::ranges::find_if(m_targets, [&t](const auto& target) { return target.lock() == t; });
    if (it == m_targets.end())
        return;

    if ((dir == Math::DIRECTION_LEFT || dir == Math::DIRECTION_UP) && it != m_targets.begin())
        std::iter_swap(it, std::prev(it));
    else if ((dir == Math::DIRECTION_RIGHT || dir == Math::DIRECTION_DOWN) && std::next(it) != m_targets.end())
        std::iter_swap(it, std::next(it));
    else
        return;

    recalculate();
}

Config::ErrorResult CLuaTiledAlgorithm::layoutMsg(const std::string_view& sv) {
    if (!m_provider || !m_provider->active || !m_provider->state)
        return {};

    auto targets = liveTargets();
    auto parent  = m_parent.lock();
    auto space   = parent ? parent->space() : nullptr;
    if (!space)
        return {};

    lua_State* L   = m_provider->state;
    const int  top = lua_gettop(L);

    lua_rawgeti(L, LUA_REGISTRYINDEX, m_provider->tableRef);
    lua_getfield(L, -1, "layout_msg");
    lua_remove(L, -2);

    if (!lua_isfunction(L, -1)) {
        lua_settop(L, top);
        return {};
    }

    pushLayoutContext(L, targets, space->workArea());
    lua_pushlstring(L, sv.data(), sv.size());

    const int status = m_provider->manager->guardedPCall(2, 1, 0, CConfigManager::LUA_TIMEOUT_LAYOUT_CALLBACK_MS, "lua layout_msg callback");
    if (status != LUA_OK) {
        std::string err = lua_tostring(L, -1) ? lua_tostring(L, -1) : "unknown lua error";
        lua_settop(L, top);
        reportError(err);
        return Config::configError(std::format("lua layout {} layout_msg failed: {}", m_provider->name, err), Config::eConfigErrorLevel::ERROR,
                                   Config::eConfigErrorCode::LUA_ERROR);
    }

    Config::ErrorResult result = {};
    if (lua_isboolean(L, -1) && !lua_toboolean(L, -1))
        result =
            Config::configError(std::format("lua layout {} rejected layoutmsg", m_provider->name), Config::eConfigErrorLevel::ERROR, Config::eConfigErrorCode::INVALID_ARGUMENT);
    else if (lua_isstring(L, -1))
        result = Config::configError(lua_tostring(L, -1), Config::eConfigErrorLevel::ERROR, Config::eConfigErrorCode::INVALID_ARGUMENT);

    lua_settop(L, top);
    recalculate();
    return result;
}

std::optional<Vector2D> CLuaTiledAlgorithm::predictSizeForNewTarget() {
    auto parent = m_parent.lock();
    auto space  = parent ? parent->space() : nullptr;
    if (!space)
        return std::nullopt;
    return space->workArea().size();
}

SP<Layout::ITarget> CLuaTiledAlgorithm::getNextCandidate(SP<Layout::ITarget> old) {
    auto targets = liveTargets();
    if (targets.empty())
        return nullptr;

    auto it = std::ranges::find(targets, old);
    if (it == targets.end())
        return targets.back();

    if (targets.size() == 1)
        return nullptr;

    auto next = std::next(it);
    if (next == targets.end())
        next = targets.begin();

    return *next;
}

std::optional<std::string> CLuaTiledAlgorithm::layoutName() const {
    if (!m_provider)
        return std::nullopt;
    return m_provider->name;
}

std::vector<SP<Layout::ITarget>> CLuaTiledAlgorithm::liveTargets() {
    std::erase_if(m_targets, [](const auto& target) { return !target || !target.lock(); });

    std::vector<SP<Layout::ITarget>> result;
    for (const auto& target : m_targets) {
        if (const auto locked = target.lock(); locked)
            result.emplace_back(locked);
    }
    return result;
}

bool CLuaTiledAlgorithm::callRecalculate(const std::vector<SP<Layout::ITarget>>& targets) {
    if (!m_provider || !m_provider->active || !m_provider->state || !m_provider->manager)
        return false;

    auto parent = m_parent.lock();
    auto space  = parent ? parent->space() : nullptr;
    if (!space)
        return false;

    lua_State* L   = m_provider->state;
    const int  top = lua_gettop(L);

    lua_rawgeti(L, LUA_REGISTRYINDEX, m_provider->tableRef);
    lua_getfield(L, -1, "recalculate");
    lua_remove(L, -2);

    if (!lua_isfunction(L, -1)) {
        lua_settop(L, top);
        return false;
    }

    pushLayoutContext(L, targets, space->workArea());

    const int status = m_provider->manager->guardedPCall(1, 0, 0, CConfigManager::LUA_TIMEOUT_LAYOUT_CALLBACK_MS, "lua layout recalculate callback");
    if (status != LUA_OK) {
        std::string err = lua_tostring(L, -1) ? lua_tostring(L, -1) : "unknown lua error";
        lua_settop(L, top);
        reportError(err);
        return false;
    }

    lua_settop(L, top);
    return true;
}

void CLuaTiledAlgorithm::applyDefaultGrid(const std::vector<SP<Layout::ITarget>>& targets) {
    auto parent = m_parent.lock();
    auto space  = parent ? parent->space() : nullptr;
    if (!space || targets.empty())
        return;

    const auto AREA = space->workArea();
    const int  cols = std::max(1, sc<int>(std::ceil(std::sqrt(sc<double>(targets.size())))));
    const int  rows = std::max(1, sc<int>(std::ceil(sc<double>(targets.size()) / sc<double>(cols))));

    for (size_t i = 0; i < targets.size(); ++i) {
        const int col = sc<int>(i) % cols;
        const int row = sc<int>(i) / cols;
        targets[i]->setPositionGlobal(CBox{AREA.x + AREA.w * col / cols, AREA.y + AREA.h * row / rows, AREA.w / cols, AREA.h / rows}.noNegativeSize());
    }
}

void CLuaTiledAlgorithm::reportError(const std::string& message) {
    if (!m_provider)
        return;

    Log::logger->log(Log::ERR, "[lua] layout {} error: {}", m_provider->name, message);

    if (m_provider->didError || !m_provider->manager)
        return;

    m_provider->didError = true;
    m_provider->manager->addError(std::format("lua layout {} error: {}", m_provider->name, message));
}

std::expected<void, std::string> CConfigManager::registerLuaLayoutProvider(std::string name, lua_State* L, int providerTableIdx) {
    if (name.empty())
        return std::unexpected("layout name cannot be empty");

    name = normalizeLuaLayoutName(std::move(name));
    if (name == "lua:")
        return std::unexpected("layout name cannot be empty");

    providerTableIdx = lua_absindex(L, providerTableIdx);
    if (!lua_istable(L, providerTableIdx))
        return std::unexpected("provider must be a table");

    lua_getfield(L, providerTableIdx, "recalculate");
    const bool hasRecalculate = lua_isfunction(L, -1);
    lua_pop(L, 1);

    if (!hasRecalculate)
        return std::unexpected("provider table must define recalculate(ctx)");

    lua_pushvalue(L, providerTableIdx);
    const int ref = luaL_ref(L, LUA_REGISTRYINDEX);

    auto      provider = makeShared<SLuaLayoutProvider>();
    provider->manager  = this;
    provider->state    = L;
    provider->name     = name;
    provider->tableRef = ref;

    if (!Layout::Supplementary::algoMatcher()->registerTiledAlgo(name, &typeid(CLuaTiledAlgorithm), [provider] { return makeUnique<CLuaTiledAlgorithm>(provider); })) {
        luaL_unref(L, LUA_REGISTRYINDEX, ref);
        return std::unexpected(std::format("layout '{}' is already registered", name));
    }

    m_luaLayoutProviders.emplace_back(provider);
    return {};
}

void CConfigManager::clearLuaLayoutProviders() {
    if (m_luaLayoutProviders.empty())
        return;

    for (auto& provider : m_luaLayoutProviders) {
        if (provider)
            provider->active = false;
    }

    for (auto& provider : m_luaLayoutProviders) {
        if (provider)
            Layout::Supplementary::algoMatcher()->unregisterAlgo(provider->name);
    }

    for (auto& provider : m_luaLayoutProviders) {
        if (provider && m_lua && provider->tableRef != LUA_NOREF) {
            luaL_unref(m_lua, LUA_REGISTRYINDEX, provider->tableRef);
            provider->tableRef = LUA_NOREF;
        }
    }

    m_luaLayoutProviders.clear();
}

static int hlLayoutRegister(lua_State* L) {
    auto*       mgr  = sc<CConfigManager*>(lua_touserdata(L, lua_upvalueindex(1)));
    const char* name = luaL_checkstring(L, 1);
    luaL_checktype(L, 2, LUA_TTABLE);

    auto result = mgr->registerLuaLayoutProvider(name, L, 2);
    if (!result)
        return Config::Lua::Bindings::Internal::configError(L, "hl.layout.register: {}", result.error());

    return 0;
}

void Config::Lua::Bindings::Internal::registerLayoutBindings(lua_State* L, CConfigManager* mgr) {
    setupLayoutTarget(L);

    lua_newtable(L);
    setMgrFn(L, mgr, "register", hlLayoutRegister);
    lua_setfield(L, -2, "layout");
}
