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
