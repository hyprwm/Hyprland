#include "ScrollingSpanLayout.hpp"

#include <algorithm>
#include <cmath>
#include <functional>

using namespace Layout::Tiled;

namespace {
    bool reservationSlotOrder(const SSpanReservation& lhs, const SSpanReservation& rhs) {
        if (lhs.slot != rhs.slot)
            return lhs.slot < rhs.slot;

        return lhs.start < rhs.start;
    }
}

bool SSpanState::active() const {
    return left > 0 || right > 0;
}

bool SSpanRange::contains(size_t column) const {
    return column >= first && column <= last;
}

std::optional<SSpanRange> Layout::Tiled::spanRangeFor(size_t anchorColumn, const SSpanState& span, size_t columnCount) {
    if (columnCount == 0 || anchorColumn >= columnCount)
        return std::nullopt;

    const size_t left     = sc<size_t>(std::max(span.left, 0));
    const size_t right    = sc<size_t>(std::max(span.right, 0));
    const size_t maxRight = columnCount - 1 - anchorColumn;

    return SSpanRange{
        .first = anchorColumn > left ? anchorColumn - left : 0,
        .last  = anchorColumn + std::min(right, maxRight),
    };
}

std::optional<SSpanState> Layout::Tiled::spanAfterColumnInsert(size_t anchorColumn, const SSpanState& span, size_t insertedColumn, size_t columnCountBefore) {
    if (insertedColumn > columnCountBefore)
        return std::nullopt;

    const auto RANGE = spanRangeFor(anchorColumn, span, columnCountBefore);
    if (!RANGE)
        return std::nullopt;

    const size_t newAnchor = anchorColumn + (insertedColumn <= anchorColumn ? 1 : 0);
    const size_t newFirst  = RANGE->first + (insertedColumn <= RANGE->first ? 1 : 0);
    const size_t newLast   = RANGE->last + (insertedColumn <= RANGE->last ? 1 : 0);

    if (newFirst > newAnchor || newLast < newAnchor)
        return std::nullopt;

    return SSpanState{.left = sc<int>(newAnchor - newFirst), .right = sc<int>(newLast - newAnchor)};
}

size_t Layout::Tiled::columnInsertAfterFocusedSpan(size_t anchorColumn, const SSpanState& span, size_t columnCount) {
    const auto RANGE = spanRangeFor(anchorColumn, span, columnCount);
    if (!RANGE)
        return columnCount;

    return std::min(RANGE->last + 1, columnCount);
}

std::optional<SSpanState> Layout::Tiled::spanAfterColumnRemove(size_t anchorColumn, const SSpanState& span, size_t removedColumn, size_t columnCountBefore) {
    if (removedColumn >= columnCountBefore || removedColumn == anchorColumn)
        return std::nullopt;

    const auto RANGE = spanRangeFor(anchorColumn, span, columnCountBefore);
    if (!RANGE)
        return std::nullopt;

    const size_t newAnchor = anchorColumn - (removedColumn < anchorColumn ? 1 : 0);
    const size_t newFirst  = RANGE->first - (removedColumn < RANGE->first ? 1 : 0);
    const size_t newLast   = RANGE->last - (removedColumn <= RANGE->last ? 1 : 0);

    if (newFirst > newAnchor || newLast < newAnchor)
        return std::nullopt;

    return SSpanState{.left = sc<int>(newAnchor - newFirst), .right = sc<int>(newLast - newAnchor)};
}

bool Layout::Tiled::columnSpansSupportedForPrimaryAxis(bool primaryHorizontal) {
    return primaryHorizontal;
}

size_t Layout::Tiled::virtualSlotFor(size_t sourceIndex, size_t sourceCount, size_t destinationRealCount) {
    if (sourceCount <= 1)
        return destinationRealCount;

    const size_t sourceLast  = sourceCount - 1;
    const size_t numerator   = sourceIndex * destinationRealCount;
    const size_t denominator = sourceLast;
    const size_t mapped      = (2 * numerator + denominator - 1) / (2 * denominator);

    return std::min(mapped, destinationRealCount);
}

