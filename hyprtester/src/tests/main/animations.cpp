#include "../../hyprctlCompat.hpp"
#include "../../Log.hpp"
#include "../../shared.hpp"
#include "tests.hpp"

using namespace Hyprutils::OS;
using namespace Hyprutils::Memory;

TEST_CASE(animationsTrivial) {
    auto str = getFromSocket("/animations");
    NLog::yellow("Testing bezier curve output from `hyprctl animations`");
    ASSERT_CONTAINS(str, std::format("beziers:\n\n\tname: quick\n\t\tX0: 0.15\n\t\tY0: 0.00\n\t\tX1: 0.10\n\t\tY1: 1.00"));
    ASSERT_CONTAINS(str, "name: overviewIn");
    ASSERT_CONTAINS(str, "name: overviewOut");
    ASSERT_CONTAINS(str, "name: overviewMove");
    ASSERT_CONTAINS(str, "name: overviewFade");
}
