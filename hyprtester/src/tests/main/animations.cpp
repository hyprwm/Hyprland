#include "hyprtester/src/Log.hpp"
#include "tests.hpp"
#include "../../shared.hpp"
#include "../../hyprctlCompat.hpp"
#include <hyprutils/os/Process.hpp>
#include <hyprutils/memory/WeakPtr.hpp>

static int ret = 0;

using namespace Hyprutils::OS;
using namespace Hyprutils::Memory;

static bool test() {
    NLog::log("{}Testing animations", Colors::GREEN);

    auto str = getFromSocket("/animations");
    NLog::log("{}Testing bezier curve output from `hyprctl animations`", Colors::YELLOW);
    {EXPECT_CONTAINS(str, std::format("beziers:\n\n\tname: quick\n\t\tX0: 0.15\n\t\tY0: 0.00\n\t\tX1: 0.10\n\t\tY1: 1.00"))};
    return !ret;
}

REGISTER_TEST_FN(test)
