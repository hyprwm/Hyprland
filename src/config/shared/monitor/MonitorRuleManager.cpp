#include "MonitorRuleManager.hpp"

#include "../../../debug/log/Logger.hpp"
#include "../../../protocols/OutputManagement.hpp"
#include "../../../helpers/Monitor.hpp"
#include "../../../Compositor.hpp"
#include "../../../render/Renderer.hpp"
#include "../../../event/EventBus.hpp"
#include "../../../managers/eventLoop/EventLoopManager.hpp"

#include <ranges>

using namespace Config;

UP<CMonitorRuleManager>& Config::monitorRuleMgr() {
    static UP<CMonitorRuleManager> p = makeUnique<CMonitorRuleManager>();
    return p;
}

CMonitorRuleManager::CMonitorRuleManager() {
    m_listeners.preChecksRender = Event::bus()->m_events.render.preChecks.listen([this](PHLMONITOR m) {
        if (m_reloadScheduled)
            performMonitorReload();

        m_reloadScheduled = false;
    });
}

void CMonitorRuleManager::clear() {
    m_rules.clear();
}

void CMonitorRuleManager::add(CMonitorRule&& x) {
    std::erase_if(m_rules, [&x](const auto& e) { return e.m_name == x.m_name; });
    m_rules.emplace_back(std::move(x));

    scheduleReload();
}

CMonitorRule CMonitorRuleManager::get(const PHLMONITOR PMONITOR) {
    auto applyWlrOutputConfig = [PMONITOR](CMonitorRule rule) -> CMonitorRule {
        const auto CONFIG = PROTO::outputManagement->getOutputStateFor(PMONITOR);

        if (!CONFIG)
            return rule;

        Log::logger->log(Log::DEBUG, "CConfigManager::getMonitorRuleFor: found a wlr_output_manager override for {}", PMONITOR->m_name);

        Log::logger->log(Log::DEBUG, " > overriding enabled: {} -> {}", !rule.m_disabled, !CONFIG->enabled);
        rule.m_disabled = !CONFIG->enabled;

        if ((CONFIG->committedProperties & OUTPUT_HEAD_COMMITTED_MODE) || (CONFIG->committedProperties & OUTPUT_HEAD_COMMITTED_CUSTOM_MODE)) {
            Log::logger->log(Log::DEBUG, " > overriding mode: {:.0f}x{:.0f}@{:.2f}Hz -> {:.0f}x{:.0f}@{:.2f}Hz", rule.m_resolution.x, rule.m_resolution.y, rule.m_refreshRate,
                             CONFIG->resolution.x, CONFIG->resolution.y, CONFIG->refresh / 1000.F);
            rule.m_resolution  = CONFIG->resolution;
            rule.m_refreshRate = CONFIG->refresh / 1000.F;
        }

        if (CONFIG->committedProperties & OUTPUT_HEAD_COMMITTED_POSITION) {
            Log::logger->log(Log::DEBUG, " > overriding offset: {:.0f}, {:.0f} -> {:.0f}, {:.0f}", rule.m_offset.x, rule.m_offset.y, CONFIG->position.x, CONFIG->position.y);
            rule.m_offset = CONFIG->position;
        }

        if (CONFIG->committedProperties & OUTPUT_HEAD_COMMITTED_TRANSFORM) {
            Log::logger->log(Log::DEBUG, " > overriding transform: {} -> {}", sc<uint8_t>(rule.m_transform), sc<uint8_t>(CONFIG->transform));
            rule.m_transform = CONFIG->transform;
        }

        if (CONFIG->committedProperties & OUTPUT_HEAD_COMMITTED_SCALE) {
            Log::logger->log(Log::DEBUG, " > overriding scale: {} -> {}", sc<uint8_t>(rule.m_scale), sc<uint8_t>(CONFIG->scale));
            rule.m_scale = CONFIG->scale;
        }

        if (CONFIG->committedProperties & OUTPUT_HEAD_COMMITTED_ADAPTIVE_SYNC) {
            Log::logger->log(Log::DEBUG, " > overriding vrr: {} -> {}", rule.m_vrr.value_or(0), CONFIG->adaptiveSync);
            rule.m_vrr = sc<int>(CONFIG->adaptiveSync);
        }

        return rule;
    };

    for (auto const& r : m_rules | std::views::reverse) {
        if (PMONITOR->matchesStaticSelector(r.m_name))
            return applyWlrOutputConfig(r);
    }

    Log::logger->log(Log::WARN, "No rule found for {}, trying to use the first.", PMONITOR->m_name);

    for (auto const& r : m_rules) {
        if (r.m_name.empty())
            return applyWlrOutputConfig(r);
    }

    Log::logger->log(Log::WARN, "No rules configured. Using the default hardcoded one.");

    CMonitorRule fallbackRule;
    fallbackRule.m_autoDir    = eAutoDirs::DIR_AUTO_RIGHT;
    fallbackRule.m_name       = "";
    fallbackRule.m_resolution = Vector2D{};
    fallbackRule.m_offset     = Vector2D{-INT32_MAX, -INT32_MAX};
    fallbackRule.m_scale      = -1;
    return applyWlrOutputConfig(fallbackRule);
}

