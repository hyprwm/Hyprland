#pragma once

#include "../../../managers/fullscreen/FullscreenController.hpp"
#include "helpers/memory/Memory.hpp"
#include "layout/algorithm/Algorithm.hpp"
#include "layout/algorithm/ModeAlgorithm.hpp"
#include "layout/target/Target.hpp"
#include <optional>
#include <unordered_map>

namespace Fullscreen {

    struct SFullscreenRequest {
        SP<Layout::ITarget> target;
        eFullscreenMode     currentMode = FSMODE_NONE;
        eFullscreenMode     mode        = FSMODE_NONE;
    };

    class IFullscreenHandler {

      public:
        IFullscreenHandler(Layout::IModeAlgorithm* const algorithm);
        virtual ~IFullscreenHandler() = default;

        IFullscreenHandler()                                     = delete;
        IFullscreenHandler(const IFullscreenHandler&)            = delete;
        IFullscreenHandler(IFullscreenHandler&&)                 = delete;
        IFullscreenHandler& operator=(const IFullscreenHandler&) = delete;
        IFullscreenHandler& operator=(IFullscreenHandler&&)      = delete;

        // FS State Queries

        virtual bool                isFullscreen(SP<Layout::ITarget> target, const std::optional<eFullscreenMode> mode = std::nullopt, const std::optional<bool> covering = true);

        virtual bool                hasFullscreen(const std::optional<bool> covering = true);

        virtual SP<Layout::ITarget> getFullscreen(const std::optional<bool> covering = true);

        virtual SFullscreenMode     getFullscreenModes(SP<Layout::ITarget> target);

        // FS Request

        virtual eFullscreenRequestResult requestFullscreen(const SFullscreenRequest& request);

        // FS State Handlers

        virtual void setTargetFullscreenModeInternal(const SP<Layout::ITarget> target, const eFullscreenMode mode);
        virtual void setTargetFullscreenModeClient(const SP<Layout::ITarget> target, const eFullscreenMode mode);

        // Helpers

        /**
        * @brief Updates window/workspace rules, decorations for the window.
        * @warning Target must return true from FS query functions before dispatching this
        */
        virtual void updateTargetRulesAndDecos(const SP<Layout::ITarget> target);

        /**
        * @note pass the window target even if target is a part of a group: function detects if a target is a part of a group and correctly dispatches the request to the group target
        */
        virtual void setTargetSizeAndPosition(const SP<Layout::ITarget> target);

        virtual void syncTargetSizeAndPosition();

        virtual void setNoMembersAboveFullscreen();

        virtual void syncFullscreenTargets();

        virtual void removeFsTarget(SP<Layout::ITarget> target, const bool recursionGuard = false);

        // Misc.

        virtual eFullscreenHandler getFullscreenHandlerName() const;

      protected:
        // Handler will never outlive its algo because algo owns its handler with UP<>
        Layout::IModeAlgorithm* const m_algorithm;

        SP<Layout::CSpace>            getSpace() const;

        SP<Layout::CAlgorithm>        getParent() const;

      private:
        std::unordered_map<WP<Layout::ITarget>, SFullscreenMode> m_fsTargets;

        // Must be defined by each layout's handler
        const eFullscreenHandler FULLSCREEN_HANDLER_TYPE = FULLSCREEN_HANDLER_DEFAULT;
    };

}
