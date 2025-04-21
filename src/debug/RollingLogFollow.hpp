#pragma once

#include <shared_mutex>

// NOLINTNEXTLINE(readability-identifier-naming)
namespace Debug {
    struct SRollingLogFollow {
        std::unordered_map<int, std::string> m_socketToRollingLogFollowQueue;
        std::shared_mutex                    m_mutex;
        bool                                 m_running                  = false;
        static constexpr size_t              ROLLING_LOG_FOLLOW_TOO_BIG = 8192;

        // Returns true if the queue is empty for the given socket
        bool isEmpty(int socket) {
            std::shared_lock<std::shared_mutex> r(m_mutex);
            return m_socketToRollingLogFollowQueue[socket].empty();
        }

        std::string debugInfo() {
            std::shared_lock<std::shared_mutex> r(m_mutex);
            return std::format("RollingLogFollow, got {} connections", m_socketToRollingLogFollowQueue.size());
        }

        std::string getLog(int socket) {
            std::unique_lock<std::shared_mutex> w(m_mutex);

            const std::string                   ret = m_socketToRollingLogFollowQueue[socket];
            m_socketToRollingLogFollowQueue[socket] = "";

            return ret;
        };

        void addLog(const std::string& log) {
            std::unique_lock<std::shared_mutex> w(m_mutex);
            m_running = true;
            std::vector<int> to_erase;
            for (const auto& p : m_socketToRollingLogFollowQueue)
                m_socketToRollingLogFollowQueue[p.first] += log + "\n";
        }

        bool isRunning() {
            std::shared_lock<std::shared_mutex> r(m_mutex);
            return m_running;
        }

        void stopFor(int socket) {
            std::unique_lock<std::shared_mutex> w(m_mutex);
            m_socketToRollingLogFollowQueue.erase(socket);
            if (m_socketToRollingLogFollowQueue.empty())
                m_running = false;
        }

        void startFor(int socket) {
            std::unique_lock<std::shared_mutex> w(m_mutex);
            m_socketToRollingLogFollowQueue[socket] = std::format("[LOG] Following log to socket: {} started\n", socket);
            m_running                               = true;
        }

        static SRollingLogFollow& get() {
            static SRollingLogFollow    instance;
            static std::mutex           gm;
            std::lock_guard<std::mutex> lock(gm);
            return instance;
        };
    };
}
