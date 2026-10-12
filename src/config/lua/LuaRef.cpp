#include "LuaRef.hpp"

using namespace Config::Lua;

CLuaRef::CLuaRef(SP<SLuaStateLifetime> lifetime, lua_State* L, int idx) : m_lifetime(std::move(lifetime)) {
    if (!m_lifetime || !m_lifetime->state)
        return;

    lua_pushvalue(L, idx);
    m_ref = luaL_ref(L, LUA_REGISTRYINDEX);
}

CLuaRef::~CLuaRef() {
    release();
}

CLuaRef::CLuaRef(CLuaRef&& other) noexcept : m_lifetime(std::move(other.m_lifetime)), m_ref(other.m_ref) {
    other.m_ref = LUA_NOREF;
}

CLuaRef& CLuaRef::operator=(CLuaRef&& other) noexcept {
    if (this == &other)
        return *this;

    release();
    m_lifetime  = std::move(other.m_lifetime);
    m_ref       = other.m_ref;
    other.m_ref = LUA_NOREF;
    return *this;
}

lua_State* CLuaRef::state() const {
    if (!m_lifetime || m_ref == LUA_NOREF || m_ref == LUA_REFNIL)
        return nullptr;

    return m_lifetime->state;
}

bool CLuaRef::push() const {
    const auto L = state();
    if (!L)
        return false;

    lua_rawgeti(L, LUA_REGISTRYINDEX, m_ref);
    return true;
}

int CLuaRef::id() const {
    return m_ref;
}

void CLuaRef::release() {
    if (const auto L = state(); L)
        luaL_unref(L, LUA_REGISTRYINDEX, m_ref);

    m_lifetime.reset();
    m_ref = LUA_NOREF;
}
