#include "tests.hpp"

#include "../shared.hpp"
#include "../../shared.hpp"

// Test if Tests::spawnKitty works
TEST_CASE(spawnKitty) {
    if (!Tests::spawnKitty("A"))
        FAIL_TEST("Could not spawn kitty!");
}
