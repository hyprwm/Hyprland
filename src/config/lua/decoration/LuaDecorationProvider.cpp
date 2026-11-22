#include "LuaDecorationProvider.hpp"

#include <hyprtoolkit-lua/Embedding.hpp>
#include <hyprtoolkit/element/Element.hpp>
#include <hyprtoolkit/window/EmbeddedSurface.hpp>

#include <array>
#include <format>
#include <optional>
#include <string_view>
#include <tuple>
#include <vector>

#include "../ConfigManager.hpp"
#include "../LuaCaller.hpp"
#include "../LuaEventHandler.hpp"
#include "../bindings/Check.hpp"
#include "../bindings/LuaBindingsInternal.hpp"
#include "../objects/LuaEventSubscription.hpp"
#include "../objects/LuaObjectHelpers.hpp"
#include "../objects/LuaWindow.hpp"
#include "../../supplementary/propRefresher/PropRefresher.hpp"
#include "../../../render/decorations/CHyprToolkitDecoration.hpp"
#include "../../../render/decorations/ToolkitDecorationController.hpp"

using namespace Config::Lua;
using namespace Config::Lua::Decorations;

static constexpr const char* CLIENT_HANDLE_MT = "hl_decoration_client";

namespace Config::Lua::Decorations {
    // owns what deco:on() subscribed to
    class CLuaDecorationClient : public IToolkitDecorationClient {
      public:
        CLuaDecorationClient(WP<CLuaEventHandler> handler);
        virtual ~CLuaDecorationClient();

        void track(uint64_t subscription);

      private:
        WP<CLuaEventHandler>  m_handler;
        std::vector<uint64_t> m_subscriptions;
    };
}

CLuaDecorationClient::CLuaDecorationClient(WP<CLuaEventHandler> handler) : m_handler(std::move(handler)) {
    ;
}

CLuaDecorationClient::~CLuaDecorationClient() {
    if (!m_handler)
        return;

    for (const auto& subscription : m_subscriptions) {
        m_handler->unregisterEvent(subscription);
    }
}

void CLuaDecorationClient::track(uint64_t subscription) {
    m_subscriptions.emplace_back(subscription);
}

static std::optional<eDecorationEdges> edgeFromString(std::string_view edge) {
    if (edge == "top")
        return DECORATION_EDGE_TOP;
    if (edge == "bottom")
        return DECORATION_EDGE_BOTTOM;
    if (edge == "left")
        return DECORATION_EDGE_LEFT;
    if (edge == "right")
        return DECORATION_EDGE_RIGHT;
    return std::nullopt;
}

static std::optional<eDecorationLayer> layerFromString(std::string_view layer) {
    if (layer == "bottom")
        return DECORATION_LAYER_BOTTOM;
    if (layer == "under")
        return DECORATION_LAYER_UNDER;
    if (layer == "over")
        return DECORATION_LAYER_OVER;
    if (layer == "overlay")
        return DECORATION_LAYER_OVERLAY;
    return std::nullopt;
}

// nullopt for nil, an error for a value of the wrong type
template <typename T>
static std::expected<std::optional<T>, std::string> optionalField(lua_State* L, int idx, const char* name, std::expected<T, std::string> (*check)(lua_State*, int)) {
    lua_getfield(L, idx, name);

    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        return std::nullopt;
    }

    auto value = check(L, -1);
    lua_pop(L, 1);

    if (!value)
        return std::unexpected(std::format("field \"{}\": {}", name, value.error()));

    return std::move(*value);
}

static std::expected<void, std::string> checkKnownFields(lua_State* L, int idx) {
    static constexpr std::array<std::string_view, 7> KNOWN = {"edge", "size", "priority", "reserved", "layer", "input", "create"};

    lua_pushnil(L);
    while (lua_next(L, idx)) {
        lua_pop(L, 1);

        // don't lua_tostring() non-string keys, it'd break lua_next
        const bool STRING_KEY = lua_type(L, -1) == LUA_TSTRING;
        if (!STRING_KEY || !std::ranges::contains(KNOWN, std::string_view{lua_tostring(L, -1)})) {
            const auto MSG = STRING_KEY ? std::format("unknown field \"{}\"", lua_tostring(L, -1)) : std::format("unexpected {} key", luaL_typename(L, -1));
            lua_pop(L, 1);
            return std::unexpected(MSG);
        }
    }

    return {};
}

