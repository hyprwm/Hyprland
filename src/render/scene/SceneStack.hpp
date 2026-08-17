#pragma once

#include "../../helpers/memory/Memory.hpp"

#include <vector>

namespace Render {
    class IScene;

    /*
     * A SceneStack holds a stack of scenes to be rendered.
     * This is used for hyprland to be able to "push" a new scene without
     * overriding the last one. For example, an overlay will "push" itself -> become
     * the "current" scene, and "pop" at the end, letting the default monitor
     * scene be rendered again.
     *
     * Only the "current" (top) scene is rendered.
     */
    class CSceneStack {
      public:
        CSceneStack()  = default;
        ~CSceneStack() = default;

        CSceneStack(const CSceneStack&) = delete;
        CSceneStack(CSceneStack&)       = delete;
        CSceneStack(CSceneStack&&)      = delete;

        void       push(SP<IScene> scene);
        SP<IScene> pop();
        bool       remove(const SP<IScene>& scene);

        SP<IScene> current() const;
        bool       hasOverride() const;

      private:
        std::vector<SP<IScene>> m_scenes;
    };
};
