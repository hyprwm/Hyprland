#pragma once

#include <chrono>
#include <functional>
#include <optional>

#include "../../helpers/memory/Memory.hpp"

class CEventLoopTimer {
  public:
    CEventLoopTimer(std::optional<std::chrono::steady_clock::duration> timeout, std::function<void(SP<CEventLoopTimer> self, void* data)> cb_, void* data_);

    // if not specified, disarms.
    // if specified, arms.
    void  updateTimeout(std::optional<std::chrono::steady_clock::duration> timeout);

    void  cancel();
    bool  passed();
    bool  armed();

    float leftUs();

    bool  cancelled();
    // resets expires
    void call(SP<CEventLoopTimer> self);

  private:
    std::function<void(SP<CEventLoopTimer> self, void* data)> cb;
    void*                                                     data = nullptr;
    std::optional<std::chrono::steady_clock::time_point>      expires;
    bool                                                      wasCancelled = false;
};
