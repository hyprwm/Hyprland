#pragma once

#include "../../../helpers/memory/Memory.hpp"
#include "../../../helpers/math/Math.hpp"
#include "../../../layout/algorithm/TiledAlgorithm.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

namespace Config::Lua {
    class CConfigManager;
}

namespace Config::Lua::Layouts {

    struct SLuaLayoutProvider {
        CConfigManager* manager = nullptr;
        lua_State*      state   = nullptr;
        std::string     name;
        int             tableRef = LUA_NOREF;
        bool            active   = true;
        bool            didError = false;
    };

    class CLuaTiledAlgorithm : public Layout::ITiledAlgorithm {
      public:
        explicit CLuaTiledAlgorithm(SP<SLuaLayoutProvider> provider);
        virtual ~CLuaTiledAlgorithm() = default;

        virtual void                       newTarget(SP<Layout::ITarget> target);
        virtual void                       movedTarget(SP<Layout::ITarget> target, std::optional<Vector2D> focalPoint = std::nullopt);
        virtual void                       removeTarget(SP<Layout::ITarget> target);
        virtual void                       resizeTarget(const Vector2D& Δ, SP<Layout::ITarget> target, Layout::eRectCorner corner = Layout::CORNER_NONE);
        virtual void                       recalculate(Layout::eRecalculateReason reason = Layout::RECALCULATE_REASON_UNKNOWN);
        virtual void                       swapTargets(SP<Layout::ITarget> a, SP<Layout::ITarget> b);
        virtual void                       moveTargetInDirection(SP<Layout::ITarget> t, Math::eDirection dir, bool silent);
        virtual Config::ErrorResult        layoutMsg(const std::string_view& sv);
        virtual std::optional<Vector2D>    predictSizeForNewTarget();
        virtual SP<Layout::ITarget>        getNextCandidate(SP<Layout::ITarget> old);
        virtual std::optional<std::string> layoutName() const;

      private:
        SP<SLuaLayoutProvider>           m_provider;
        std::vector<WP<Layout::ITarget>> m_targets;

        std::vector<SP<Layout::ITarget>> liveTargets();
        bool                             callRecalculate(const std::vector<SP<Layout::ITarget>>& targets);
        void                             applyDefaultGrid(const std::vector<SP<Layout::ITarget>>& targets);
        void                             reportError(const std::string& message);
    };

}
