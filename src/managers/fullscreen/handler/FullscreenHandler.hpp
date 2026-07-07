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

        /*
        Default FS handler with overridable methods for layouts wishing to implement their own FS handlers
        
        For future layout FS handlers, use this as a template for what each method should/should not do and which extra checks it should perform

        If layouts decide to have custom targets that may be able to be FSed, they must make another list, as well as helper functions for them. Controller will not be handling set/get for those; all handling must be done by layout's FS Handler  

        Different layouts have different critera for what a window must be/have to be considered FS. Generally these should be checked and self-correct in syncFullscreenTargets().

        TODO: implement proper error recovery and correction logic for handler FS queries as well, OR preferably don't call the handler even from the algorithms and use the controller wherever possible


        Default FS Handler Specific
        ---------------------------
          std::optional<bool> covering has true as default argument because for the default fullscreen behaviour, there is no such thing we non-covering fullscreen. In any case this value is ignored in this handler
        
        */

      public:
        IFullscreenHandler(Layout::IModeAlgorithm* const algorithm);
        virtual ~IFullscreenHandler() = default;

        IFullscreenHandler()                                     = delete;
        IFullscreenHandler(const IFullscreenHandler&)            = delete;
        IFullscreenHandler(IFullscreenHandler&&)                 = delete;
        IFullscreenHandler& operator=(const IFullscreenHandler&) = delete;
        IFullscreenHandler& operator=(IFullscreenHandler&&)      = delete;

        // FS State Queries

        /// @warning only for internal FS mode
        virtual bool isFullscreen(SP<Layout::ITarget> target, const std::optional<eFullscreenMode> mode = std::nullopt, const std::optional<bool> covering = true);
        /// @warning only for internal FS mode
        virtual bool hasFullscreen(const std::optional<bool> covering = true);
        /// @warning only for internal FS mode
        virtual SP<Layout::ITarget> getFullscreen(const std::optional<bool> covering = true);
        virtual SFullscreenMode     getFullscreenModes(SP<Layout::ITarget> target);

        // FS Request
        /// @note Only for setting internal FS mode. Prefer to call setTargetFullscreenModeClient() independently for client mode
        virtual eFullscreenRequestResult requestFullscreen(const SFullscreenRequest& request);

        // FS State Handlers

        virtual void setTargetFullscreenModeInternal(const SP<Layout::ITarget> target, const eFullscreenMode mode);
        virtual void setTargetFullscreenModeClient(const SP<Layout::ITarget> target, const eFullscreenMode mode);

        // Helpers

        /**
        * @brief For settings window position and size.
        *
        * @note pass the window target even if target is a part of a group: function detects if a target is a part of a group and correctly dispatches the request to the group target
        * @warning Handlers must take care to bring the FS target to a state where it will qualify as a FS window by window/workspace rule matchers before dispatching this.
        */
        virtual void setTargetSizeAndPosition(const SP<Layout::ITarget> target);

        /// @note Optional: FS member hiding behaviour
        virtual void setNoMembersAboveFullscreen();

        /**
        * @brief FS target State Syncing (cleaning up FS target list if exists, other self corrections and error mitigation)
        *
        * @note This function is responsible for performing clean-up on the FS handler, **NOT** on the targets themselves.
        * This means that this function doesn't take care of unFSing non-FS-criterea-compliant targets; it merely untracks the target it if is offending.
        * It is assumed that the target was/will be unFSed via the apropriate functions
        * 
        * It is possible for layouts that wish to implement target unFSing to do so, but they must take care as this function is designed to be
        * called often in FS setter paths and is easy to infinite-recurse
        */
        virtual void syncFullscreenTargets();

        /**
        * @brief Un-Tracks a target.
        *
        *  @note This does **NOT** un-FS a target - It untracks the target from the handler
        */
        virtual void removeFsTarget(SP<Layout::ITarget> target, const bool recursionGuard = false);

        // Misc.

        virtual eFullscreenHandler getFullscreenHandlerName() const;

      protected:
        // Handler will never outlive its algo because algo owns its handler with UP<>
        Layout::IModeAlgorithm* const m_algorithm;

        SP<Layout::CSpace>            getSpace() const;

        SP<Layout::CAlgorithm>        getParent() const;

      private:
        /// Targetss with ONLY client FS mode set.
        std::unordered_map<WP<Layout::ITarget>, SFullscreenMode> m_fsTargets;

        // Must be defined by each layout's handler
        const eFullscreenHandler FULLSCREEN_HANDLER_TYPE = FULLSCREEN_HANDLER_DEFAULT;

        // Helpers for default FS behaviour
    };

}
