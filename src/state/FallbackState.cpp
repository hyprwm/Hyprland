#include "FallbackState.hpp"

#include "../Compositor.hpp"
#include "../event/EventBus.hpp"
#include "../helpers/Monitor.hpp"
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
        if (g_pCompositor->m_monitors.size() > 1)
            return;

        if (!g_pCompositor->m_monitors.empty() && g_pCompositor->m_monitors.front() != removed)
            return;

        if (removed == m_fallbackOutput)
            return;

        setFallbackActive(true);
    });

    m_listeners.newMon = Event::bus()->m_events.monitor.newMon.listen([this](PHLMONITOR added) {
        if (!m_fallbackOutput && added->m_name == "FALLBACK")
            m_fallbackOutput = added;
    });

    m_listeners.monitorAdded = Event::bus()->m_events.monitor.added.listen([this](PHLMONITOR added) {
        if (!m_fallbackActive)
            return;

        if (added == m_fallbackOutput)
            return;

        setFallbackActive(false);
    });

    // Add a timer for startup - if we're taking more than [timeout] to init - enter fallback so that everything else
    // can run properly

    m_listeners.ready = Event::bus()->m_events.ready.listen([this] {
        initOutput();

        m_launchTimer = makeShared<CEventLoopTimer>(
            std::chrono::milliseconds(READY_TIMEOUT_TO_UNSAFE_MS),
            [this](SP<CEventLoopTimer> self, void* data) {
                Log::logger->log(Log::WARN, "[FallbackStateKeeper] Launch timeout exceeded, entering fallback state.");
                setFallbackActive(true);
            },
            nullptr);

        g_pEventLoopManager->addTimer(m_launchTimer);
    });

    m_listeners.start = Event::bus()->m_events.start.listen([this] {
        g_pEventLoopManager->removeTimer(m_launchTimer);
        m_launchTimer = nullptr;

        setFallbackActive(false);
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

    m_fallbackActive = enabled;

    if (enabled)
        m_fallbackOutput->onConnect(false);
    else
        m_fallbackOutput->onDisconnect();
}

PHLMONITOR CFallbackStateKeeper::fallbackOutput() const {
    return m_fallbackOutput.lock();
}
