#pragma once

#include <chrono>
#include <functional>
#include <optional>

class CEventLoopTimer {
  public:
    CEventLoopTimer(std::optional<std::chrono::system_clock::duration> timeout, std::function<void(std::shared_ptr<CEventLoopTimer> self, void* data)> cb_, void* data_);

    // if not specified, disarms.
    // if specified, arms.
    void  updateTimeout(std::optional<std::chrono::system_clock::duration> timeout);

    void  cancel();
    bool  passed();

    float leftUs();

    bool  cancelled();
    // resets expires
    void call(std::shared_ptr<CEventLoopTimer> self);

  private:
    std::function<void(std::shared_ptr<CEventLoopTimer> self, void* data)> cb;
    void*                                                                  data = nullptr;
    std::optional<std::chrono::system_clock::time_point>                   expires;
    bool                                                                   wasCancelled = false;
};
