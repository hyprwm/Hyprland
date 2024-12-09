#pragma once

#include <shared_mutex>

// NOLINTNEXTLINE(readability-identifier-naming)
namespace Debug {
    struct SRollingLogFollow {
        std::unordered_map<int, std::string> socketToRollingLogFollowQueue;
        std::shared_mutex                    m;
        bool                                 running                    = false;
        static constexpr size_t              ROLLING_LOG_FOLLOW_TOO_BIG = 8192;

        // Returns true if the queue is empty for the given socket
        bool isEmpty(int socket) {
            std::shared_lock<std::shared_mutex> r(m);
            return socketToRollingLogFollowQueue[socket].empty();
        }

        std::string debugInfo() {
            std::shared_lock<std::shared_mutex> r(m);
            return std::format("RollingLogFollow, got {} connections", socketToRollingLogFollowQueue.size());
        }

        std::string getLog(int socket) {
            std::unique_lock<std::shared_mutex> w(m);

            const std::string                   ret = socketToRollingLogFollowQueue[socket];
            socketToRollingLogFollowQueue[socket]   = "";

            return ret;
        };

        void addLog(const std::string& log) {
            std::unique_lock<std::shared_mutex> w(m);
            running = true;
            std::vector<int> to_erase;
            for (const auto& p : socketToRollingLogFollowQueue)
                socketToRollingLogFollowQueue[p.first] += log + "\n";
        }

        bool isRunning() {
            std::shared_lock<std::shared_mutex> r(m);
            return running;
        }

        void stopFor(int socket) {
            std::unique_lock<std::shared_mutex> w(m);
            socketToRollingLogFollowQueue.erase(socket);
            if (socketToRollingLogFollowQueue.empty())
                running = false;
        }

        void startFor(int socket) {
            std::unique_lock<std::shared_mutex> w(m);
            socketToRollingLogFollowQueue[socket] = std::format("[LOG] Following log to socket: {} started\n", socket);
            running                               = true;
        }

        static SRollingLogFollow& get() {
            static SRollingLogFollow    instance;
            static std::mutex           gm;
            std::lock_guard<std::mutex> lock(gm);
            return instance;
        };
    };
}
