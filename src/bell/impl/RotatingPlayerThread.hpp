#pragma once

#include <thread>
#include <string>
#include <functional>
#include <condition_variable>
#include <mutex>
#include <optional>

namespace Bell {
    /*
     * A rotating player thread. This will execute player() on queue() on another thread,
     * to not block the main thread with the playing.
     * Additionally, if queue() is called while the thread is busy, it will be ignored.
     */
    class CRotatingPlayerThread {
      public:
        CRotatingPlayerThread(std::function<void(const std::string&)>&& player);
        ~CRotatingPlayerThread();

        void queue(std::string data);
        void stop();

      private:
        void                                    run();

        std::function<void(const std::string&)> m_player = nullptr;
        std::condition_variable                 m_cv;
        std::mutex                              m_mtx;
        std::optional<std::string>              m_dataPending;
        bool                                    m_busy = false;
        bool                                    m_exit = false;
        std::jthread                            m_thread;
    };
}
