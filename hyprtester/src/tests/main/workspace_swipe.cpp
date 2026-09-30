#include "tests.hpp"
#include "../../hyprctlCompat.hpp"

#include <hyprutils/utils/ScopeGuard.hpp>
#include <format>
#include <string_view>

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
}
