#pragma once

#include "FocusBlockReasons.hpp"

namespace Desktop::View {
    class IFocusable {
      public:
        virtual ~IFocusable();

        // Whether the surface intrinsically accepts input. Desktop state may still block it.
        virtual bool focusAvailable() const = 0;

        bool         acceptsInput() const;
        void         setInputBlocked(eFocusBlockReason reason, bool blocked);
        bool         isInputBlocked() const;
        bool         isInputBlockedReasonAnyOf(FocusBlockReasons reasons) const;
        bool         noInputBlockedReasonsBesides(FocusBlockReasons reasons) const;
        bool         hasInputBlockedReasonsBesides(FocusBlockReasons reasons) const;

      protected:
        IFocusable();

        virtual void onInputBlockStateUpdated(bool blocked);

      private:
        FocusBlockReasons m_reasons = FOCUS_BLOCK_NONE;
    };
}
