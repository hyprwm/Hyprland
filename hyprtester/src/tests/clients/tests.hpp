#pragma once

#include <vector>
#include <functional>

inline std::vector<std::function<bool()>> clientTestFns;

#define REGISTER_CLIENT_TEST_FN(fn)                                                                                                                                                \
    static auto _register_fn = [] {                                                                                                                                                \
        clientTestFns.emplace_back(fn);                                                                                                                                            \
        return 1;                                                                                                                                                                  \
    }();
