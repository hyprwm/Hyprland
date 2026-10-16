#include "LuaLayoutProvider.hpp"

#include "LuaLayoutContext.hpp"
#include "LuaLayoutTarget.hpp"
#include "../ConfigManager.hpp"
#include "../LuaCaller.hpp"
#include "../bindings/Check.hpp"
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
        name = std::format("lua:{}", name);
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
    if (!m_provider || !m_provider->table.state())
        return {};

    auto targets = liveTargets();
    auto parent  = m_parent.lock();
    auto space   = parent ? parent->space() : nullptr;
    if (!space)
        return {};

    lua_State* L   = m_provider->table.state();
    const int  top = lua_gettop(L);

    m_provider->table.push();
    lua_getfield(L, -1, "layout_msg");
    lua_remove(L, -2);

    if (!lua_isfunction(L, -1)) {
        lua_settop(L, top);
        return {};
    }

    pushLayoutContext(L, targets, space->workArea());
    lua_pushlstring(L, sv.data(), sv.size());

    Config::ErrorResult result = {};
    const auto          CALLED = m_provider->manager->caller().callPushed(2, CLuaCaller::LUA_TIMEOUT_LAYOUT_CALLBACK_MS, m_provider->layoutMsgContext, [&](lua_State* L) {
        if (lua_isboolean(L, -1) && !lua_toboolean(L, -1))
            result = Config::configError(std::format("lua layout {} rejected layoutmsg", m_provider->name), Config::eConfigErrorLevel::ERROR,
                                         Config::eConfigErrorCode::INVALID_ARGUMENT);
        else if (lua_isstring(L, -1))
            result = Config::configError(lua_tostring(L, -1), Config::eConfigErrorLevel::ERROR, Config::eConfigErrorCode::INVALID_ARGUMENT);
    });

    if (!CALLED)
        return Config::configError(CALLED.error(), Config::eConfigErrorLevel::ERROR, Config::eConfigErrorCode::LUA_ERROR);

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
    if (!m_provider || !m_provider->table.state() || !m_provider->manager)
        return false;

    auto parent = m_parent.lock();
    auto space  = parent ? parent->space() : nullptr;
    if (!space)
        return false;

    lua_State* L   = m_provider->table.state();
    const int  top = lua_gettop(L);

    m_provider->table.push();
    lua_getfield(L, -1, "recalculate");
    lua_remove(L, -2);

    if (!lua_isfunction(L, -1)) {
        lua_settop(L, top);
        return false;
    }

    pushLayoutContext(L, targets, space->workArea());

    return m_provider->manager->caller().callPushed(1, CLuaCaller::LUA_TIMEOUT_LAYOUT_CALLBACK_MS, m_provider->recalculateContext).has_value();
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

    auto provider                = makeShared<SLuaLayoutProvider>();
    provider->manager            = this;
    provider->name               = name;
    provider->recalculateContext = std::format("hl.layout \"{}\" recalculate", name);
    provider->layoutMsgContext   = std::format("hl.layout \"{}\" layout_msg", name);
    provider->table              = CLuaRef{m_luaStateLifetime, L, providerTableIdx};

    if (!Layout::Supplementary::algoMatcher()->registerTiledAlgo(name, &typeid(CLuaTiledAlgorithm), [provider] { return makeUnique<CLuaTiledAlgorithm>(provider); })) {
        provider->table = {};
        return std::unexpected(std::format("layout '{}' is already registered", name));
    }

    m_luaLayoutProviders.emplace_back(provider);
    return {};
}

void CConfigManager::clearLuaLayoutProviders() {
    if (m_luaLayoutProviders.empty())
        return;

    // before unregistering, which can recalculate
    for (auto& provider : m_luaLayoutProviders) {
        if (provider)
            provider->table = {};
    }

    for (auto& provider : m_luaLayoutProviders) {
        if (provider)
            Layout::Supplementary::algoMatcher()->unregisterAlgo(provider->name);
    }

    m_luaLayoutProviders.clear();
}

static int hlLayoutRegister(lua_State* L) {
    auto*      mgr  = sc<CConfigManager*>(lua_touserdata(L, lua_upvalueindex(1)));
    const auto name = Bindings::Check::string(L, 1);
    if (!name)
        return Bindings::Internal::configError(L, std::format("layout.register: bad argument 1: {}", name.error()));

    luaL_checktype(L, 2, LUA_TTABLE);

    auto result = mgr->registerLuaLayoutProvider(*name, L, 2);
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
