#pragma once

#include <vector>
#include <functional>

inline std::vector<std::function<bool()>> testFns;

#define REGISTER_TEST_FN(fn)                                                                                                                                                       \
    static auto _register_fn = [] {                                                                                                                                                \
        testFns.emplace_back(fn);                                                                                                                                                  \
        return 1;                                                                                                                                                                  \
    }();
