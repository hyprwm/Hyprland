#pragma once

#define REGISTER(type)                                                                                                                                                             \
    static auto _REGISTER_TYPE = [] -> int {                                                                                                                                       \
        Config::Supplementary::fixRunners.emplace_back(makeUnique<type>());                                                                                                        \
        return 1;                                                                                                                                                                  \
    }();