bool Layout::Tiled::hasSpanDependencyCycle(const std::vector<std::vector<SSpanState>>& columnSpans, size_t columnCount) {
    // Build adjacency list: anchor → destination columns.
    std::vector<std::vector<size_t>> adjacency;
    adjacency.resize(columnCount);

    for (size_t anchorIdx = 0; anchorIdx < columnCount && anchorIdx < columnSpans.size(); ++anchorIdx) {
        for (const auto& span : columnSpans[anchorIdx]) {
            if (!span.active())
                continue;

            const auto RANGE = spanRangeFor(anchorIdx, span, columnCount);
            if (!RANGE)
                continue;

            for (size_t destIdx = RANGE->first; destIdx <= RANGE->last; ++destIdx) {
                if (destIdx != anchorIdx)
                    adjacency[anchorIdx].push_back(destIdx);
            }
        }
    }

    // DFS with three-color marking to detect cycles.
    enum Color : uint8_t {
        WHITE,
        GRAY,
        BLACK
    };
    std::vector<Color>          color(columnCount, WHITE);

    std::function<bool(size_t)> dfs = [&](size_t node) -> bool {
        color[node] = GRAY;
        for (size_t neighbor : adjacency[node]) {
            if (neighbor >= columnCount)
                continue;
            if (color[neighbor] == GRAY)
                return true; // back edge
            if (color[neighbor] == WHITE && dfs(neighbor))
                return true;
        }
        color[node] = BLACK;
        return false;
    };

    for (size_t i = 0; i < columnCount; ++i) {
        if (color[i] == WHITE && dfs(i))
            return true;
    }

    return false;
}

SColumnSpanValidationResult Layout::Tiled::validateColumnReservations(const SColumnSpanValidation& column, float minRowHeight) {
    auto reservations = column.reservations;

    for (const auto& reservation : reservations) {
        if (reservation.slot > column.realTargetCount)
            return {.valid = false, .error = "reservation slot out of range"};

        if (reservation.start < 0.F || reservation.size <= 0.F || reservation.start + reservation.size > 1.F)
            return {.valid = false, .error = "reservation band out of range"};
    }

    std::ranges::sort(reservations, reservationSlotOrder);

    for (size_t i = 1; i < reservations.size(); ++i) {
        if (reservations[i - 1].start > reservations[i].start)
            return {.valid = false, .error = "reservation slot order conflicts with band order"};
    }

    std::ranges::sort(reservations, {}, &SSpanReservation::start);
    for (size_t i = 1; i < reservations.size(); ++i) {
        if (reservations[i - 1].start + reservations[i - 1].size > reservations[i].start)
            return {.valid = false, .error = "reservation bands overlap"};
    }

    // Walk reservations in vertical order. When a gap between two
    // reservations is too small for the real targets that should fit
    // there, overflow the excess targets to later gaps. Only fail
    // when the final gap cannot accommodate everything.
    size_t previousSlot    = 0;
    float  previousEnd     = 0.F;
    size_t overflowTargets = 0;

    for (const auto& reservation : reservations) {
        const size_t targetsToPlace = reservation.slot - previousSlot + overflowTargets;
        const float  gapSize        = reservation.start - previousEnd;
        const size_t fitCount       = sc<size_t>(std::floor((std::max(gapSize, 0.F) + 0.0001F) / minRowHeight));

        if (fitCount >= targetsToPlace) {
            overflowTargets = 0;
        } else {
            overflowTargets = targetsToPlace - fitCount;
        }

        previousSlot = reservation.slot;
        previousEnd  = reservation.start + reservation.size;
    }

    const size_t realTargetsInLastGap = column.realTargetCount - previousSlot + overflowTargets;
    const float  lastGapSize          = 1.F - previousEnd;

    if (lastGapSize < sc<float>(realTargetsInLastGap) * minRowHeight)
        return {.valid = false, .error = "not enough space for real targets around reservations"};

    return {};
}

