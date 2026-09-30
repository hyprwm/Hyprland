#include "tests.hpp"
#include "../../hyprctlCompat.hpp"

#include <hyprutils/utils/ScopeGuard.hpp>
#include <array>
#include <format>
#include <string_view>
#include <thread>

namespace SpecialWorkspaceSwipeTests {
    static std::string run(std::string_view body, bool animations) {
        Hyprutils::Utils::CScopeGuard cleanup([] { getFromSocket("/eval hl.plugin.test.special_swipe_reset()"); });
        const auto                    SCRIPT = std::format("local animations = {}\n", animations ? "true" : "false") + R"(
            local p = hl.plugin.test
            hl.config({ animations = { enabled = animations }, decoration = { dim_special = 0.4 } })
            for _, leaf in ipairs({ 'specialWorkspace', 'specialWorkspaceIn', 'specialWorkspaceOut' }) do
                hl.animation({ leaf = leaf, enabled = true, speed = 1, bezier = 'default', style = 'slidefade' })
            end
            local snapshot = p.special_swipe_snapshot
            local function eq(actual, expected, label)
                assert(actual == expected, label .. ': expected ' .. tostring(expected) .. ', got ' .. tostring(actual))
            end
            local function near(actual, expected, label)
                assert(math.abs(actual - expected) < 0.0001, label .. ': expected ' .. expected .. ', got ' .. actual)
            end
            local function preview(opened, forced, distance)
                local s = snapshot()
                eq(s.exists, true, 'fixture workspace exists')
                eq(s.gesture_alive, true, 'gesture object exists')
                eq(s.session, true, 'successful session survives own activation signals')
                eq(s.active, true, 'preview owns active special')
                eq(s.visible, true, 'preview is visible')
                eq(s.forced, true, 'preview rendering hold')
                eq(s.acquired, not forced, 'hold ownership')
                local progress = math.min(math.max(distance / 150, 0), 1)
                near(s.alpha, opened and 1 - progress or progress, 'preview alpha')
                near(s.alpha_goal, s.alpha, 'preview alpha warped')
            end
            local function finished(opened, forced)
                local s = snapshot()
                eq(s.session, false, 'session ended')
                eq(s.released, true, 'session references and acquired hold released')
                eq(s.gesture_alive, true, 'gesture remains registered after end')
                eq(s.forced, forced, 'preexisting hold preserved')
                eq(s.active, opened, 'final active special')
                eq(s.visible, opened, 'final visibility')
                near(s.alpha_goal, opened and 1 or 0, 'final alpha goal')
                near(s.fade_goal, opened and 1 or 0, 'final monitor fade goal')
                near(s.dim_goal, opened and 0.4 or 0, 'final monitor dim goal')
                if opened then
                    near(s.offset_goal_x, 0, 'open offset x goal')
                    near(s.offset_goal_y, 0, 'open offset y goal')
                end
                if not animations then
                    p.special_swipe_settle()
                    s = snapshot()
                    near(s.alpha, s.alpha_goal, 'disabled animation alpha settled')
                    near(s.fade, s.fade_goal, 'disabled animation fade settled')
                    near(s.dim, s.dim_goal, 'disabled animation dim settled')
                    near(s.blur, s.blur_goal, 'disabled animation blur settled')
                    near(s.offset_x, s.offset_goal_x, 'disabled animation offset x settled')
                    near(s.offset_y, s.offset_goal_y, 'disabled animation offset y settled')
                end
            end
        )" + std::string{body};
        return getFromSocket("/eval " + SCRIPT);
    }

    static std::string runOnOutputs(std::string_view body, bool animations) {
        const std::array<std::string, 2> NAMES = {
            "HYPRTEST-SPECIAL-A",
            "HYPRTEST-SPECIAL-B",
        };
        std::array<bool, 2>           created = {};
        Hyprutils::Utils::CScopeGuard cleanup([&] {
            getFromSocket("/eval hl.plugin.test.special_swipe_reset()");
            for (size_t i = 0; i < NAMES.size(); ++i) {
                if (created[i])
                    getFromSocket("/output remove " + NAMES[i]);
            }
        });
        for (size_t i = 0; i < NAMES.size(); ++i) {
            auto result = getFromSocket(std::format("/eval hl.monitor({{ output = '{}', mode = '1920x1080@60', position = '{}x0', scale = '1' }})", NAMES[i], 24000 + i * 1920));
            if (result != "ok")
                return result;
            result = getFromSocket("/output create headless " + NAMES[i]);
            if (result != "ok")
                return result;
            created[i] = true;
            bool ready = false;
            for (int attempt = 0; attempt < 50 && !ready; ++attempt) {
                ready = getFromSocket("/monitors").contains("Monitor " + NAMES[i] + " (ID ");
                if (!ready)
                    std::this_thread::sleep_for(std::chrono::milliseconds(20));
            }
            if (!ready)
                return "Timed out creating special swipe output " + NAMES[i];
        }
        const auto FOCUSED = getFromSocket("/dispatch hl.dsp.focus({ monitor = 'HYPRTEST-SPECIAL-A' })");
        if (FOCUSED != "ok")
            return FOCUSED;
        return run(body, animations);
    }

    TEST_CASE(specialWorkspaceSwipeThresholds) {
        for (const bool ANIMATIONS : {false, true}) {
            OK(run(R"(
                for _, opened in ipairs({ false, true }) do
                    for _, cancelled in ipairs({ false, true }) do
                        for _, forced in ipairs({ false, true }) do
                            for _, distance in ipairs({ 44.99, 45, 150 }) do
                                p.special_swipe_fixture(opened, forced)
                                p.special_swipe_begin()
                                -- Deltas accumulate; the first update must not be treated as a full gesture.
                                p.special_swipe_update(distance / 2)
                                p.special_swipe_update(distance / 2)
                                preview(opened, forced, distance)
                                p.special_swipe_end(cancelled)
                                local committed = distance >= 45
                                local expected_open = opened
                                if committed then expected_open = not opened end
                                finished(expected_open, forced)
                                p.special_swipe_update(150)
                                p.special_swipe_end(not cancelled)
                                finished(expected_open, forced)
                            end
                        end
                    end
                end
            )",
                   ANIMATIONS));
        }
    }

    TEST_CASE(specialWorkspaceSwipeRestartAndDestructor) {
        for (const bool ANIMATIONS : {false, true}) {
            OK(run(R"(
                for _, opened in ipairs({ false, true }) do
                    for _, forced in ipairs({ false, true }) do
                        for _, restart in ipairs({ false, true }) do
                            p.special_swipe_fixture(opened, forced)
                            p.special_swipe_begin()
                            p.special_swipe_update(100) -- Internal abort must roll back even above the threshold.
                            preview(opened, forced, 100)
                            if restart then
                                p.special_swipe_begin()
                                local s = snapshot()
                                eq(s.session, true, 'restart acquired a fresh session')
                                eq(s.forced, true, 'restart holds rendering')
                                eq(s.acquired, not forced, 'restart did not mistake old hold for external ownership')
                                p.special_swipe_update(100)
                            end
                            p.special_swipe_destroy()
                            local s = snapshot()
                            eq(s.gesture_alive, false, 'gesture destructor ran')
                            eq(s.forced, forced, 'destructor released only its own hold')
                            eq(s.active, opened, 'destructor rolled back original visibility')
                            eq(s.visible, opened, 'destructor restored workspace visibility')
                            near(s.alpha_goal, opened and 1 or 0, 'destructor alpha goal')
                            near(s.fade_goal, opened and 1 or 0, 'destructor fade goal')
                        end
                    end
                end
            )",
                   ANIMATIONS));
        }
    }

    TEST_CASE(specialWorkspaceSwipeReplacementInvalidation) {
        for (const bool ANIMATIONS : {false, true}) {
            OK(run(R"(
                for _, opened in ipairs({ false, true }) do
                    for _, forced in ipairs({ false, true }) do
                        p.special_swipe_fixture(opened, forced)
                        p.special_swipe_begin()
                        p.special_swipe_update(75)
                        preview(opened, forced, 75)
                        -- Probe checks synchronous cleanup before sending late events and destroying the object.
                        p.special_swipe_invalidate('replace')
                        eq(snapshot().forced, forced, 'replacement preserves original external hold')
                    end
                end
            )",
                   ANIMATIONS));
        }
    }

    TEST_CASE(specialWorkspaceSwipeMoveInvalidation) {
        for (const bool ANIMATIONS : {false, true}) {
            OK(runOnOutputs(R"(
                for _, opened in ipairs({ false, true }) do
                    for _, forced in ipairs({ false, true }) do
                        hl.dispatch(hl.dsp.focus({ monitor = 'HYPRTEST-SPECIAL-A' }))
                        p.special_swipe_fixture(opened, forced)
                        p.special_swipe_begin()
                        p.special_swipe_update(75)
                        preview(opened, forced, 75)
                        p.special_swipe_invalidate('move', 'HYPRTEST-SPECIAL-B')
                        local s = snapshot()
                        eq(s.active, true, 'moved special remains active')
                        near(s.alpha_goal, 1, 'moved preview completes alpha IN')
                        near(s.offset_goal_x, 0, 'moved preview completes offset x IN')
                        near(s.offset_goal_y, 0, 'moved preview completes offset y IN')
                    end
                end
            )",
                            ANIMATIONS));
        }
    }

    TEST_CASE(specialWorkspaceSwipeMoveExternalState) {
        for (const bool ANIMATIONS : {false, true}) {
            OK(runOnOutputs(R"(
                for _, opened in ipairs({ false, true }) do
                    for _, forced in ipairs({ false, true }) do
                        for _, mode in ipairs({ 'alpha', 'offset', 'both', 'animated', 'animated-same', 'unupdated' }) do
                            hl.dispatch(hl.dsp.focus({ monitor = 'HYPRTEST-SPECIAL-A' }))
                            p.special_swipe_fixture(opened, forced)
                            p.special_swipe_begin()
                            if mode ~= 'unupdated' then
                                p.special_swipe_update(75)
                                preview(opened, forced, 75)
                            end
                            -- The destination replacement's callback overrides state before the gesture's move listener.
                            -- Unchanged properties must complete IN independently; external values/config/animations survive.
                            p.special_swipe_invalidate('move', 'HYPRTEST-SPECIAL-B', mode)
                            eq(snapshot().active, true, 'externally controlled special stays active after move')
                            eq(snapshot().forced, forced, 'move releases only the gesture hold')
                        end
                    end
                end
            )",
                            ANIMATIONS));
        }
    }

    TEST_CASE(specialWorkspaceSwipeOutputRemoval) {
        for (const bool ANIMATIONS : {false, true}) {
            for (const bool OPENED : {false, true}) {
                for (const bool FORCED : {false, true}) {
                    OK(runOnOutputs(std::format(R"(
                        p.special_swipe_fixture({}, {})
                        p.special_swipe_begin()
                        p.special_swipe_update(75)
                        preview({}, {}, 75)
                        p.special_swipe_invalidate('remove')
                    )",
                                                OPENED ? "true" : "false", FORCED ? "true" : "false", OPENED ? "true" : "false", FORCED ? "true" : "false"),
                                    ANIMATIONS));
                }
            }
        }
    }

    TEST_CASE(specialWorkspaceSwipeReentrantEnd) {
        for (const bool ANIMATIONS : {false, true}) {
            OK(run(R"(
                p.special_swipe_fixture(false, false)
                p.special_swipe_begin()
                p.special_swipe_update(20)
                preview(false, false, 20)
                p.special_swipe_reentrant_end()
                preview(false, false, 75)
                p.special_swipe_end(false)
                finished(true, false)
            )",
                   ANIMATIONS));
        }
    }

    TEST_CASE(specialWorkspaceSwipeEmptyWorkspaceLifetime) {
        for (const bool ANIMATIONS : {false, true}) {
            OK(run(R"(
                for _, opened in ipairs({ false, true }) do
                    local distance = opened and 150 or 20
                    p.special_swipe_fixture(opened, false)
                    p.special_swipe_begin()
                    p.special_swipe_update(distance)
                    preview(opened, false, distance)
                    p.special_swipe_drop_workspace() -- Remove the fixture's independent strong reference first.
                    eq(snapshot().exists, true, 'live session retains its empty workspace')
                    p.special_swipe_end(false)
                    local s = snapshot()
                    eq(s.gesture_alive, true, 'gesture object stays registered')
                    eq(s.released, true, 'ended gesture dropped all session references')
                    eq(s.exists, false, 'closed empty workspace released while gesture stays alive')
                    p.special_swipe_update(150)
                    p.special_swipe_end(true)
                    eq(snapshot().exists, false, 'late events do not recreate released workspace')
                end
            )",
                   ANIMATIONS));
        }
    }
}
