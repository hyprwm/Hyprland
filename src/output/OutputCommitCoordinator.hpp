#pragma once

#include "DamageRing.hpp"
#include "../SharedDefs.hpp"
#include "../desktop/DesktopTypes.hpp"

#include <aquamarine/output/Output.hpp>
#include <optional>

namespace Monitor {
    class CMonitor;

    class COutputCommitCoordinator {
      public:
        enum eFrameKind : uint8_t {
            FRAME_COMPOSED = 0,
            FRAME_DIRECT_SCANOUT,
        };

        enum eSubmitResult : uint8_t {
            SUBMIT_FAILED = 0,
            SUBMIT_SYNCHRONOUS,
            SUBMIT_ASYNC,
        };

        struct SFrame {
            eFrameKind                               kind = FRAME_COMPOSED;
            std::optional<CDamageRing::CTransaction> damage;
            CRegion                                  renderedDamage;
            PHLWINDOWREF                             scanoutCandidate;
            bool                                     rollbackSwapchain = false;
            bool                                     tearing           = false;
            bool                                     vrr               = false;
            bool                                     copyFBPrepared    = false;
            std::optional<uint32_t>                  previousFormat;
            uint64_t                                 id = 0;
        };

        struct SStagedRender {
            CRegion damage;
            bool    copyFBPrepared = false;
        };

        explicit COutputCommitCoordinator(CMonitor* monitor);
        ~COutputCommitCoordinator();

        bool                         asyncEnabled() const;
        bool                         canBeginFrame() const;
        bool                         hasPendingCommit() const;
        bool                         ownsCommit(uint64_t id) const;
        bool                         shouldForwardCommitEvent() const;
        bool                         deferStateCommit();
        void                         stageRenderedDamage(const CRegion& damage, bool copyFBPrepared);
        std::optional<SStagedRender> takeStagedRender();
        eSubmitResult                submit(SFrame&& frame);
        void                         onCommitResult(const Aquamarine::IOutput::SCommitResult& result);
        void                         onPresented(uint64_t id, bool presented);
        void                         cancelPending();

      private:
        bool                         canSubmitAsync(const SFrame& frame) const;
        eSubmitResult                submitSynchronously(SFrame&& frame);
        void                         submitted(SFrame& frame, bool async);
        void                         failed(SFrame&& frame, bool rollbackSwapchain);
        void                         scheduleRecovery();
        void                         flushDeferredStateCommit();

        CMonitor*                    m_monitor = nullptr;
        std::optional<SFrame>        m_pending;
        std::optional<SStagedRender> m_stagedRender;
        bool                         m_stateCommitPending = false;
    };
}
