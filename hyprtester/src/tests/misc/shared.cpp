#include "tests.hpp"

#include "../shared.hpp"
#include "../../shared.hpp"

// Test if Tests::spawnKitty works
TEST_CASE(spawnKitty) {
    ASSERT(!!Tests::spawnKitty("kitty"), true);
}
