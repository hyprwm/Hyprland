#pragma once

#include "../../../../helpers/math/Math.hpp"
#include "../../../../helpers/memory/Memory.hpp"
#include <vector>

namespace Layout::Tiled {

    struct SColumnData;

    enum eScrollDirection : uint8_t {
        SCROLL_DIR_RIGHT = 0,
        SCROLL_DIR_LEFT,
        SCROLL_DIR_DOWN,
        SCROLL_DIR_UP,
    };

    struct SStripData {
        float              size = 1.F;  // size along primary axis
        std::vector<float> targetSizes; // sizes along secondary axis for each target in this strip
        WP<SColumnData>    userData;

        SStripData() = default;
    };

    struct STapeLayoutResult {
        CBox   box;
        size_t stripIndex  = 0;
        size_t targetIndex = 0;
    };

    class CScrollTapeController {
      public:
        CScrollTapeController(eScrollDirection direction = SCROLL_DIR_RIGHT);
        ~CScrollTapeController() = default;

        void              setDirection(eScrollDirection dir);
        eScrollDirection  getDirection() const;
        bool              isPrimaryHorizontal() const;
        bool              isReversed() const;

        size_t            addStrip(float size = 1.0F);
        void              insertStrip(size_t afterIndex, float size = 1.0F);
        void              removeStrip(size_t index);
        size_t            stripCount() const;
        SStripData&       getStrip(size_t index);
        const SStripData& getStrip(size_t index) const;
        void              swapStrips(size_t a, size_t b);

        void              setOffset(double offset);
        double            getOffset() const;
        void              adjustOffset(double delta);

        double            calculateMaxExtent(const CBox& usableArea, bool fullscreenOnOne = false) const;
        double            calculateStripStart(size_t stripIndex, const CBox& usableArea, bool fullscreenOnOne = false) const;
        double            calculateStripSize(size_t stripIndex, const CBox& usableArea, bool fullscreenOnOne = false) const;

        CBox              calculateTargetBox(size_t stripIndex, size_t targetIndex, const CBox& usableArea, const Vector2D& workspaceOffset, bool fullscreenOnOne = false);

        double            calculateCameraOffset(const CBox& usableArea, bool fullscreenOnOne = false);
        Vector2D          getCameraTranslation(const CBox& usableArea, bool fullscreenOnOne = false);

        void              centerStrip(size_t stripIndex, const CBox& usableArea, bool fullscreenOnOne = false);
        void              fitStrip(size_t stripIndex, const CBox& usableArea, bool fullscreenOnOne = false);

        bool              isStripVisible(size_t stripIndex, const CBox& usableArea, bool fullscreenOnOne = false) const;

        size_t            getStripAtCenter(const CBox& usableArea, bool fullscreenOnOne = false) const;

      private:
        eScrollDirection        m_direction = SCROLL_DIR_RIGHT;
        std::vector<SStripData> m_strips;
        double                  m_offset = 0.0;

        double                  getPrimary(const Vector2D& v) const;
        double                  getSecondary(const Vector2D& v) const;
        void                    setPrimary(Vector2D& v, double val) const;
        void                    setSecondary(Vector2D& v, double val) const;
        bool                    isBeingDragged() const;

        Vector2D                makeVector(double primary, double secondary) const;
    };
};
