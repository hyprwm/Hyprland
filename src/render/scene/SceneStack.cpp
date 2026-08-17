#include "SceneStack.hpp"

#include "../../macros.hpp"

using namespace Render;

void CSceneStack::push(SP<IScene> scene) {
    m_scenes.emplace_back(std::move(scene));
}

SP<IScene> CSceneStack::pop() {
    RASSERT(!m_scenes.empty(), "CSceneStack: called pop() on an empty SceneStack");
    auto x = m_scenes.back();
    m_scenes.pop_back();
    return x;
}

SP<IScene> CSceneStack::current() const {
    RASSERT(!m_scenes.empty(), "CSceneStack: called current() on an empty SceneStack");
    return m_scenes.back();
}
