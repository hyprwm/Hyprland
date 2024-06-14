#pragma once

#include <shared_mutex>

namespace Debug {
    struct RollingLogFollow {
        std::unordered_map<int, std::string> socketToRollingLogFollowQueue;
        std::shared_mutex                    m;
        bool                                 running                    = false;
        static constexpr size_t              ROLLING_LOG_FOLLOW_TOO_BIG = 8192;

        // Returns true if the queue is empty for the given socket
        bool isEmpty(int socket) {
            std::shared_lock<std::shared_mutex> r(m);
            return socketToRollingLogFollowQueue[socket].empty();
        }

        std::string DebugInfo() {
            std::shared_lock<std::shared_mutex> r(m);
            return std::format("RollingLogFollow, got {} connections", socketToRollingLogFollowQueue.size());
        }

        std::string GetLog(int socket) {
            std::unique_lock<std::shared_mutex> w(m);

            const std::string                   ret = socketToRollingLogFollowQueue[socket];
            socketToRollingLogFollowQueue[socket]   = "";

            return ret;
        };

        void AddLog(std::string log) {
            std::unique_lock<std::shared_mutex> w(m);
            running = true;
            std::vector<int> to_erase;
            for (const auto& p : socketToRollingLogFollowQueue)
                socketToRollingLogFollowQueue[p.first] += log + "\n";
        }

        bool IsRunning() {
            std::shared_lock<std::shared_mutex> r(m);
            return running;
        }

        void StopFor(int socket) {
            std::unique_lock<std::shared_mutex> w(m);
            socketToRollingLogFollowQueue.erase(socket);
            if (socketToRollingLogFollowQueue.empty())
                running = false;
        }

        void StartFor(int socket) {
            std::unique_lock<std::shared_mutex> w(m);
            socketToRollingLogFollowQueue[socket] = std::format("[LOG] Following log to socket: {} started\n", socket);
            running                               = true;
        }

        static RollingLogFollow& Get() {
            static RollingLogFollow     instance;
            static std::mutex           gm;
            std::lock_guard<std::mutex> lock(gm);
            return instance;
        };
    };
}
