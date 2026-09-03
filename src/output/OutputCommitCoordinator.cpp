#include "OutputCommitCoordinator.hpp"

#include "Monitor.hpp"
#include "../config/ConfigValue.hpp"
#include "../managers/eventLoop/EventLoopManager.hpp"
#include "../managers/screenshare/ScreenshareManager.hpp"
#include "../protocols/PresentationTime.hpp"
#include "../render/Renderer.hpp"
#include "../desktop/view/window/Window.hpp"
#include "../debug/log/Logger.hpp"

#include <cerrno>
#include <cstring>
#include <utility>

using namespace Monitor;

COutputCommitCoordinator::COutputCommitCoordinator(CMonitor* monitor) : m_monitor(monitor) {
    ;
}

COutputCommitCoordinator::~COutputCommitCoordinator() {
    cancelPending();
}

bool COutputCommitCoordinator::asyncEnabled() const {
    static auto PASYNCCOMMIT = CConfigValue<Config::INTEGER>("render:async_commit");

    return *PASYNCCOMMIT && m_monitor && m_monitor->m_output && (m_monitor->m_output->commitCapabilities() & Aquamarine::IOutput::AQ_OUTPUT_COMMIT_CAPABILITY_QUEUED);
}

bool COutputCommitCoordinator::canBeginFrame() const {
    return !m_pending.has_value();
}

bool COutputCommitCoordinator::hasPendingCommit() const {
    return m_pending.has_value();
}

bool COutputCommitCoordinator::ownsCommit(uint64_t id) const {
    return id && m_pending && m_pending->id == id;
}

bool COutputCommitCoordinator::shouldForwardCommitEvent() const {
    return !m_pending || !m_pending->id;
}

bool COutputCommitCoordinator::deferStateCommit() {
    if (!m_pending)
        return false;

    m_stateCommitPending = true;
    return true;
}

void COutputCommitCoordinator::stageRenderedDamage(const CRegion& damage, bool copyFBPrepared) {
    m_stagedRender = SStagedRender{
        .damage         = damage,
        .copyFBPrepared = copyFBPrepared,
    };
}

std::optional<COutputCommitCoordinator::SStagedRender> COutputCommitCoordinator::takeStagedRender() {
    return std::exchange(m_stagedRender, std::nullopt);
}

bool COutputCommitCoordinator::canSubmitAsync(const SFrame& frame) const {
    if (!asyncEnabled() || m_pending)
        return false;

    const auto CAPS = m_monitor->m_output->commitCapabilities();
    if (frame.vrr && !(CAPS & Aquamarine::IOutput::AQ_OUTPUT_COMMIT_CAPABILITY_VRR))
        return false;
    if (frame.tearing && !(CAPS & Aquamarine::IOutput::AQ_OUTPUT_COMMIT_CAPABILITY_TEARING))
        return false;

    return true;
}

COutputCommitCoordinator::eSubmitResult COutputCommitCoordinator::submit(SFrame&& frame) {
    if (!m_monitor || !m_monitor->m_output) {
        if (m_monitor)
            PROTO::presentation->discardUntagged(m_monitor->m_self.lock());
        return SUBMIT_FAILED;
    }

    const auto CAPS = m_monitor->m_output->commitCapabilities();

    LOG(Log::TRACE, "COutputCommitCoordinator: submit() with caps={}", m_monitor->m_name, sc<uint32_t>(CAPS));

    if (!canSubmitAsync(frame))
        return submitSynchronously(std::move(frame));

    LOG(Log::TRACE, "COutputCommitCoordinator: submitting asynchronously for {}", m_monitor->m_name);

    Aquamarine::IOutput::SCommitOptions options;

    if (!frame.tearing && !frame.vrr && (CAPS & Aquamarine::IOutput::AQ_OUTPUT_COMMIT_CAPABILITY_TIMED))
        options.targetPresentation = m_monitor->m_output->nextVBlank();
    options.lateCursor = m_monitor->m_output->hasCursorPlane() && (CAPS & Aquamarine::IOutput::AQ_OUTPUT_COMMIT_CAPABILITY_LATE_CURSOR);

    const auto SUBMISSION = m_monitor->m_output->commitAsync(options);
    if (!SUBMISSION.id) {
        if (SUBMISSION.error == ENOTSUP)
            return submitSynchronously(std::move(frame));

        LOG(Log::TRACE, "Async output commit for {} rejected: {}", m_monitor->m_name, strerror(SUBMISSION.error));
        PROTO::presentation->discardUntagged(m_monitor->m_self.lock());
        failed(std::move(frame), true);
        return SUBMIT_FAILED;
    }

    frame.id = SUBMISSION.id;
    if (frame.damage) {
        frame.damage->commit();
        frame.damage.reset();
    }
    m_pending = std::move(frame);
    PROTO::presentation->tagQueued(m_monitor->m_self.lock(), SUBMISSION.id, m_pending->tearing, m_pending->vrr);

    return SUBMIT_ASYNC;
}

COutputCommitCoordinator::eSubmitResult COutputCommitCoordinator::submitSynchronously(SFrame&& frame) {
    LOG(Log::TRACE, "COutputCommitCoordinator: submitting synchronously for {}", m_monitor->m_name);

    PROTO::presentation->tagQueued(m_monitor->m_self.lock(), 0, frame.tearing, frame.vrr);

    bool ok = frame.kind == FRAME_DIRECT_SCANOUT ? m_monitor->m_output->commit() : m_monitor->m_state.commit();
    if (!ok && frame.kind == FRAME_COMPOSED && m_monitor->m_inFence.isValid()) {
        m_monitor->m_output->state->resetExplicitFences();
        ok = m_monitor->m_state.commit();
    }

    if (!ok) {
        PROTO::presentation->discardQueued(m_monitor->m_self.lock(), 0);
        failed(std::move(frame), true);
        return SUBMIT_FAILED;
    }

    submitted(frame, false);
    return SUBMIT_SYNCHRONOUS;
}

