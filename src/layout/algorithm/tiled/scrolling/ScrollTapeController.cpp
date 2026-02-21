#include "ScrollTapeController.hpp"
#include "ScrollingAlgorithm.hpp"
#include <algorithm>
#include <cmath>

using namespace Layout::Tiled;

CScrollTapeController::CScrollTapeController(eScrollDirection direction) : m_direction(direction) {
    ;
}

void CScrollTapeController::setDirection(eScrollDirection dir) {
    m_direction = dir;
}

eScrollDirection CScrollTapeController::getDirection() const {
    return m_direction;
}

bool CScrollTapeController::isPrimaryHorizontal() const {
    return m_direction == SCROLL_DIR_RIGHT || m_direction == SCROLL_DIR_LEFT;
}

bool CScrollTapeController::isReversed() const {
    return m_direction == SCROLL_DIR_LEFT || m_direction == SCROLL_DIR_UP;
}

size_t CScrollTapeController::stripCount() const {
    return m_strips.size();
}

SStripData& CScrollTapeController::getStrip(size_t index) {
    return m_strips[index];
}

const SStripData& CScrollTapeController::getStrip(size_t index) const {
    return m_strips[index];
}

void CScrollTapeController::setOffset(double offset) {
    m_offset = offset;
}

double CScrollTapeController::getOffset() const {
    return m_offset;
}

void CScrollTapeController::adjustOffset(double delta) {
    m_offset += delta;
}

size_t CScrollTapeController::addStrip(float size) {
    m_strips.emplace_back();
    m_strips.back().size = size;
    return m_strips.size() - 1;
}

void CScrollTapeController::insertStrip(size_t afterIndex, float size) {
    if (afterIndex >= m_strips.size()) {
        addStrip(size);
        return;
    }

    SStripData newStrip;
    newStrip.size = size;
    m_strips.insert(m_strips.begin() + afterIndex + 1, newStrip);
}

void CScrollTapeController::removeStrip(size_t index) {
    if (index < m_strips.size())
        m_strips.erase(m_strips.begin() + index);
}

double CScrollTapeController::getPrimary(const Vector2D& v) const {
    return isPrimaryHorizontal() ? v.x : v.y;
}

double CScrollTapeController::getSecondary(const Vector2D& v) const {
    return isPrimaryHorizontal() ? v.y : v.x;
}

void CScrollTapeController::setPrimary(Vector2D& v, double val) const {
    if (isPrimaryHorizontal())
        v.x = val;
    else
        v.y = val;
}

void CScrollTapeController::setSecondary(Vector2D& v, double val) const {
    if (isPrimaryHorizontal())
        v.y = val;
    else
        v.x = val;
}

Vector2D CScrollTapeController::makeVector(double primary, double secondary) const {
    if (isPrimaryHorizontal())
        return {primary, secondary};
    else
        return {secondary, primary};
}

double CScrollTapeController::calculateMaxExtent(const CBox& usableArea, bool fullscreenOnOne) const {
    if (m_strips.empty())
        return 0.0;

    if (fullscreenOnOne && m_strips.size() == 1)
        return getPrimary(usableArea.size());

    double       total         = 0.0;
    const double usablePrimary = getPrimary(usableArea.size());

    for (const auto& strip : m_strips) {
        total += usablePrimary * strip.size;
    }

    return total;
}

double CScrollTapeController::calculateStripStart(size_t stripIndex, const CBox& usableArea, bool fullscreenOnOne) const {
    if (stripIndex >= m_strips.size())
        return 0.0;

    const double usablePrimary = getPrimary(usableArea.size());
    double       current       = 0.0;

    for (size_t i = 0; i < stripIndex; ++i) {
        const double stripSize = (fullscreenOnOne && m_strips.size() == 1) ? usablePrimary : usablePrimary * m_strips[i].size;
        current += stripSize;
    }

    return current;
}

double CScrollTapeController::calculateStripSize(size_t stripIndex, const CBox& usableArea, bool fullscreenOnOne) const {
    if (stripIndex >= m_strips.size())
        return 0.0;

    const double usablePrimary = getPrimary(usableArea.size());

    if (fullscreenOnOne && m_strips.size() == 1)
        return usablePrimary;

    return usablePrimary * m_strips[stripIndex].size;
}

