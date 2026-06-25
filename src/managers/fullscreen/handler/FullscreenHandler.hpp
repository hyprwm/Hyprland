#pragma once

#include "../../../managers/fullscreen/FullscreenController.hpp"
#include "desktop/DesktopTypes.hpp"
#include "helpers/memory/Memory.hpp"
#include "layout/algorithm/ModeAlgorithm.hpp"
#include "layout/target/Target.hpp"
#include <optional>
#include <unordered_map>
#include <utility>

namespace Fullscreen {

    struct SFullscreenRequest {
        SP<Layout::ITarget> target;
        eFullscreenMode     currentMode = FSMODE_NONE;
        eFullscreenMode     mode        = FSMODE_NONE;
    };

    class IFullscreenHandler {

        // TODO : edit below comments into a coherent manual

        // Default FS handler with overridable methods for layouts wishing to implement their own FS handlers

        // std::optional<bool> covering has true as default argument because for the default fullscreen behaviour, there is no such thing we non-covering fullscreen. In any case this value is ignored in this handler

        // If layouts decide to have custom targets that may be able to be FSed, they must make another list, as well as helper functions for them. Controller will not be handling set/get for those; all handling must be done by layout's FS Handler

      public:
        IFullscreenHandler(WP<Layout::IModeAlgorithm> algorithm);
        virtual ~IFullscreenHandler() = default;

        IFullscreenHandler()                                     = delete;
        IFullscreenHandler(const IFullscreenHandler&)            = delete;
        IFullscreenHandler(IFullscreenHandler&&)                 = delete;
        IFullscreenHandler& operator=(const IFullscreenHandler&) = delete;
        IFullscreenHandler& operator=(IFullscreenHandler&&)      = delete;

        // FS State Queries

        /// @warning only for internal FS mode
        virtual bool                isFullscreen(const SP<Layout::ITarget> target, const std::optional<eFullscreenMode> mode = std::nullopt, const std::optional<bool> covering = true);
        /// @warning only for internal FS mode
        virtual bool                hasFullscreen(const std::optional<bool> covering = true);
        /// @warning only for internal FS mode
        virtual SP<Layout::ITarget> getFullscreen(const std::optional<bool> covering = true);
        /// @warning Doesn't check if target is FS - simply returns the tracked mode
        virtual SFullscreenMode     getFullscreenModes(const SP<Layout::ITarget> target);

        // FS Request
        /// @note Only for setting internal FS mode. Prefer to call setTargetFullscreenModeClient() independently for client mode
        virtual eFullscreenRequestResult requestFullscreen(const SFullscreenRequest& request);

        // FS State Handlers

        virtual void setTargetFullscreenModeInternal(const SP<Layout::ITarget> target, const eFullscreenMode mode);
        virtual void setTargetFullscreenModeClient(const SP<Layout::ITarget> target, const eFullscreenMode mode);

        // Target Movement Between FS Handlers

        // the Target passed is guaranteed to be a FS Target
        // Must rerender by the end
        virtual void moveFullscreenTargetToHandler(const SP<Layout::ITarget> target, const std::optional<bool> covering = true);

        // Must properly remove FS properties and state of the target in preparation for move to a new workspace with potentially a new handler.
        // Simply removing the target from various lists should suffice. Re-render and re-decoration will be handled by the receiving handler.
        virtual void moveFullscreenTargetOutOfHandler(const SP<Layout::ITarget> target);

        // Optional

        // FS target hiding behaviour
        virtual void setNoMembersAboveFullscreen();

        // FS target State Syncing (cleaning up FS target list if exists, other self corrections and error mitigation)
        virtual void syncFullscreenTargets();

        // Helpers

        virtual void removeFsTarget(SP<Layout::ITarget> target, const bool recursionGuard = false);

        // Misc.

        virtual eFullscreenHandler getFullscreenHandlerName() const;

      protected:
        // Handler will never outlive its algo because algo owns its handler with UP<>
        const WP<Layout::IModeAlgorithm> m_algorithm;

        SP<Layout::CSpace>            getSpace() const;

      private:
        /// Targetss with ONLY client FS mode set.
        std::unordered_map<WP<Layout::ITarget>, SFullscreenMode> m_fsTargets;

        // Must be defined by each layout's handler
        const eFullscreenHandler FULLSCREEN_HANDLER_TYPE = FULLSCREEN_HANDLER_DEFAULT;

        // Helpers for default FS behaviour
    };

}
