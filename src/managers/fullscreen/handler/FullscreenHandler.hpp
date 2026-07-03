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
      
      
      // ERSTARR TODO : edit below comments into a coherent manual
      /*
        Default FS handler with overridable methods for layouts wishing to implement their own FS handlers
        
        std::optional<bool> covering has true as default argument because for the default fullscreen behaviour, there is no such thing we non-covering fullscreen. In any case this value is ignored in this handler
        
        If layouts decide to have custom targets that may be able to be FSed, they must make another list, as well as helper functions for them. Controller will not be handling set/get for those; all handling must be done by layout's FS Handler  
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
        virtual bool                isFullscreen(SP<Layout::ITarget> target, const std::optional<eFullscreenMode> mode = std::nullopt, const std::optional<bool> covering = true);
        /// @warning only for internal FS mode
        virtual bool                hasFullscreen(const std::optional<bool> covering = true);
        /// @warning only for internal FS mode
        virtual SP<Layout::ITarget> getFullscreen(const std::optional<bool> covering = true);
        /// @note also checks if target isFullscreen()
        virtual SFullscreenMode     getFullscreenModes(SP<Layout::ITarget> target);

        // FS Request
        /// @note Only for setting internal FS mode. Prefer to call setTargetFullscreenModeClient() independently for client mode
        virtual eFullscreenRequestResult requestFullscreen(const SFullscreenRequest& request);

        // FS State Handlers

        virtual void setTargetFullscreenModeInternal(const SP<Layout::ITarget> target, const eFullscreenMode mode);
        virtual void setTargetFullscreenModeClient(const SP<Layout::ITarget> target, const eFullscreenMode mode);


        // optional: FS target hiding behaviour
        virtual void setNoMembersAboveFullscreen();

        /**
        * @brief FS target State Syncing (cleaning up FS target list if exists, other self corrections and error mitigation)
        *
        * @note This function is responsible for performing clean-up on the FS handler, **NOT** on the targets themselves.
        *This means that this function doesn't take care of unFSing non-FS-criterea-compliant targets; it merely untracks the target it if is offending.
        *It is assumed that the target was/will be unFSed via the apropriate functions
        
        */
        virtual void syncFullscreenTargets();

        // Helpers

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

        SP<Layout::CAlgorithm>         getParent() const;

      private:
        /// Targetss with ONLY client FS mode set.
        std::unordered_map<WP<Layout::ITarget>, SFullscreenMode> m_fsTargets;

        // Must be defined by each layout's handler
        const eFullscreenHandler FULLSCREEN_HANDLER_TYPE = FULLSCREEN_HANDLER_DEFAULT;

        // Helpers for default FS behaviour
    };

}
