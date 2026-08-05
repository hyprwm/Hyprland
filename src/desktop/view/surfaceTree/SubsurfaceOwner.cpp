#include "SubsurfaceOwner.hpp"

#include "../Subsurface.hpp"

#include <utility>

using namespace Desktop::View;

CSubsurfaceOwner::CSubsurfaceOwner() {
    ;
}

CSubsurfaceOwner::~CSubsurfaceOwner() = default;

const SP<CSubsurface>& CSubsurfaceOwner::subsurfaceHead() const {
    return m_subsurfaceHead;
}

void CSubsurfaceOwner::setSubsurfaceHead(SP<CSubsurface> head) {
    m_subsurfaceHead = std::move(head);
}

void CSubsurfaceOwner::resetSubsurfaceHead() {
    m_subsurfaceHead.reset();
}

size_t CSubsurfaceOwner::subsurfaceTreeSize() const {
    return m_subsurfaceHead ? m_subsurfaceHead->allChildrenCount() : 0;
}

size_t CSubsurfaceOwner::subsurfaceMappedTreeSize() const {
    return m_subsurfaceHead ? m_subsurfaceHead->allMappedChildrenCount() : 0;
}
