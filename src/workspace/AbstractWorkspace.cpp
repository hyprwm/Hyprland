#include "AbstractWorkspace.hpp"

using namespace Workspace;

std::string_view Workspace::identityTypeName(const IAbstractWorkspace& workspace) {
    if (std::holds_alternative<SWorkspaceNumberedID>(workspace.id()))
        return "numbered";
    if (workspace.type() == eWorkspaceType::SPECIAL)
        return "special";
    return "named";
}

IAbstractWorkspace::IAbstractWorkspace(eWorkspaceType x) : m_type(x) {
    ;
}

eWorkspaceType IAbstractWorkspace::type() const {
    return m_type;
}
