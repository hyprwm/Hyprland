#include "SceneStack.hpp"

#include "../../macros.hpp"

#include <algorithm>

using namespace Render;

void CSceneStack::push(SP<IScene> scene) {
    m_scenes.emplace_back(std::move(scene));
}

SP<IScene> CSceneStack::pop() {
    RASSERT(m_scenes.size() > 1, "CSceneStack: called pop() without an override scene");
    auto x = m_scenes.back();
    m_scenes.pop_back();
    return x;
}

bool CSceneStack::remove(const SP<IScene>& scene) {
    const auto IT =
        std::ranges::find_if(m_scenes.begin() + std::min<size_t>(m_scenes.size(), 1), m_scenes.end(), [&scene](const auto& candidate) { return candidate.get() == scene.get(); });

    if (IT == m_scenes.end())
        return false;

    m_scenes.erase(IT);
    return true;
}

SP<IScene> CSceneStack::current() const {
    RASSERT(!m_scenes.empty(), "CSceneStack: called current() on an empty SceneStack");
    return m_scenes.back();
}

bool CSceneStack::hasOverride() const {
    return m_scenes.size() > 1;
}
