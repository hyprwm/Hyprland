#include "../shared.hpp"
#include "../../shared.hpp"
#include "../../hyprctlCompat.hpp"
#include "tests.hpp"

TEST_CASE(layoutCustomGrid) {
    OK(getFromSocket("r/eval hl.config({ general = { layout = 'lua:grid' } })"));

    ASSERT(!!Tests::spawnKitty("kitty_A"), true);
    ASSERT(!!Tests::spawnKitty("kitty_B"), true);

    {
        auto clients = getFromSocket("/clients");
        EXPECT_COUNT_STRING(clients, "size: 931,1036", 2);
    }

    ASSERT(!!Tests::spawnKitty("kitty_C"), true);

    {
        auto clients = getFromSocket("/clients");
        EXPECT_COUNT_STRING(clients, "size: 931,511", 3);
    }

    ASSERT(!!Tests::spawnKitty("kitty_D"), true);

    {
        auto clients = getFromSocket("/clients");
        EXPECT_COUNT_STRING(clients, "size: 931,511", 4);
    }
}

TEST_CASE(layoutCustomColumns) {
    OK(getFromSocket("r/eval hl.config({ general = { layout = 'lua:columns' } })"));

    ASSERT(!!Tests::spawnKitty("kitty_A"), true);
    ASSERT(!!Tests::spawnKitty("kitty_B"), true);

    {
        auto clients = getFromSocket("/clients");
        EXPECT_COUNT_STRING(clients, "size: 931,1036", 2);
    }

    ASSERT(!!Tests::spawnKitty("kitty_C"), true);

    {
        auto clients = getFromSocket("/clients");
        EXPECT_COUNT_STRING(clients, ",1036\n", 3); // this won't split evenly
    }

    ASSERT(!!Tests::spawnKitty("kitty_D"), true);

    {
        auto clients = getFromSocket("/clients");
        EXPECT_COUNT_STRING(clients, ",1036\n", 4); // this won't split evenly
    }
}
