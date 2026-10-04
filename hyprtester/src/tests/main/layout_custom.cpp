#include "../shared.hpp"
#include "../../shared.hpp"
#include "../../hyprctlCompat.hpp"
#include "tests.hpp"

TEST_CASE(layoutCustomGrid) {
    OK(getFromSocket("r/eval hl.config({ general = { layout = 'lua:grid' } })"));

    SPAWN_KITTY("kitty_A");
    SPAWN_KITTY("kitty_B");

    {
        auto clients = getFromSocket("/clients");
        EXPECT_COUNT_STRING(clients, "size: 931,1036", 2);
    }

    SPAWN_KITTY("kitty_C");

    {
        auto clients = getFromSocket("/clients");
        EXPECT_COUNT_STRING(clients, "size: 931,511", 3);
    }

    SPAWN_KITTY("kitty_D");

    {
        auto clients = getFromSocket("/clients");
        EXPECT_COUNT_STRING(clients, "size: 931,511", 4);
    }
}

TEST_CASE(layoutCustomColumns) {
    OK(getFromSocket("r/eval hl.config({ general = { layout = 'lua:columns' } })"));

    SPAWN_KITTY("kitty_A");
    SPAWN_KITTY("kitty_B");

    {
        auto clients = getFromSocket("/clients");
        EXPECT_COUNT_STRING(clients, "size: 931,1036", 2);
    }

    SPAWN_KITTY("kitty_C");

    {
        auto clients = getFromSocket("/clients");
        EXPECT_COUNT_STRING(clients, ",1036\n", 3); // this won't split evenly
    }

    SPAWN_KITTY("kitty_D");

    {
        auto clients = getFromSocket("/clients");
        EXPECT_COUNT_STRING(clients, ",1036\n", 4); // this won't split evenly
    }
}

TEST_CASE(layoutCustomResizeTarget) {
    OK(getFromSocket("r/eval hl.config({ general = { layout = 'lua:split' } })"));

    ASSERT(!!Tests::spawnKitty("kitty_A"), true);
    ASSERT(!!Tests::spawnKitty("kitty_B"), true);

    {
        auto clients = getFromSocket("/clients");
        EXPECT_COUNT_STRING(clients, "size: 931,1036", 2);
    }

    OK(getFromSocket("/dispatch hl.dsp.window.resize({ x = 1031, y = 1036, window = 'class:kitty_A' })"));

    {
        auto clients = getFromSocket("/clients");
        EXPECT_COUNT_STRING(clients, "size: 1031,1036", 1);
        EXPECT_COUNT_STRING(clients, "size: 831,1036", 1);
    }
}

TEST_CASE(layoutCustomResizeTargetAbsent) {
    OK(getFromSocket("r/eval hl.config({ general = { layout = 'lua:columns' } })"));

    ASSERT(!!Tests::spawnKitty("kitty_A"), true);
    ASSERT(!!Tests::spawnKitty("kitty_B"), true);

    OK(getFromSocket("/dispatch hl.dsp.window.resize({ x = 1031, y = 1036, window = 'class:kitty_A' })"));

    {
        auto clients = getFromSocket("/clients");
        EXPECT_COUNT_STRING(clients, "size: 931,1036", 2);
    }
}
