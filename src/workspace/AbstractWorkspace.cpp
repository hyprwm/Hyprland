#include "AbstractWorkspace.hpp"

using namespace Workspace;

IAbstractWorkspace::IAbstractWorkspace(eWorkspaceType x) : m_type(x) {
    ;
}

eWorkspaceType IAbstractWorkspace::type() const {
    return m_type;
}
