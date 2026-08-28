#pragma once

#define NON_MOVABLE(cname)                                                                                                                                                         \
    cname(cname&&)                 = delete;                                                                                                                                       \
    cname(cname&)                  = delete;                                                                                                                                       \
    cname(const cname&)            = delete;                                                                                                                                       \
    cname& operator=(const cname&) = delete;                                                                                                                                       \
    cname& operator=(cname&&)      = delete;
