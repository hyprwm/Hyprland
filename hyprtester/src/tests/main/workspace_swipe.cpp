#include "tests.hpp"
#include "../../hyprctlCompat.hpp"

#include <hyprutils/utils/ScopeGuard.hpp>
#include <array>
#include <format>
#include <string_view>
#include <thread>

namespace WorkspaceSwipeTests {
    static std::string run(std::string_view body, bool bounds = false, bool forced = false) {
        // Runs even when Lua assertions fail, before the harness reloads gesture bindings.
        Hyprutils::Utils::CScopeGuard cleanup([] {
            getFromSocket("/eval hl.plugin.test.workspace_swipe_reset()");
            getFromSocket("/eval hl.gesture({ fingers = 9, direction = 'horizontal', disable_inhibit = true, action = 'unset' })");
        });

        const auto                    SCRIPT = std::format("local bounds, forced = {}, {}\n", bounds ? "true" : "false", forced ? "true" : "false") + R"(
            local p = hl.plugin.test
            p.alt(0)
            hl.config({ animations = { enabled = false }, gestures = {
                workspace_swipe_distance = 300,
                workspace_swipe_cancel_ratio = 0.5,
                workspace_swipe_min_speed_to_force = 0,
                workspace_swipe_direction_lock = false,
                workspace_swipe_forever = false,
                workspace_swipe_invert = false,
                workspace_swipe_create_new = true,
                workspace_swipe_use_r = not bounds,
            } })
            for _, leaf in ipairs({ 'workspaces', 'workspacesIn', 'workspacesOut' }) do
                hl.animation({ leaf = leaf, enabled = true, speed = 1, bezier = 'default', style = 'slide' })
            end
            hl.gesture({ fingers = 9, direction = 'horizontal', disable_inhibit = true, action = 'workspace' })
            p.workspace_swipe_fixture(bounds, forced)

            local left = bounds and 'r-1' or '9901'
            local origin = bounds and 'r+0' or '9902'
            local right = bounds and 'name:hyprtester-swipe-wrap' or '9903'
            local snapshot = p.workspace_snapshot
            local l, o, r = snapshot(left), snapshot(origin), snapshot(right)

            local function eq(actual, expected, label)
                assert(actual == expected, label .. ': expected ' .. tostring(expected) .. ', got ' .. tostring(actual))
            end
            local function baseline(selector, expected)
                local actual = snapshot(selector)
                for _, key in ipairs({ 'exists', 'address', 'alpha', 'alpha_goal', 'offset_x', 'offset_y',
                                       'offset_goal_x', 'offset_goal_y', 'forced', 'mapped', 'active', 'monitor' }) do
                    eq(actual[key], expected[key], selector .. '.' .. key)
                end
            end
            local function neutral(selector, expected_force)
                local s = snapshot(selector)
                eq(s.exists, true, selector .. '.exists')
                eq(s.active, true, selector .. '.active')
                eq(s.mapped, true, selector .. '.mapped')
                eq(s.monitor, o.monitor, selector .. '.monitor')
                eq(s.alpha, 1, selector .. '.alpha')
                eq(s.alpha_goal, 1, selector .. '.alpha_goal')
                eq(s.offset_x, 0, selector .. '.offset_x')
                eq(s.offset_y, 0, selector .. '.offset_y')
                eq(s.offset_goal_x, 0, selector .. '.offset_goal_x')
                eq(s.offset_goal_y, 0, selector .. '.offset_goal_y')
                eq(s.forced, expected_force, selector .. '.forced')
                eq(s.swiping, false, 'unified gesture finished')
            end
            local function preview(selector)
                local s = snapshot(selector)
                eq(s.swiping, true, 'unified gesture started through trackpad')
                eq(s.forced, true, selector .. '.forced during preview')
                eq(s.alpha_goal, 1, selector .. '.preview alpha goal')
                assert(s.offset_goal_x ~= 0, selector .. ': preview must move horizontally')
                eq(s.offset_goal_y, 0, selector .. '.preview offset y')
                eq(s.active, false, selector .. ': preview is not committed')
                local active = snapshot(origin)
                eq(active.active, true, 'origin remains active during preview')
                eq(active.forced, true, 'origin forced during preview')
                assert(active.offset_goal_x ~= 0, 'origin must move during preview')
            end
            local function cleaned()
                baseline(left, l)
                baseline(right, r)
                neutral(origin, forced)
            end

            eq(l.exists, true, 'left fixture exists')
            eq(r.exists, true, 'right fixture exists')
            eq(l.mapped, false, 'left fixture inactive')
            eq(r.mapped, false, 'right fixture inactive')
            eq(l.forced, forced, 'left preexisting force')
            eq(r.forced, forced, 'right preexisting force')
            eq(l.alpha_goal, 0.25, 'left non-neutral baseline')
            eq(r.alpha_goal, 0.625, 'right non-neutral baseline')
            eq(l.offset_goal_x, 31, 'left offset baseline')
            eq(r.offset_goal_x, -29, 'right offset baseline')
            neutral(origin, forced)
        )" + std::string{body};
        return getFromSocket("/eval " + SCRIPT);
    }

    static std::string runOnOutputs(std::string_view body) {
        const std::array<std::string, 2> NAMES   = {"HYPRTEST-SWIPE-A", "HYPRTEST-SWIPE-B"};
        std::array<bool, 2>              created = {};
        Hyprutils::Utils::CScopeGuard    cleanup([&] {
            getFromSocket("/eval hl.plugin.test.workspace_swipe_reset()");
            for (size_t i = 0; i < NAMES.size(); ++i) {
                if (created[i])
                    getFromSocket("/output remove " + NAMES[i]);
            }
        });
        for (size_t i = 0; i < NAMES.size(); ++i) {
            auto result = getFromSocket(std::format("/eval hl.monitor({{ output = '{}', mode = '1920x1080@60', position = '{}x0', scale = '1' }})", NAMES[i], 20000 + i * 1920));
            if (result != "ok")
                return result;
            result = getFromSocket("/output create headless " + NAMES[i]);
            if (result != "ok")
                return result;
            created[i] = true;
            // Wait only for output readiness; swipe cleanup itself must be synchronous.
            bool ready = false;
            for (int attempt = 0; attempt < 50 && !ready; ++attempt) {
                ready = getFromSocket("/monitors").contains("Monitor " + NAMES[i] + " (ID ");
                if (!ready)
                    std::this_thread::sleep_for(std::chrono::milliseconds(20));
            }
            if (!ready)
                return "Timed out creating swipe output " + NAMES[i];
        }
        const auto FOCUSED = getFromSocket("/dispatch hl.dsp.focus({ monitor = 'HYPRTEST-SWIPE-A' })");
        if (FOCUSED != "ok")
            return FOCUSED;
        return run("p.workspace_swipe_peer('HYPRTEST-SWIPE-B')\n" + std::string{body});
    }

    TEST_CASE(workspaceSwipeReversalCleanup) {
        OK(run(R"(
            p.swipe_begin(9)
            p.swipe_update(-90)
            preview(left)
            baseline(right, r) -- An untouched neighbor must not be modified either.

            p.swipe_update(180) -- Backend delta: cumulative position is now +90.
            preview(right)
            baseline(left, l)

            p.swipe_update(-180)
            preview(left)
            baseline(right, r)
            p.workspace_swipe_cancel()
            cleaned()
            p.swipe_end(false)
            cleaned()
        )"));
    }

    TEST_CASE(workspaceSwipeMissingNeighborCleanup) {
        for (const int DIRECTION : {-1, 1}) {
            OK(run(std::format(R"(
                local direction = {}
                local missing = direction > 0 and '9904' or '9900'
                local untouched = direction > 0 and left or right
                -- Start at a fixture edge: 9902 exists on one side, the other is uncreated.
                origin = direction > 0 and right or left
                hl.dispatch(hl.dsp.focus({{ workspace = origin }}))
                o = snapshot(origin) -- The hook ticks disabled animations synchronously.
                local neighbor = snapshot('9902')
                local untouched_baseline = snapshot(untouched)
                neutral(origin, false)
                eq(snapshot(missing).exists, false, 'destination starts uncreated')
                eq(neighbor.forced, false, 'preview baseline is not forced')

                p.swipe_begin(9)
                p.swipe_update(-direction * 90)
                preview('9902')
                assert(snapshot('9902').offset_goal_x ~= neighbor.offset_goal_x, 'preview must change neighbor offset')

                p.swipe_update(direction * 180) -- Reverse to cumulative +/-90 toward the missing target.
                eq(snapshot('9902').forced, false, 'abandoned preview force released before end')
                baseline('9902', neighbor)
                baseline(untouched, untouched_baseline)
                eq(snapshot(missing).exists, false, 'reversal must not create its destination')
                local active = snapshot(origin)
                eq(active.active, true, 'origin remains active')
                eq(active.swiping, true, 'valid uncreated target keeps the swipe active')
                eq(active.forced, true, 'origin remains forced during the swipe')
                assert(active.offset_goal_x * direction < 0, 'origin must move toward the missing target')

                p.workspace_swipe_cancel()
                p.swipe_end(false)
                neutral(origin, false)
                baseline('9902', neighbor)
                baseline(untouched, untouched_baseline)
                eq(snapshot(missing).exists, false, 'cancelled swipe must not create its destination')
            )",
                               DIRECTION)));
        }
    }

    TEST_CASE(workspaceSwipeCancelCleanup) {
        OK(run(R"(
            p.swipe_begin(9)
            p.swipe_update(-90)
            preview(left)
            p.swipe_update(180)
            preview(right)
            p.workspace_swipe_cancel()
            cleaned()
            p.workspace_swipe_cancel() -- Cancellation is idempotent.
            p.swipe_end(true)
            cleaned()

            -- A cancelled session must not poison the next trackpad gesture.
            p.swipe_begin(9)
            p.swipe_update(-180)
            preview(left)
            p.swipe_end(false)
            neutral(left, false)
            eq(snapshot(origin).forced, false, 'origin force released after next commit')
            baseline(right, r)
        )"));
    }

    TEST_CASE(workspaceSwipeInvalidUpdateCleanup) {
        OK(run(R"(
            p.swipe_begin(9)
            p.swipe_update(-90)
            preview(left)
            p.swipe_update(180)
            preview(right)

            -- The typed UINT32_MAX origin has valid m neighbors but no real r+1 target.
            hl.config({ gestures = { workspace_swipe_use_r = true } })
            p.swipe_update(10)
            cleaned()
            p.swipe_end(false)
            cleaned()
        )",
               true));
    }

    TEST_CASE(workspaceSwipeInvalidEndCleanup) {
        OK(run(R"(
            p.swipe_begin(9)
            p.swipe_update(-90)
            preview(left)
            p.swipe_update(180)
            preview(right)

            hl.config({ gestures = { workspace_swipe_use_r = true } })
            -- Direct end isolates its invalid-target branch from update's abort branch.
            -- The hook verifies r+1 is rejected by the real resolver before calling end().
            p.workspace_swipe_invalid_end()
            cleaned()
            p.swipe_end(false)
            cleaned()
        )",
               true));
    }

    TEST_CASE(workspaceSwipePreexistingForce) {
        OK(run(R"(
            p.swipe_begin(9)
            p.swipe_update(-90)
            preview(left)
            baseline(right, r)
            p.swipe_update(180)
            preview(right)
            baseline(left, l)
            p.workspace_swipe_cancel()
            cleaned()
            p.swipe_end(false)
            cleaned()
        )",
               false, true));

        for (const auto DISTANCE : {90, 180}) {
            OK(run(std::format(R"(
                p.swipe_begin(9)
                p.swipe_update({})
                preview(right)
                p.swipe_end(false)
                neutral({}, true)
                eq(snapshot(left).forced, true, 'preexisting left force survives end')
                eq(snapshot(origin).forced, true, 'preexisting origin force survives end')
                eq(snapshot(right).forced, true, 'preexisting right force survives end')
                baseline(left, l)
            )",
                               DISTANCE, DISTANCE < 150 ? "origin" : "right"),
                   false, true));
        }

        OK(run(R"(
            p.swipe_begin(9)
            p.swipe_update(-90)
            preview(left)
            hl.config({ gestures = { workspace_swipe_use_r = true } })
            p.workspace_swipe_invalid_end()
            cleaned()
            p.swipe_end(true)
        )",
               true, true));
    }

    TEST_CASE(workspaceSwipeCommitRollbackThresholds) {
        // Backend cancellation historically still runs the ordinary workspace end policy.
        for (const bool CANCELLED : {false, true}) {
            for (const int DIRECTION : {-1, 1}) {
                for (const int DISTANCE : {1, 149, 150, 300}) {
                    OK(run(std::format(R"(
                        local direction, distance, cancelled = {}, {}, {}
                        local destination = direction < 0 and left or right
                        local untouched = direction < 0 and right or left
                        local untouched_baseline = direction < 0 and r or l
                        p.swipe_begin(9)
                        p.swipe_update(direction * 5) -- Cross trackpad recognition threshold.
                        preview(destination)
                        baseline(untouched, untouched_baseline)
                        p.swipe_update(direction * (distance - 5))
                        p.swipe_end(cancelled)
                        neutral(distance < 150 and origin or destination, false)
                        eq(snapshot(left).forced, false, 'left force released')
                        eq(snapshot(origin).forced, false, 'origin force released')
                        eq(snapshot(right).forced, false, 'right force released')
                    )",
                                       DIRECTION, DISTANCE, CANCELLED ? "true" : "false")));
                }
            }
        }
    }

    TEST_CASE(workspaceSwipeSpeedThresholds) {
        for (const int DISTANCE : {29, 30}) {
            OK(run(std::format(R"(
                hl.config({{ gestures = {{ workspace_swipe_min_speed_to_force = 30 }} }})
                p.swipe_begin(9)
                p.swipe_update({})
                preview(right)
                p.swipe_end(false)
                neutral({}, false)
                baseline(left, l)
            )",
                               DISTANCE, DISTANCE < 30 ? "origin" : "right")));
        }

        OK(run(R"(
            hl.config({ gestures = { workspace_swipe_min_speed_to_force = 30 } })
            p.swipe_begin(9)
            p.swipe_update(100)
            preview(right)
            baseline(left, l)
            p.swipe_update(-99) -- Fast motion still rolls back below the absolute two-pixel floor.
            p.swipe_end(false)
            neutral(origin, false)
            eq(snapshot(right).forced, false, 'preview force released below two-pixel floor')
        )"));
    }

    TEST_CASE(workspaceSwipeOriginOutputFocus) {
        OK(runOnOutputs(R"(
            local peer = snapshot('9910')
            p.swipe_begin(9)
            p.swipe_update(-90)
            preview(left)
            local token = snapshot(origin).session_id
            assert(token ~= 0, 'session must have a token')
            hl.dispatch(hl.dsp.focus({ monitor = 'HYPRTEST-SWIPE-B' }))
            p.swipe_update(-90)
            eq(snapshot(origin).session_id, token, 'focus change retains origin session')
            preview(left)
            p.swipe_end(false)
            neutral(left, false)
            eq(snapshot(left).monitor, 'HYPRTEST-SWIPE-A', 'commit stays on origin output')
            baseline('9910', peer)
        )"));
    }

    TEST_CASE(workspaceSwipeOriginInvalidated) {
        for (const bool MOVE : {false, true}) {
            OK(runOnOutputs(std::string{R"(
                p.swipe_begin(9)
                p.swipe_update(-90)
                preview(left)
            )"} + (MOVE ? "hl.dispatch(hl.dsp.workspace.move({ workspace = origin, monitor = 'HYPRTEST-SWIPE-B' }))\n" : "hl.dispatch(hl.dsp.focus({ workspace = right }))\n") +
                            R"(
                -- No update/end/cancel before these checks: lifecycle signals must abort a paused swipe.
                eq(snapshot(origin).session_id, 0, 'origin invalidation cancels immediately')
                eq(snapshot(origin).swiping, false, 'no paused unified session')
                eq(snapshot(origin).forced, false, 'origin force released')
                baseline(left, l)
                local placement = snapshot(origin)
                local replacement = snapshot(right)
                p.swipe_end(false)
                baseline(origin, placement)
                baseline(right, replacement)
            )"));
        }
    }

    TEST_CASE(workspaceSwipeOriginOutputSwapped) {
        OK(runOnOutputs(R"(
            local peer = snapshot('9910')
            p.swipe_begin(9)
            p.swipe_update(-90)
            preview(left)
            hl.dispatch(hl.dsp.workspace.swap_monitors({ monitor1 = 'HYPRTEST-SWIPE-A', monitor2 = 'HYPRTEST-SWIPE-B' }))
            eq(snapshot(origin).session_id, 0, 'swap cancels the paused swipe synchronously')
            o.monitor = 'HYPRTEST-SWIPE-B'
            neutral(origin, false) -- The moved origin must lose its gesture-authored displacement.
            baseline(left, l)
            baseline(right, r)
            peer.monitor = 'HYPRTEST-SWIPE-A'
            baseline('9910', peer)
            p.swipe_end(false)
            neutral(origin, false)
            baseline('9910', peer)
        )"));
    }

    TEST_CASE(workspaceSwipeReentrantEndOwnership) {
        for (const bool FOREVER : {false, true}) {
            for (const bool REDIRECT : {false, true}) {
                for (const int DIRECTION : {-1, 1}) {
                    OK(run(std::format(R"(
                        hl.config({{ gestures = {{ workspace_swipe_forever = {} }} }})
                        -- The probe checks the callback's token, exact offsets, alpha and both force holds
                        -- immediately after the outer end/forever update returns, before an animation tick.
                        p.workspace_swipe_reentrant_end_probe({}, {})
                        local replacement = snapshot(origin)
                        assert(replacement.swiping and replacement.session_id ~= 0, 'replacement remains live')
                        p.workspace_swipe_cancel()
                        eq(snapshot(origin).forced, false, 'replacement origin/neighbor hold released by its own cancel')
                        eq(snapshot(left).forced, false, 'left hold released by replacement cancel')
                        eq(snapshot(right).forced, false, 'right hold released by replacement cancel')
                    )",
                                       FOREVER ? "true" : "false", DIRECTION, REDIRECT ? "true" : "false")));
                }
            }
        }
    }

    TEST_CASE(workspaceSwipeOriginOutputRemoved) {
        OK(runOnOutputs(R"(
            p.swipe_begin(9)
            p.swipe_update(-90)
            preview(left)
            p.workspace_swipe_remove_origin_output()
            eq(snapshot(origin).session_id, 0, 'output removal cancels without more input')
            eq(snapshot(origin).swiping, false, 'removed output has no pending swipe')
            for _, selector in ipairs({ left, origin, right }) do
                local s = snapshot(selector)
                eq(s.exists, true, 'held fixture survives output migration')
                eq(s.forced, false, 'output removal releases participant force')
                assert(s.monitor ~= 'HYPRTEST-SWIPE-A', 'workspace migrated off removed output')
            end
            local placement = snapshot(origin)
            p.swipe_end(false)
            baseline(origin, placement)
        )"));
    }

    TEST_CASE(workspaceSwipeForeverSession) {
        OK(run(R"(
            hl.config({ gestures = { workspace_swipe_forever = true } })
            p.swipe_begin(9)
            p.swipe_update(90)
            preview(right)
            local token = snapshot(origin).session_id
            assert(token ~= 0, 'initial session token')
            p.swipe_update(210)
            eq(snapshot(right).active, true, 'first segment committed')
            eq(snapshot(right).session_id, token, 'forever restart preserves token')
            origin = right
            p.swipe_update(-90)
            preview('9902') -- The original adapter must still own the restarted segment.
            eq(snapshot(origin).session_id, token, 'adapter continues same session')
            p.swipe_end(false)
            neutral(origin, false)
            eq(snapshot(origin).session_id, 0, 'backend end finishes restarted segment')
            baseline(left, l)
        )"));
    }

    TEST_CASE(workspaceSwipeTrackpadOwnership) {
        OK(run(R"(
            p.workspace_swipe_adapter_probe(false)
            cleaned()
            p.workspace_swipe_adapter_probe(true)
            cleaned()
        )"));
    }

    TEST_CASE(workspaceSwipeTouchDeviceIdentity) {
        OK(run(R"(
            hl.config({ gestures = { workspace_swipe_touch = true, workspace_swipe_touch_invert = false } })
            p.workspace_swipe_touch_setup()
            local touch, state = p.workspace_swipe_touch, p.workspace_swipe_touch_snapshot
            touch('down', 0, 0, 0.0)
            local token = snapshot(origin).session_id
            assert(token ~= 0 and state(0, 0).owner, 'touch ID zero owns a session')
            touch('motion', 0, 0, 0.3)
            preview(left)
            local offset = snapshot(origin).offset_goal_x
            touch('down', 1, 0, 0.0)
            eq(state(1, 0).consumed, true, 'same ID on second device is separately consumed')
            eq(state(1, 0).owner, false, 'second device does not own the swipe')
            touch('motion', 1, 0, 0.8)
            eq(snapshot(origin).offset_goal_x, offset, 'foreign motion cannot drive owner')
            touch('up', 1, 0)
            eq(snapshot(origin).session_id, token, 'foreign up cannot finish owner')
            eq(state(0, 0).consumed, true, 'owner contact survives foreign up')
            touch('down', 1, 0, 0.0)
            touch('cancel', 1, 0)
            eq(snapshot(origin).session_id, token, 'foreign hardware cancel cannot cancel owner')
            touch('motion', 0, 0, 0.6)
            touch('up', 0, 0)
            neutral(left, false)
            eq(state(0, 0).consumed, false, 'owner up releases consumption')
        )"));
    }

    TEST_CASE(workspaceSwipeTouchConsumedAfterCancel) {
        Hyprutils::Utils::CScopeGuard clientCleanup([] { getFromSocket("/dispatch hl.dsp.window.kill({ window = 'class:swipe-touch-client' })"); });
        SPAWN_KITTY("swipe-touch-client");
        for (const bool HARDWARE : {false, true}) {
            OK(run(std::string{R"(
                hl.dispatch(hl.dsp.window.move({ window = 'class:swipe-touch-client', workspace = origin }))
                snapshot(origin) -- Settle the disabled window/workspace animations before hit testing.
                hl.config({ gestures = { workspace_swipe_touch = true, workspace_swipe_touch_invert = false } })
                p.workspace_swipe_touch_setup()
                local touch, state = p.workspace_swipe_touch, p.workspace_swipe_touch_snapshot
                touch('down', 0, 7, 0.5) -- An ordinary client contact must survive gesture cancellation.
                eq(state(0, 7).consumed, false, 'central client contact is not consumed')
                eq(state(0, 7).client_focus, true, 'client touch focus established')
                local focus_events = state(0, 7).focus_events
                assert(focus_events > 0, 'client received touch-down focus')
                touch('down', 0, 0, 0.0)
                touch('motion', 0, 0, 0.3)
                preview(left)
                local owner_token = snapshot(origin).session_id
                touch('cancel', 0, 7) -- Non-gesture hardware cancel must not cancel the gesture or client.
                eq(snapshot(origin).session_id, owner_token, 'unconsumed cancel leaves gesture owner alone')
                eq(state(0, 7).focus_events, focus_events, 'unconsumed cancel leaves client contact alone')
                touch('down', 1, 0, 0.0)
            )"} + (HARDWARE ? "touch('cancel', 0, 0)\n" : "p.workspace_swipe_cancel()\n") +
                   R"(
                cleaned()
                eq(state(1, 0).consumed, true, 'suppressed contact survives owner cancellation')
                -- A fresh trackpad session must not be driven or ended by old touch events.
                p.swipe_begin(9)
                p.swipe_update(-90)
                local token, offset = snapshot(origin).session_id, snapshot(origin).offset_goal_x
                touch('motion', 1, 0, 0.8)
                eq(snapshot(origin).offset_goal_x, offset, 'consumed motion does not drive new session')
                touch('up', 1, 0)
                eq(snapshot(origin).session_id, token, 'consumed up does not end new session')
                eq(state(0, 7).focus_events, focus_events, 'gesture termination did not release client contact')
                eq(state(1, 0).consumed, false, 'suppressed up releases consumption')
            )" + (HARDWARE ? "" : R"(
                touch('motion', 0, 0, 0.8)
                eq(snapshot(origin).offset_goal_x, offset, 'stale owner motion does not drive new session')
                touch('up', 0, 0)
                eq(snapshot(origin).session_id, token, 'stale owner up does not end new session')
                eq(state(0, 7).focus_events, focus_events, 'stale owner up remains consumed')
            )") + R"(
                p.workspace_swipe_cancel()
                p.swipe_end(false)
                cleaned()
                touch('up', 0, 7)
                eq(state(0, 7).focus_events, focus_events + 1, 'only the real client up releases client contact')
                -- Release the client's hold on the numbered fixture before the next subcase.
                hl.dispatch(hl.dsp.window.move({ window = 'class:swipe-touch-client', workspace = '1' }))
            )"));
        }
    }
}
