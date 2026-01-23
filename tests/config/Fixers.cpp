
#include <config/fixer/runner/ConfigFixSimpleRewriter.hpp>
#include <config/fixer/runner/runners/Fixer12033.hpp>

#include <gtest/gtest.h>

TEST(Config, fixer12033) {
    SP<Config::Supplementary::CFixer12033> fixer = makeShared<Config::Supplementary::CFixer12033>();

    const std::string                      CONFIG = R"#(# sample config

misc:new_window_takes_over_fullscreen = 0
misc:new_window_takes_over_fullscreen=1
misc:new_window_takes_over_fullscreen=\
\
\
1

misc {
    new_window_takes_over_fullscreen = \
                                        2
}

master {
    inherit_fullscreen = yes
})#";

    EXPECT_EQ(fixer->check(CONFIG), false);

    const std::string CONFIG_FIXED = R"#(# sample config

#misc:new_window_takes_over_fullscreen = 0
#misc:new_window_takes_over_fullscreen=1
#misc:new_window_takes_over_fullscreen=\
#\
#\
#1

misc {
#    new_window_takes_over_fullscreen = \
#                                        2
}

master {
#    inherit_fullscreen = yes
}
misc:new_window_takes_over_fullscreen = 2)#";

    std::cout << "Fixed:\n" << fixer->run(CONFIG) << "\n";

    EXPECT_EQ(fixer->run(CONFIG), CONFIG_FIXED);
}