CBox CScrollTapeController::calculateTargetBox(size_t stripIndex, size_t targetIndex, const CBox& usableArea, const Vector2D& workspaceOffset, bool fullscreenOnOne) {
    if (stripIndex >= m_strips.size())
        return {};

    const auto& strip = m_strips[stripIndex];
    if (targetIndex >= strip.targetSizes.size())
        return {};

    const double usableSecondary = getSecondary(usableArea.size());
    const double usablePrimary   = getPrimary(usableArea.size());
    const double cameraOffset    = calculateCameraOffset(usableArea, fullscreenOnOne);

    // calculate position along primary axis (strip position)
    double primaryPos  = calculateStripStart(stripIndex, usableArea, fullscreenOnOne);
    double primarySize = calculateStripSize(stripIndex, usableArea, fullscreenOnOne);

    // calculate position along secondary axis (within strip)
    double secondaryPos = 0.0;
    for (size_t i = 0; i < targetIndex; ++i) {
        secondaryPos += strip.targetSizes[i] * usableSecondary;
    }
    double secondarySize = strip.targetSizes[targetIndex] * usableSecondary;

    // apply camera offset based on direction
    // for RIGHT/DOWN: scroll offset moves content left/up (subtract)
    // for LEFT/UP: scroll offset moves content right/down (different coordinate system)
    if (m_direction == SCROLL_DIR_LEFT) {
        // LEFT: flip the entire primary axis, then apply offset
        primaryPos = usablePrimary - primaryPos - primarySize + cameraOffset;
    } else if (m_direction == SCROLL_DIR_UP) {
        // UP: flip the entire primary axis, then apply offset
        primaryPos = usablePrimary - primaryPos - primarySize + cameraOffset;
    } else {
        // RIGHT/DOWN: normal offset
        primaryPos -= cameraOffset;
    }

    // create the box in primary/secondary coordinates
    Vector2D pos  = makeVector(primaryPos, secondaryPos);
    Vector2D size = makeVector(primarySize, secondarySize);

    // translate to workspace position
    pos = pos + workspaceOffset;

    return CBox{pos, size};
}

double CScrollTapeController::calculateCameraOffset(const CBox& usableArea, bool fullscreenOnOne) {
    const double maxExtent     = calculateMaxExtent(usableArea, fullscreenOnOne);
    const double usablePrimary = getPrimary(usableArea.size());

    // don't adjust the offset if we are dragging
    if (isBeingDragged())
        return m_offset;

    // if the content fits in viewport, center it
    if (maxExtent < usablePrimary)
        m_offset = std::round((maxExtent - usablePrimary) / 2.0);

    // if the offset is negative but we already extended, reset offset to 0
    if (maxExtent > usablePrimary && m_offset < 0.0)
        m_offset = 0.0;

    return m_offset;
}

Vector2D CScrollTapeController::getCameraTranslation(const CBox& usableArea, bool fullscreenOnOne) {
    const double offset = calculateCameraOffset(usableArea, fullscreenOnOne);

    if (isReversed())
        return makeVector(offset, 0.0);
    else
        return makeVector(-offset, 0.0);
}

void CScrollTapeController::centerStrip(size_t stripIndex, const CBox& usableArea, bool fullscreenOnOne) {
    if (stripIndex >= m_strips.size())
        return;

    const double usablePrimary = getPrimary(usableArea.size());
    const double stripStart    = calculateStripStart(stripIndex, usableArea, fullscreenOnOne);
    const double stripSize     = calculateStripSize(stripIndex, usableArea, fullscreenOnOne);

    m_offset = stripStart - (usablePrimary - stripSize) / 2.0;
}

void CScrollTapeController::fitStrip(size_t stripIndex, const CBox& usableArea, bool fullscreenOnOne) {
    if (stripIndex >= m_strips.size())
        return;

    const double usablePrimary = getPrimary(usableArea.size());
    const double stripStart    = calculateStripStart(stripIndex, usableArea, fullscreenOnOne);
    const double stripSize     = calculateStripSize(stripIndex, usableArea, fullscreenOnOne);

    m_offset = std::clamp(m_offset, stripStart - usablePrimary + stripSize, stripStart);
}

bool CScrollTapeController::isStripVisible(size_t stripIndex, const CBox& usableArea, bool fullscreenOnOne) const {
    if (stripIndex >= m_strips.size())
        return false;

    const double stripStart = calculateStripStart(stripIndex, usableArea, fullscreenOnOne);
    const double stripEnd   = stripStart + calculateStripSize(stripIndex, usableArea, fullscreenOnOne);
    const double viewStart  = m_offset;
    const double viewEnd    = m_offset + getPrimary(usableArea.size());

    return stripStart < viewEnd && viewStart < stripEnd;
}

size_t CScrollTapeController::getStripAtCenter(const CBox& usableArea, bool fullscreenOnOne) const {
    if (m_strips.empty())
        return 0;

    const double usablePrimary = getPrimary(usableArea.size());
    double       currentPos    = m_offset;

    for (size_t i = 0; i < m_strips.size(); ++i) {
        const double stripSize = calculateStripSize(i, usableArea, fullscreenOnOne);
        currentPos += stripSize;

        if (currentPos >= usablePrimary / 2.0 - 2.0)
            return i;
    }

    return m_strips.empty() ? 0 : m_strips.size() - 1;
}

void CScrollTapeController::swapStrips(size_t a, size_t b) {
    if (a >= m_strips.size() || b >= m_strips.size())
        return;

    std::swap(m_strips.at(a), m_strips.at(b));
}

bool CScrollTapeController::isBeingDragged() const {
    for (const auto& s : m_strips) {
        if (!s.userData)
            continue;

        for (const auto& d : s.userData->targetDatas) {
            if (d->target == g_layoutManager->dragController()->target())
                return true;
        }
    }

    return false;
}