std::expected<SToolkitDecorationSpec, std::string> Decorations::parseSpec(lua_State* L, int idx) {
    idx = lua_absindex(L, idx);
    if (!lua_istable(L, idx))
        return std::unexpected("spec must be a table");

    if (auto known = checkKnownFields(L, idx); !known)
        return std::unexpected(known.error());

    SToolkitDecorationSpec spec;

    const auto             EDGE = optionalField<std::string>(L, idx, "edge", Bindings::Check::string);
    if (!EDGE)
        return std::unexpected(EDGE.error());
    if (!*EDGE)
        return std::unexpected("missing field \"edge\"");
    const auto PARSED_EDGE = edgeFromString(**EDGE);
    if (!PARSED_EDGE)
        return std::unexpected(std::format("field \"edge\": expected top, bottom, left or right, got \"{}\"", **EDGE));
    spec.edge = *PARSED_EDGE;

    const auto SIZE = optionalField<int64_t>(L, idx, "size", Bindings::Check::integer);
    if (!SIZE)
        return std::unexpected(SIZE.error());
    if (!*SIZE)
        return std::unexpected("missing field \"size\"");
    if (**SIZE <= 0 || **SIZE > 10000)
        return std::unexpected(std::format("field \"size\": must be in [1, 10000], got {}", **SIZE));
    spec.size = sc<int>(**SIZE);

    const auto PRIORITY = optionalField<int64_t>(L, idx, "priority", Bindings::Check::integer);
    if (!PRIORITY)
        return std::unexpected(PRIORITY.error());
    if (*PRIORITY && (**PRIORITY < 0 || **PRIORITY > UINT32_MAX))
        return std::unexpected(std::format("field \"priority\": must be a non-negative 32-bit integer, got {}", **PRIORITY));
    spec.priority = sc<uint32_t>(PRIORITY->value_or(spec.priority));

    const auto RESERVED = optionalField<bool>(L, idx, "reserved", Bindings::Check::boolean);
    if (!RESERVED)
        return std::unexpected(RESERVED.error());
    spec.reserved = RESERVED->value_or(spec.reserved);

    const auto LAYER = optionalField<std::string>(L, idx, "layer", Bindings::Check::string);
    if (!LAYER)
        return std::unexpected(LAYER.error());
    if (*LAYER) {
        const auto PARSED_LAYER = layerFromString(**LAYER);
        if (!PARSED_LAYER)
            return std::unexpected(std::format("field \"layer\": expected bottom, under, over or overlay, got \"{}\"", **LAYER));
        spec.layer = *PARSED_LAYER;
    }

    const auto INPUT = optionalField<bool>(L, idx, "input", Bindings::Check::boolean);
    if (!INPUT)
        return std::unexpected(INPUT.error());
    spec.input = INPUT->value_or(spec.input);

    lua_getfield(L, idx, "create");
    const bool HAS_CREATE = lua_isfunction(L, -1);
    lua_pop(L, 1);
    if (!HAS_CREATE)
        return std::unexpected("field \"create\" must be a function");

    return spec;
}

static int decorationOn(lua_State* L) {
    auto*      mgr    = CConfigManager::fromLuaState(L);
    auto*      client = sc<WP<CLuaDecorationClient>*>(lua_touserdata(L, lua_upvalueindex(1)));

    const auto EVENT = Bindings::Check::string(L, 2);
    if (!EVENT)
        return Bindings::Internal::configError(L, std::format("deco:on: bad argument 1: {}", EVENT.error()));

    luaL_checktype(L, 3, LUA_TFUNCTION);

    if (!*client)
        return Bindings::Internal::configError(L, "deco:on: the decoration no longer exists");

    const auto SUBSCRIPTION = mgr->m_eventHandler->registerEvent(*EVENT, CLuaRef{mgr->luaStateLifetime(), L, 3});
    if (!SUBSCRIPTION)
        return Bindings::Internal::configError(L, "deco:on: unknown event \"{}\"", *EVENT);

    (*client)->track(*SUBSCRIPTION);
    Objects::CLuaEventSubscription::push(L, mgr->m_eventHandler.get(), *SUBSCRIPTION);
    return 1;
}

