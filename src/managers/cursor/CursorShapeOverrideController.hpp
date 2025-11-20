#pragma once

#include "../../helpers/memory/Memory.hpp"
#include "../../helpers/signal/Signal.hpp"

#include <array>
#include <string>

namespace Cursor {
    enum eCursorShapeOverrideGroup : uint8_t {
        // unknown group - lowest priority
        CURSOR_OVERRIDE_UNKNOWN = 0,
        // window edges for resizing from edge
        CURSOR_OVERRIDE_WINDOW_EDGE,
        // Drag and drop
        CURSOR_OVERRIDE_DND,
        // special action: Interactive::CDrag, kill, etc.
        CURSOR_OVERRIDE_SPECIAL_ACTION,

        //
        CURSOR_OVERRIDE_END,
    };

    class CShapeOverrideController {
      public:
        CShapeOverrideController()  = default;
        ~CShapeOverrideController() = default;

        CShapeOverrideController(const CShapeOverrideController&) = delete;
        CShapeOverrideController(CShapeOverrideController&)       = delete;
        CShapeOverrideController(CShapeOverrideController&&)      = delete;

        void setOverride(const std::string& name, eCursorShapeOverrideGroup group);
        void unsetOverride(eCursorShapeOverrideGroup group);

        struct {
            // if string is empty, override was cleared
            CSignalT<const std::string&> overrideChanged;
        } m_events;

      private:
        void                                         recheckOverridesResendIfChanged();

        std::array<std::string, CURSOR_OVERRIDE_END> m_overrides;
        std::string                                  m_overrideShape;
    };

    inline UP<CShapeOverrideController> overrideController = makeUnique<CShapeOverrideController>();
};