bool SColumnSpanLayoutResult::valid() const {
    return validation.valid;
}

SColumnSpanLayoutResult Layout::Tiled::layoutColumnWithReservations(const SColumnSpanValidation& column, const std::vector<float>& realTargetSizes, float minRowHeight) {
    auto validation = validateColumnReservations(column, minRowHeight);
    if (!validation.valid)
        return {.validation = std::move(validation)};

    std::vector<float> sizes = realTargetSizes;
    if (sizes.size() < column.realTargetCount)
        sizes.resize(column.realTargetCount, 1.F);

    auto reservations = column.reservations;
    std::ranges::sort(reservations, reservationSlotOrder);

    SColumnSpanLayoutResult result{.validation = {}};

    size_t                  adjustedSlot = 0;
    float                   previousEnd  = 0.F;
    size_t                  virtualIndex = 0;
    const size_t            virtualCount = column.realTargetCount + reservations.size();

    // Intermediate calculations use double to avoid cumulative
    // floating-point drift in the cursor across many targets.
    // Final values are stored as float (range [0, 1]).
    auto placeGap = [&](size_t begin, size_t end, float gapStart, float gapEnd) {
        if (begin >= end)
            return;

        const double gapSize    = sc<double>(gapEnd) - sc<double>(gapStart);
        const size_t gapTargets = end - begin;
        const double reserved   = sc<double>(gapTargets) * sc<double>(minRowHeight);
        const double extra      = std::max(gapSize - reserved, 0.0);
        double       total      = 0.0;
        for (size_t i = begin; i < end; ++i)
            total += std::max(sc<double>(sizes[i]), sc<double>(minRowHeight));

        double cursor = sc<double>(gapStart);
        for (size_t i = begin; i < end; ++i) {
            const double fraction          = total > 0.0 ? std::max(sc<double>(sizes[i]), sc<double>(minRowHeight)) / total : 1.0 / sc<double>(end - begin);
            const double remainingMinSpace = sc<double>(end - i - 1) * sc<double>(minRowHeight);
            const double size              = i + 1 == end ?
                             std::max(sc<double>(gapEnd) - cursor, sc<double>(minRowHeight)) :
                             std::max(std::min(sc<double>(minRowHeight) + extra * fraction, sc<double>(gapEnd) - cursor - remainingMinSpace), sc<double>(minRowHeight));
            result.realTargets.emplace_back(
                SRealTargetLayout{.index = i, .start = sc<float>(cursor), .size = sc<float>(size), .virtualIndex = virtualIndex, .virtualCount = virtualCount});
            cursor += size;
            ++virtualIndex;
        }
    };

    for (size_t i = 0; i < reservations.size(); ++i) {
        const auto& reservation = reservations[i];

        // Adjust the slot so that the gap places only as many real
        // targets as can physically fit. Targets that cannot fit
        // overflow to later gaps.
        const size_t expectedInGap = reservation.slot - adjustedSlot;
        const float  gapSize       = std::max(reservation.start - previousEnd, 0.F);
        const size_t fitCount      = sc<size_t>(std::floor((gapSize + 0.0001F) / minRowHeight));
        const size_t placed        = std::min(expectedInGap, fitCount);

        placeGap(adjustedSlot, adjustedSlot + placed, previousEnd, reservation.start);
        adjustedSlot += placed;
        virtualIndex += (expectedInGap - placed); // overflow targets defer their virtual index
        previousEnd = reservation.start + reservation.size;
        ++virtualIndex; // reservation itself
    }

    // Final gap: place all remaining real targets.
    placeGap(adjustedSlot, column.realTargetCount, previousEnd, 1.F);

    return result;
}