void COutputCommitCoordinator::onCommitResult(const Aquamarine::IOutput::SCommitResult& result) {
    if (!ownsCommit(result.id))
        return;

    if (result.status != Aquamarine::IOutput::AQ_OUTPUT_COMMIT_SUBMITTED) {
        auto frame = std::move(*m_pending);
        m_pending.reset();
        PROTO::presentation->discardQueued(m_monitor->m_self.lock(), result.id);
        LOG(Log::ERR, "Async output commit {} for {} failed: {}", result.id, m_monitor->m_name,
            result.status == Aquamarine::IOutput::AQ_OUTPUT_COMMIT_CANCELLED ? "cancelled" : strerror(result.error));
        failed(std::move(frame), false);
        flushDeferredStateCommit();
        return;
    }

    if (result.missedTarget)
        LOG(Log::TRACE, "Async output commit {} for {} missed its target", result.id, m_monitor->m_name);
    else
        LOG(Log::TRACE, "Async output commit {} for {} submitted successfully", result.id, m_monitor->m_name);

    submitted(*m_pending, true);
    m_monitor->m_events.commit.emit();
}

void COutputCommitCoordinator::submitted(SFrame& frame, bool async) {
    LOG(Log::TRACE, "COutputCommitCoordinator: submitted {}, async={}", m_monitor->m_name, async);

    if (frame.damage)
        frame.damage->commit();

    if (frame.kind == FRAME_COMPOSED) {
        if (!frame.copyFBPrepared)
            m_monitor->resources()->markMirrorFBStale(frame.renderedDamage);
        if (!m_monitor->m_mirrors.empty())
            g_pHyprRenderer->damageMirrorsWith(m_monitor->m_self.lock(), frame.renderedDamage);
    } else if (!frame.copyFBPrepared)
        m_monitor->resources()->markMirrorFBStale();

    if (frame.kind == FRAME_DIRECT_SCANOUT) {
        const auto CANDIDATE = frame.scanoutCandidate.lock();
        if (CANDIDATE) {
            if (m_monitor->m_lastScanout.expired())
                LOG(Log::DEBUG, "Entered a direct scanout to {:x}: \"{}\"", rc<uintptr_t>(CANDIDATE.get()), CANDIDATE->metadata().title());
            m_monitor->m_lastScanout           = CANDIDATE;
            m_monitor->m_directScanoutIsActive = true;
        }
    }

    if (!async && frame.kind == FRAME_COMPOSED)
        m_monitor->m_directScanoutIsActive = false;

    if (!async && frame.kind == FRAME_COMPOSED && frame.copyFBPrepared && Screenshare::mgr())
        Screenshare::mgr()->onOutputCommit(m_monitor->m_self.lock());
}

void COutputCommitCoordinator::onPresented(uint64_t id, bool presented) {
    if (!ownsCommit(id))
        return;

    if (presented && m_pending->kind == FRAME_COMPOSED && m_pending->copyFBPrepared && Screenshare::mgr())
        Screenshare::mgr()->onOutputCommit(m_monitor->m_self.lock());

    if (!presented) {
        auto frame = std::move(*m_pending);
        m_pending.reset();
        failed(std::move(frame), false);
        flushDeferredStateCommit();
        return;
    }

    m_pending.reset();
    flushDeferredStateCommit();
}

void COutputCommitCoordinator::failed(SFrame&& frame, bool rollbackSwapchain) {
    LOG(Log::TRACE, "COutputCommitCoordinator: failed for {}", m_monitor->m_name);

    if (frame.damage)
        frame.damage->rollback();
    if (rollbackSwapchain && frame.rollbackSwapchain && m_monitor->m_output && m_monitor->m_output->swapchain)
        m_monitor->m_output->swapchain->rollback();

    if (m_monitor->m_output)
        m_monitor->m_output->state->resetExplicitFences();

    if (frame.kind == FRAME_DIRECT_SCANOUT) {
        if (frame.previousFormat) {
            m_monitor->m_drmFormat = *frame.previousFormat;
            m_monitor->m_output->state->setFormat(*frame.previousFormat);
        }
        m_monitor->m_output->state->setBuffer(nullptr);
        m_monitor->m_lastScanout.reset();
        m_monitor->m_previousFSWindow.reset();
    }

    m_monitor->m_tearingState.busy = false;
    m_monitor->m_damage.damageEntire();
    scheduleRecovery();
}

void COutputCommitCoordinator::scheduleRecovery() {
    g_pEventLoopManager->doLater([monitor = m_monitor->m_self] {
        if (monitor)
            monitor->scheduleFrame(Aquamarine::IOutput::AQ_SCHEDULE_RENDER_MONITOR);
    });
}

void COutputCommitCoordinator::flushDeferredStateCommit() {
    if (!m_stateCommitPending || m_pending)
        return;

    m_stateCommitPending = false;
    if (!m_monitor->m_state.commit()) {
        m_monitor->m_damage.damageEntire();
        scheduleRecovery();
    }
}

void COutputCommitCoordinator::cancelPending() {
    m_stagedRender.reset();
    m_stateCommitPending = false;

    if (!m_pending)
        return;

    PROTO::presentation->discardQueued(m_monitor->m_self.lock(), m_pending->id);
    auto frame = std::move(*m_pending);
    m_pending.reset();
    if (frame.damage)
        frame.damage->rollback();
}
