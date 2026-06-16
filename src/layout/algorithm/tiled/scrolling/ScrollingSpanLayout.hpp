#pragma once

#include "../../../../macros.hpp"

#include <optional>
#include <string>
#include <vector>

namespace Layout::Tiled {
    struct SSpanState {
        int  prev = 0;
        int  next = 0;

        bool active() const;
    };

    struct SSpanRange {
        size_t first = 0;
        size_t last  = 0;

        bool   contains(size_t column) const;
    };

    struct SSpanReservation {
        size_t slot  = 0;
        float  start = 0.F;
        float  size  = 0.F;
    };

    struct SSpanReservationParams {
        float start = 0.F; // normalized [0, 1]
        float size  = 0.F; // normalized [0, 1]
    };

    struct SColumnSpanValidation {
        size_t                        realTargetCount = 0;
        std::vector<SSpanReservation> reservations;
    };

    struct SColumnSpanValidationResult {
        bool        valid = true;
        std::string error;
    };

    struct SRealTargetLayout {
        size_t index        = 0;
        float  start        = 0.F;
        float  size         = 0.F;
        size_t virtualIndex = 0;
        size_t virtualCount = 0;
    };

    struct SColumnSpanLayoutResult {
        SColumnSpanValidationResult    validation;
        std::vector<SRealTargetLayout> realTargets;

        bool                           valid() const;
    };

    std::optional<SSpanRange> spanRangeFor(size_t anchorColumn, const SSpanState& span, size_t columnCount);
    std::optional<SSpanState> spanAfterColumnInsert(size_t anchorColumn, const SSpanState& span, size_t insertedColumn, size_t columnCountBefore);
    size_t                    columnInsertAfterFocusedSpan(size_t anchorColumn, const SSpanState& span, size_t columnCount);
    std::optional<SSpanState> spanAfterColumnRemove(size_t anchorColumn, const SSpanState& span, size_t removedColumn, size_t columnCountBefore);
    /**
     * Map a source target's vertical position (sourceIndex of sourceCount)
     * into a virtual slot index in a destination column with destinationRealCount
     * real targets.
     *
     * Rules:
     * - First source target (index 0) maps to slot 0 (top).
     * - Last source target (index sourceCount-1) maps to slot destinationRealCount
     *   (bottom, after all real targets).
     * - Middle positions map proportionally with half-up rounding.
     * - Single or empty source returns destinationRealCount (bottom).
     *
     * The returned slot is in [0, destinationRealCount]. A slot of
     * destinationRealCount means the reservation sits below all real targets.
     */
    size_t                      virtualSlotFor(size_t sourceIndex, size_t sourceCount, size_t destinationRealCount);
    bool                        hasSpanDependencyCycle(const std::vector<std::vector<SSpanState>>& columnSpans, size_t columnCount);
    SColumnSpanValidationResult validateColumnReservations(const SColumnSpanValidation& column, float minRowHeight);
    SColumnSpanLayoutResult     layoutColumnWithReservations(const SColumnSpanValidation& column, const std::vector<float>& realTargetSizes, float minRowHeight);
};