// { window, name, root, on }
static void pushDecoration(lua_State* L, const SP<CHyprToolkitDecoration>& decoration, const std::string& name, const WP<CLuaDecorationClient>& client) {
    lua_newtable(L);

    Objects::CLuaWindow::push(L, decoration->window());
    lua_setfield(L, -2, "window");

    lua_pushlstring(L, name.data(), name.size());
    lua_setfield(L, -2, "name");

    try {
        Hyprtoolkit::Lua::pushElement(L, decoration->surface()->rootElement());
    } catch (const std::exception& e) {
        LOG(Log::ERR, "[Lua] decoration \"{}\": couldn't expose its root: {}", name, e.what());
        lua_pushnil(L);
    }
    lua_setfield(L, -2, "root");

    new (lua_newuserdata(L, sizeof(WP<CLuaDecorationClient>))) WP<CLuaDecorationClient>(client);
    luaL_setmetatable(L, CLIENT_HANDLE_MT);
    lua_pushcclosure(L, decorationOn, 1);
    lua_setfield(L, -2, "on");
}

CLuaDecorationProvider::CLuaDecorationProvider(CConfigManager* manager, std::string name, const SToolkitDecorationSpec& spec, CLuaRef&& create) :
    m_manager(manager), m_name(std::move(name)), m_createContext(std::format("hl.decoration \"{}\" create", m_name)), m_spec(spec), m_create(std::move(create)) {
    ;
}

const std::string& CLuaDecorationProvider::name() const {
    return m_name;
}

const SToolkitDecorationSpec& CLuaDecorationProvider::spec() const {
    return m_spec;
}

UP<IToolkitDecorationClient> CLuaDecorationProvider::createClient(const SP<CHyprToolkitDecoration>& decoration) {
    if (!decoration || !decoration->surface() || !m_manager->m_eventHandler)
        return nullptr;

    auto                     client = makeUnique<CLuaDecorationClient>(m_manager->m_eventHandler);
    WP<CLuaDecorationClient> weak   = client;

    // errors are reported by the caller
    std::ignore = m_manager->caller().call(m_create, CLuaCaller::LUA_TIMEOUT_TOOLKIT_CALLBACK_MS, m_createContext, [&](lua_State* L) {
        pushDecoration(L, decoration, m_name, weak);
        return 1;
    });

    return client;
}

std::expected<void, std::string> CConfigManager::registerLuaDecorationProvider(std::string name, lua_State* L, int specIdx) {
    if (name.empty())
        return std::unexpected("decoration name cannot be empty");

    // the decorations rule is a space-separated list
    if (name.find_first_of(" \t\n") != std::string::npos)
        return std::unexpected("decoration names cannot contain whitespace");

    specIdx         = lua_absindex(L, specIdx);
    const auto SPEC = Decorations::parseSpec(L, specIdx);
    if (!SPEC)
        return std::unexpected(SPEC.error());

    lua_getfield(L, specIdx, "create");
    auto provider = makeShared<CLuaDecorationProvider>(this, name, *SPEC, CLuaRef{m_luaStateLifetime, L, -1});
    lua_pop(L, 1);

    if (!Toolkit::decorations()->registerProvider(provider))
        return std::unexpected(std::format("decoration \"{}\" is already registered", name));

    m_luaDecorations.emplace_back(std::move(name));
    return {};
}

void CConfigManager::clearLuaDecorationProviders() {
    if (Toolkit::decorations()) {
        for (const auto& name : m_luaDecorations) {
            Toolkit::decorations()->unregisterProvider(name);
        }
    }

    m_luaDecorations.clear();
}

static int hlDecorationRegister(lua_State* L) {
    auto*      mgr  = sc<CConfigManager*>(lua_touserdata(L, lua_upvalueindex(1)));
    const auto NAME = Bindings::Check::string(L, 1);
    if (!NAME)
        return Bindings::Internal::configError(L, std::format("hl.decoration.register: bad argument 1: {}", NAME.error()));

    luaL_checktype(L, 2, LUA_TTABLE);

    if (const auto RESULT = mgr->registerLuaDecorationProvider(*NAME, L, 2); !RESULT)
        return Bindings::Internal::configError(L, "hl.decoration.register: {}", RESULT.error());

    // windows may already want it
    Config::Supplementary::refresher()->scheduleRefresh(Config::Supplementary::REFRESH_WINDOW_STATES);
    return 0;
}

static void setupClientHandle(lua_State* L) {
    luaL_newmetatable(L, CLIENT_HANDLE_MT);
    lua_pushcfunction(L, Objects::gcRef<WP<CLuaDecorationClient>>);
    lua_setfield(L, -2, "__gc");
    lua_pop(L, 1);
}

void Config::Lua::Bindings::Internal::registerDecorationBindings(lua_State* L, CConfigManager* mgr) {
    setupClientHandle(L);

    lua_newtable(L);
    setMgrFn(L, mgr, "register", hlDecorationRegister);
    lua_setfield(L, -2, "decoration");
}
