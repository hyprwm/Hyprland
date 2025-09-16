#include "tests.hpp"
#include "../../shared.hpp"
#include "../../hyprctlCompat.hpp"
#include <hyprutils/os/Process.hpp>
#include <hyprutils/memory/WeakPtr.hpp>
#include "../shared.hpp"

static int ret = 0;

using namespace Hyprutils::OS;
using namespace Hyprutils::Memory;

static void testConfigRegistration() {
    NLog::log("{}Testing config: dwindle:smart_split_on_drop", Colors::GREEN);
    
    NLog::log("{}Setting dwindle:smart_split_on_drop to true", Colors::YELLOW);
    OK(getFromSocket("/keyword dwindle:smart_split_on_drop true"));
    
    auto configStr = getFromSocket("/getoption dwindle:smart_split_on_drop");
    EXPECT_CONTAINS(configStr, "1");
    
    NLog::log("{}Setting dwindle:smart_split_on_drop to false", Colors::YELLOW);
    OK(getFromSocket("/keyword dwindle:smart_split_on_drop false"));
    
    configStr = getFromSocket("/getoption dwindle:smart_split_on_drop");
    EXPECT_CONTAINS(configStr, "0");
}

static void testSmartSplitBehavior() {
    NLog::log("{}Testing smart split behavior with smart_split_on_drop enabled", Colors::YELLOW);
    
    NLog::log("{}Switching to workspace \"smart_split\"", Colors::YELLOW);
    getFromSocket("/dispatch workspace name:smart_split");
    
    OK(getFromSocket("/keyword dwindle:smart_split 1"));
    OK(getFromSocket("/keyword dwindle:smart_split_on_drop true"));
    OK(getFromSocket("/keyword dwindle:split_width_multiplier 1.5"));
    
    if (!Tests::spawnKitty("kitty_A"))
        return;
    
    NLog::log("{}Expecting 1 window", Colors::YELLOW);
    EXPECT(Tests::windowCount(), 1);
    
    if (!Tests::spawnKitty("kitty_B"))
        return;
    
    NLog::log("{}Expecting 2 windows", Colors::YELLOW);
    EXPECT(Tests::windowCount(), 2);
    
    NLog::log("{}Windows created successfully with smart_split_on_drop enabled", Colors::YELLOW);
    
    Tests::killAllWindows();
    EXPECT(Tests::windowCount(), 0);
}

static void testSmartSplitOnDropBehavior() {
    NLog::log("{}Testing smart split with drop simulation", Colors::YELLOW);
    
    NLog::log("{}Switching to workspace \"smart_split_drop\"", Colors::YELLOW);
    getFromSocket("/dispatch workspace name:smart_split_drop");
    
    OK(getFromSocket("/keyword dwindle:smart_split 1"));
    OK(getFromSocket("/keyword dwindle:smart_split_on_drop true"));
    OK(getFromSocket("/keyword dwindle:split_width_multiplier 1.5"));
    
    if (!Tests::spawnKitty("kitty_A"))
        return;
    
    NLog::log("{}Expecting 1 window", Colors::YELLOW);
    EXPECT(Tests::windowCount(), 1);
    
    if (!Tests::spawnKitty("kitty_B"))
        return;
    
    OK(getFromSocket("/dispatch moveactive exact 100 100"));
    
    NLog::log("{}Expecting 2 windows after drop simulation", Colors::YELLOW);
    EXPECT(Tests::windowCount(), 2);
    
    NLog::log("{}Windows created and moved successfully with smart split on drop", Colors::YELLOW);
    
    Tests::killAllWindows();
    EXPECT(Tests::windowCount(), 0);
}

static void testDefaultBehavior() {
    NLog::log("{}Testing default behavior with smart_split_on_drop disabled", Colors::YELLOW);
    
    NLog::log("{}Switching to workspace \"default_behavior\"", Colors::YELLOW);
    getFromSocket("/dispatch workspace name:default_behavior");
    
    OK(getFromSocket("/keyword dwindle:smart_split 1"));
    OK(getFromSocket("/keyword dwindle:smart_split_on_drop false"));
    OK(getFromSocket("/keyword dwindle:split_width_multiplier 1.5"));
    
    if (!Tests::spawnKitty("kitty_A"))
        return;
    
    NLog::log("{}Expecting 1 window", Colors::YELLOW);
    EXPECT(Tests::windowCount(), 1);
    
    if (!Tests::spawnKitty("kitty_B"))
        return;
    
    NLog::log("{}Expecting 2 windows", Colors::YELLOW);
    EXPECT(Tests::windowCount(), 2);
    
    NLog::log("{}Windows created successfully with smart_split enabled for all operations", Colors::YELLOW);
    
    Tests::killAllWindows();
    EXPECT(Tests::windowCount(), 0);
}

static bool test() {
    NLog::log("{}Testing config: dwindle:smart_split_on_drop", Colors::GREEN);
    
    testConfigRegistration();
    testSmartSplitBehavior();
    testSmartSplitOnDropBehavior();
    testDefaultBehavior();
    
    return !ret;
}

REGISTER_TEST_FN(test)
