#pragma once

#include "../../../render/decorations/ToolkitDecorationProvider.hpp"
#include "../LuaRef.hpp"

#include <expected>
#include <string>

namespace Config::Lua {
    class CConfigManager;
}

namespace Config::Lua::Decorations {

    // validates a hl.decoration.register spec table
    std::expected<SToolkitDecorationSpec, std::string> parseSpec(lua_State* L, int idx);

    class CLuaDecorationProvider : public IToolkitDecorationProvider {
      public:
        CLuaDecorationProvider(CConfigManager* manager, std::string name, const SToolkitDecorationSpec& spec, CLuaRef&& create);
        virtual ~CLuaDecorationProvider() = default;

        virtual const std::string&            name() const override;
        virtual const SToolkitDecorationSpec& spec() const override;
        virtual UP<IToolkitDecorationClient>  createClient(const SP<CHyprToolkitDecoration>& decoration) override;

      private:
        CConfigManager*        m_manager = nullptr;
        std::string            m_name;
        std::string            m_createContext;
        SToolkitDecorationSpec m_spec;
        CLuaRef                m_create;
    };
}
