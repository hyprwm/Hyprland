#include "tests.hpp"
#include "../../shared.hpp"
#include "../../hyprctlCompat.hpp"
#include <thread>
#include <chrono>
#include <hyprutils/os/Process.hpp>
#include <hyprutils/memory/WeakPtr.hpp>
#include "../shared.hpp"

static int ret = 0;

using namespace Hyprutils::OS;
using namespace Hyprutils::Memory;

#define UP CUniquePointer
#define SP CSharedPointer

static bool test() {
    NLog::log("{}Testing dwindle", Colors::GREEN);

    OK(getFromSocket("/dispatch workspace 101"));

    // test the layout preset

    Tests::spawnKitty();

    {
        auto str = getFromSocket("/clients");
        EXPECT_COUNT_STRING(str, "at: 22,22", 1);
        EXPECT_COUNT_STRING(str, "size: 1876,1036", 1);
    }

    Tests::spawnKitty();

    {
        auto str = getFromSocket("/clients");
        EXPECT_COUNT_STRING(str, "at: 22,22", 1);
        EXPECT_COUNT_STRING(str, "size: 605,1036", 1);

        EXPECT_COUNT_STRING(str, "at: 641,22", 1);
        EXPECT_COUNT_STRING(str, "size: 1257,1036", 1);
    }

    Tests::spawnKitty();

    {
        auto str = getFromSocket("/clients");
        EXPECT_COUNT_STRING(str, "at: 22,22", 1);
        EXPECT_COUNT_STRING(str, "at: 641,22", 1);
        EXPECT_COUNT_STRING(str, "at: 1284,22", 1);

        EXPECT_COUNT_STRING(str, "size: 605,1036", 1);
        EXPECT_COUNT_STRING(str, "size: 629,1036", 1);
        EXPECT_COUNT_STRING(str, "size: 614,1036", 1);
    }

    Tests::spawnKitty();

    {
        auto str = getFromSocket("/clients");
        EXPECT_COUNT_STRING(str, "at: 22,22", 1);
        EXPECT_COUNT_STRING(str, "at: 641,22", 1);
        EXPECT_COUNT_STRING(str, "at: 1284,22", 1);
        EXPECT_COUNT_STRING(str, "at: 1284,547", 1);

        EXPECT_COUNT_STRING(str, "size: 605,1036", 1);
        EXPECT_COUNT_STRING(str, "size: 629,1036", 1);
        EXPECT_COUNT_STRING(str, "size: 614,511", 2);
    }

    Tests::spawnKitty();

    {
        auto str = getFromSocket("/clients");
        EXPECT_COUNT_STRING(str, "at: 22,22", 1);
        EXPECT_COUNT_STRING(str, "at: 641,22", 1);
        EXPECT_COUNT_STRING(str, "at: 1284,22", 1);
        EXPECT_COUNT_STRING(str, "at: 1284,547", 1);
        EXPECT_COUNT_STRING(str, "at: 22,547", 1);

        EXPECT_COUNT_STRING(str, "size: 629,1036", 1);
        EXPECT_COUNT_STRING(str, "size: 614,511", 2);
        EXPECT_COUNT_STRING(str, "size: 605,511", 2);
    }

    Tests::killAllWindows();

    OK(getFromSocket("/dispatch workspace 1"));

    NLog::log("{}Reloading the config", Colors::YELLOW);
    OK(getFromSocket("/reload"));

    return !ret;
}

REGISTER_TEST_FN(test)
