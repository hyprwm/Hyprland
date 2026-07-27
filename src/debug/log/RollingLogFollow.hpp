#pragma once

#include <shared_mutex>
#include <unordered_map>
#include <utility>

namespace Log {
    struct SRollingLogFollow {
        std::unordered_map<int, std::string> m_socketToRollingLogFollowQueue;
        std::shared_mutex                    m_mutex;
        bool                                 m_running = false;

        std::string                          takeLog(int socket) {
            std::unique_lock<std::shared_mutex> w(m_mutex);

            const auto                          queue = m_socketToRollingLogFollowQueue.find(socket);
            if (queue == m_socketToRollingLogFollowQueue.end())
                return {};

            return std::exchange(queue->second, {});
        };

        void addLog(const std::string_view& log) {
            std::unique_lock<std::shared_mutex> w(m_mutex);
            m_running = true;
            for (const auto& p : m_socketToRollingLogFollowQueue) {
                m_socketToRollingLogFollowQueue[p.first] += log;
                m_socketToRollingLogFollowQueue[p.first] += "\n";
            }
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
            static SRollingLogFollow instance;
            return instance;
        };
    };
}
