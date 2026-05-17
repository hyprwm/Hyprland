#include "FallbackState.hpp"
#include "MonitorState.hpp"

#include "../Compositor.hpp"
#include "../event/EventBus.hpp"
#include "../output/Monitor.hpp"
#include "../managers/eventLoop/EventLoopManager.hpp"

#include <aquamarine/backend/Headless.hpp>

using namespace State;

static constexpr const long long READY_TIMEOUT_TO_UNSAFE_MS = 2000;

UP<CFallbackStateKeeper>&        State::fallbackState() {
    static UP<CFallbackStateKeeper> p = makeUnique<CFallbackStateKeeper>();
    return p;
}

CFallbackStateKeeper::CFallbackStateKeeper() {
    initSignals();
}

CFallbackStateKeeper::~CFallbackStateKeeper() = default;

void CFallbackStateKeeper::initSignals() {
    m_listeners.monitorRemoved = Event::bus()->m_events.monitor.removed.listen([this](PHLMONITOR removed) {
        if (State::monitorState()->monitors().size() > 1)
            return;

        if (!State::monitorState()->monitors().empty() && State::monitorState()->monitors().front() != removed)
            return;

        if (removed == m_fallbackOutput)
            return;

        Log::logger->log(Log::DEBUG, "[FallbackStateKeeper] Monitor {} removed, entering fallback", removed->m_name);

        setFallbackActive(true);
    });

    m_listeners.newMon = Event::bus()->m_events.monitor.newMon.listen([this](PHLMONITOR added) {
        if (added->m_name == "FALLBACK") {
            if (m_fallbackOutput) {
                Log::logger->log(Log::ERR, "[FallbackStateKeeper] BUG THIS: 'FALLBACK' added but already exists");
                m_fallbackOutput->onDisconnect();
                State::monitorState()->remove(m_fallbackOutput);
                m_fallbackOutput.reset();
            }

            m_fallbackOutput                     = added;
            m_fallbackOutput->m_isUnsafeFallback = true;
        }
    });

    m_listeners.monitorAdded = Event::bus()->m_events.monitor.added.listen([this](PHLMONITOR added) {
        if (!m_fallbackActive)
            return;

        if (added == m_fallbackOutput)
            return;

        Log::logger->log(Log::DEBUG, "[FallbackStateKeeper] Monitor {} added, leaving fallback", added->m_name);

        setFallbackActive(false);
    });

    // Add a timer for startup - if we're taking more than [timeout] to init - enter fallback so that everything else
    // can run properly

    m_listeners.ready = Event::bus()->m_events.ready.listen([this] {
        m_launchTimer = makeShared<CEventLoopTimer>(
            std::chrono::milliseconds(READY_TIMEOUT_TO_UNSAFE_MS),
            [this](SP<CEventLoopTimer> self, void* data) {
                if (!State::monitorState()->monitors().empty() && (State::monitorState()->monitors().size() > 1 || State::monitorState()->monitors().front() != m_fallbackOutput)) {
                    Log::logger->log(Log::WARN, "[FallbackStateKeeper] Launch timeout of {}ms exceeded, but we have monitors?!", READY_TIMEOUT_TO_UNSAFE_MS);
                    m_launchTimer.reset();
                    return;
                }

                Log::logger->log(Log::WARN, "[FallbackStateKeeper] Launch timeout of {}ms exceeded, entering fallback state.", READY_TIMEOUT_TO_UNSAFE_MS);
                setFallbackActive(true);
                m_launchTimer.reset();
            },
            nullptr);

        g_pEventLoopManager->addTimer(m_launchTimer);
    });

    m_listeners.start = Event::bus()->m_events.start.listen([this] {
        Log::logger->log(Log::WARN, "[FallbackStateKeeper] Start fired, removing fallback timer");

        g_pEventLoopManager->removeTimer(m_launchTimer);
        m_launchTimer = nullptr;

        g_pEventLoopManager->doLater([this] { setFallbackActive(false); });
    });
}

void CFallbackStateKeeper::initOutput() {
    SP<Aquamarine::IBackendImplementation> headless;
    for (auto const& impl : g_pCompositor->m_aqBackend->getImplementations()) {
        if (impl->type() == Aquamarine::AQ_BACKEND_HEADLESS) {
            headless = impl;
            break;
        }
    }

    if (!headless) {
        Log::logger->log(Log::WARN, "[FallbackStateKeeper] No headless in prepareFallbackOutput?!");
        return;
    }

    if (!headless->createOutput(FALLBACK_OUTPUT_NAME))
        Log::logger->log(Log::ERR, "[FallbackStateKeeper] Failed to create the fallback output?!");
}

void CFallbackStateKeeper::setFallbackActive(bool enabled) {
    if (enabled == m_fallbackActive)
        return;

    if (m_fallbackStateUpdateDeferred)
        return;

    m_fallbackActive = enabled;

    g_pEventLoopManager->doLater([this] {
        m_fallbackStateUpdateDeferred = false;

        // edge case, dk if possible.
        if (m_fallbackActive == !!m_fallbackOutput)
            return;

        if (m_fallbackActive)
            initOutput();
        else if (m_fallbackOutput) {
            m_fallbackOutput->onDisconnect();
            m_fallbackOutput->m_output->destroy();
            m_fallbackOutput.reset();
        }
    });

    m_fallbackStateUpdateDeferred = true;
}

PHLMONITOR CFallbackStateKeeper::fallbackOutput() const {
    return m_fallbackOutput;
}
