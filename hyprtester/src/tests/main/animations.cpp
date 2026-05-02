#include "../../Log.hpp"
#include "tests.hpp"
#include "../../shared.hpp"
#include "../../hyprctlCompat.hpp"
#include <hyprutils/os/Process.hpp>
#include <hyprutils/memory/WeakPtr.hpp>

using namespace Hyprutils::OS;
using namespace Hyprutils::Memory;

TEST_CASE(animationsTrivial) {
    auto str = getFromSocket("/animations");
    NLog::log("{}Testing bezier curve output from `hyprctl animations`", Colors::YELLOW);
    ASSERT_CONTAINS(str, std::format("beziers:\n\n\tname: quick\n\t\tX0: 0.15\n\t\tY0: 0.00\n\t\tX1: 0.10\n\t\tY1: 1.00"));
}