const std::vector<CMonitorRule>& CMonitorRuleManager::all() {
    return m_rules;
}

std::vector<CMonitorRule>& CMonitorRuleManager::allMut() {
    return m_rules;
}

void CMonitorRuleManager::scheduleReload() {
    if (m_reloadScheduled)
        return;

    m_reloadScheduled = true;
}

void CMonitorRuleManager::performMonitorReload() {
    bool overAgain = false;

    for (auto const& m : g_pCompositor->m_realMonitors) {
        if (!m->m_output || m->m_isUnsafeFallback)
            continue;

        auto rule = get(m);

        if (!m->applyMonitorRule(Config::CMonitorRule{rule})) {
            overAgain = true;
            break;
        }

        // ensure mirror
        m->setMirror(rule.m_mirrorOf);

        g_pHyprRenderer->arrangeLayersForMonitor(m->m_id);
    }

    if (overAgain)
        performMonitorReload();

    m_reloadScheduled = false;

    Event::bus()->m_events.monitor.layoutChanged.emit();
}

void CMonitorRuleManager::ensureMonitorStatus() {
    for (auto const& rm : g_pCompositor->m_realMonitors) {
        if (!rm->m_output || rm->m_isUnsafeFallback)
            continue;

        auto rule = get(rm);

        if (rule.m_disabled == rm->m_enabled)
            rm->applyMonitorRule(std::move(rule));
    }
}

void CMonitorRuleManager::ensureVRR(PHLMONITOR pMonitor) {
    static auto PVRR = CConfigValue<Config::INTEGER>("misc:vrr");

    static auto ensureVRRForDisplay = [&](PHLMONITOR m) -> void {
        if (!m->m_output || m->m_createdByUser)
            return;

        const auto USEVRR = m->m_activeMonitorRule.m_vrr.has_value() ? m->m_activeMonitorRule.m_vrr.value() : *PVRR;

        if (USEVRR == 0) {
            if (m->m_vrrActive) {
                m->m_output->state->resetExplicitFences();
                m->m_output->state->setAdaptiveSync(false);

                if (!m->m_state.commit())
                    Log::logger->log(Log::ERR, "Couldn't commit output {} in ensureVRR -> false", m->m_output->name);
            }
            m->m_vrrActive = false;
            return;
        }

        const auto PWORKSPACE = m->m_activeWorkspace;

        if (USEVRR == 1) {
            bool wantVRR = true;
            if (PWORKSPACE && PWORKSPACE->m_hasFullscreenWindow && (PWORKSPACE->m_fullscreenMode & FSMODE_FULLSCREEN))
                wantVRR = !PWORKSPACE->getFullscreenWindow()->m_ruleApplicator->noVRR().valueOrDefault();

            if (wantVRR) {
                if (!m->m_vrrActive) {
                    m->m_output->state->resetExplicitFences();
                    m->m_output->state->setAdaptiveSync(true);

                    if (!m->m_state.test()) {
                        Log::logger->log(Log::DEBUG, "Pending output {} does not accept VRR.", m->m_output->name);
                        m->m_output->state->setAdaptiveSync(false);
                    }

                    if (!m->m_state.commit())
                        Log::logger->log(Log::ERR, "Couldn't commit output {} in ensureVRR -> true", m->m_output->name);
                }
                m->m_vrrActive = true;
            } else {
                if (m->m_vrrActive) {
                    m->m_output->state->resetExplicitFences();
                    m->m_output->state->setAdaptiveSync(false);

                    if (!m->m_state.commit())
                        Log::logger->log(Log::ERR, "Couldn't commit output {} in ensureVRR -> false", m->m_output->name);
                }
                m->m_vrrActive = false;
            }
            return;
        } else if (USEVRR == 2 || USEVRR == 3) {
            if (!PWORKSPACE)
                return; // ???

            bool wantVRR = PWORKSPACE->m_hasFullscreenWindow && (PWORKSPACE->m_fullscreenMode & FSMODE_FULLSCREEN);
            if (wantVRR && PWORKSPACE->getFullscreenWindow()->m_ruleApplicator->noVRR().valueOrDefault())
                wantVRR = false;

            if (wantVRR && USEVRR == 3) {
                const auto contentType = PWORKSPACE->getFullscreenWindow()->getContentType();
                wantVRR                = contentType == NContentType::CONTENT_TYPE_GAME || contentType == NContentType::CONTENT_TYPE_VIDEO;
            }

            if (wantVRR) {
                /* fullscreen */
                m->m_vrrActive = true;

                if (!m->m_output->state->state().adaptiveSync) {
                    m->m_output->state->setAdaptiveSync(true);

                    if (!m->m_state.test()) {
                        Log::logger->log(Log::DEBUG, "Pending output {} does not accept VRR.", m->m_output->name);
                        m->m_output->state->setAdaptiveSync(false);
                    }
                }
            } else {
                m->m_vrrActive = false;

                m->m_output->state->setAdaptiveSync(false);
            }
        }
    };

    if (pMonitor) {
        ensureVRRForDisplay(pMonitor);
        return;
    }

    for (auto const& m : g_pCompositor->m_monitors) {
        ensureVRRForDisplay(m);
    }
}
