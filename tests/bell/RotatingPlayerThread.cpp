#include <bell/impl/RotatingPlayerThread.hpp>

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <future>
#include <thread>

using namespace std::chrono_literals;

TEST(RotatingPlayerThread, ExecutesQueuedDataOnWorkerThread) {
    std::promise<std::pair<std::thread::id, std::string>> result;
    auto                                                  resultFuture = result.get_future();
    const auto                                            callerThread = std::this_thread::get_id();

    Bell::CRotatingPlayerThread                           player([&](const std::string& data) { result.set_value({std::this_thread::get_id(), data}); });
    player.queue("sound");

    ASSERT_EQ(resultFuture.wait_for(1s), std::future_status::ready);
    const auto [thread, data] = resultFuture.get();
    EXPECT_NE(thread, callerThread);
    EXPECT_EQ(data, "sound");
}

TEST(RotatingPlayerThread, IgnoresQueuedDataWhileBusyWithoutBlocking) {
    std::promise<void>          callbackStarted;
    std::promise<void>          releaseCallback;
    std::promise<void>          callbackFinished;
    std::promise<std::string>   nextData;
    auto                        callbackStartedFuture  = callbackStarted.get_future();
    auto                        releaseCallbackFuture  = releaseCallback.get_future().share();
    auto                        callbackFinishedFuture = callbackFinished.get_future();
    auto                        nextDataFuture         = nextData.get_future();
    std::atomic<int>            invocationCount        = 0;

    Bell::CRotatingPlayerThread player([&](const std::string& data) {
        if (++invocationCount == 1) {
            callbackStarted.set_value();
            releaseCallbackFuture.wait();
            callbackFinished.set_value();
            return;
        }

        nextData.set_value(data);
    });

    player.queue("playing");
    ASSERT_EQ(callbackStartedFuture.wait_for(1s), std::future_status::ready);

    auto       busyQueue       = std::async(std::launch::async, [&] { player.queue("ignored"); });
    const auto busyQueueStatus = busyQueue.wait_for(200ms);

    releaseCallback.set_value();
    busyQueue.wait();

    EXPECT_EQ(busyQueueStatus, std::future_status::ready);
    ASSERT_EQ(callbackFinishedFuture.wait_for(1s), std::future_status::ready);
    EXPECT_EQ(nextDataFuture.wait_for(200ms), std::future_status::timeout);

    player.queue("accepted");
    ASSERT_EQ(nextDataFuture.wait_for(1s), std::future_status::ready);
    EXPECT_EQ(nextDataFuture.get(), "accepted");
    EXPECT_EQ(invocationCount, 2);
}

TEST(RotatingPlayerThread, DestructionWaitsForActiveCallback) {
    std::promise<void> callbackStarted;
    std::promise<void> releaseCallback;
    auto               callbackStartedFuture = callbackStarted.get_future();
    auto               releaseCallbackFuture = releaseCallback.get_future().share();

    auto               player = std::make_unique<Bell::CRotatingPlayerThread>([&](const std::string&) {
        callbackStarted.set_value();
        releaseCallbackFuture.wait();
    });

    player->queue("sound");
    ASSERT_EQ(callbackStartedFuture.wait_for(1s), std::future_status::ready);

    auto       destroy       = std::async(std::launch::async, [player = std::move(player)]() mutable { player.reset(); });
    const auto destroyStatus = destroy.wait_for(200ms);

    releaseCallback.set_value();

    EXPECT_EQ(destroyStatus, std::future_status::timeout);
    EXPECT_EQ(destroy.wait_for(1s), std::future_status::ready);
}

TEST(RotatingPlayerThread, RepeatedIdleDestructionIsSafe) {
    for (size_t i = 0; i < 256; ++i) {
        Bell::CRotatingPlayerThread player([](const std::string&) {});
    }
}
