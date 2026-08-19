#include "BackdropScopePassElement.hpp"
#include "../Renderer.hpp"

void CBackdropScopePlanner::begin(SP<SBackdropScope> scope) {
    RASSERT(scope, "Cannot plan a null backdrop scope");
    scope->required = false;
    scope->damage.clear();
    m_scopes.emplace_back(std::move(scope));
}

void CBackdropScopePlanner::addLiveBlur(const CRegion& damage) {
    if (m_scopes.empty())
        return;

    m_scopes.back()->required = true;
    m_scopes.back()->damage.add(damage);
}

void CBackdropScopePlanner::end(SP<SBackdropScope> scope, const CBox& bounds) {
    RASSERT(!m_scopes.empty() && m_scopes.back() == scope, "Unbalanced backdrop scope markers");
    if (scope->required)
        scope->damage.intersect(bounds);
    m_scopes.pop_back();
}

bool CBackdropScopePlanner::empty() const {
    return m_scopes.empty();
}

CBackdropScopePassElement::CBackdropScopePassElement(eAction action, SP<SBackdropScope> scope) : m_action(action), m_scope(std::move(scope)) {
    ;
}

std::vector<UP<IPassElement>> CBackdropScopePassElement::draw(Render::CRenderingContext& context) {
    if (!m_scope->required)
        return {};

    if (m_action == eAction::BEGIN)
        g_pHyprRenderer->beginBackdropScope(context, m_scope);
    else
        g_pHyprRenderer->endBackdropScope(context, m_scope);

    return {};
}

bool CBackdropScopePassElement::needsLiveBlur(const Render::CRenderingContext&) {
    return false;
}

bool CBackdropScopePassElement::needsPrecomputeBlur(const Render::CRenderingContext&) {
    return false;
}

bool CBackdropScopePassElement::undiscardable() {
    return true;
}

const char* CBackdropScopePassElement::passName() {
    return "CBackdropScopePassElement";
}

ePassElementType CBackdropScopePassElement::type() {
    return EK_BACKDROP_SCOPE;
}

CBackdropScopePassElement::eAction CBackdropScopePassElement::action() const {
    return m_action;
}

SP<SBackdropScope> CBackdropScopePassElement::scope() const {
    return m_scope;
}
