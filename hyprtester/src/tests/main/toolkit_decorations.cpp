#include "../../shared.hpp"
#include "../../hyprctlCompat.hpp"
#include "../shared.hpp"
#include "tests.hpp"

TEST_CASE(toolkitDecorationsFollowTheRule) {
    OK(getFromSocket("/eval hl.decoration.register('test_bar', { edge = 'top', size = 30, create = function(deco) end })"));
    OK(getFromSocket("/eval hl.window_rule({ match = { class = 'kitty_decorated' }, decorations = 'test_bar' })"));

    SPAWN_KITTY("kitty_decorated");
    SPAWN_KITTY("kitty_plain");

    EXPECT_CONTAINS(getFromSocket("/decorations class:kitty_decorated"), "Toolkit: test_bar");
    EXPECT_NOT_CONTAINS(getFromSocket("/decorations class:kitty_plain"), "Toolkit: test_bar");
}
