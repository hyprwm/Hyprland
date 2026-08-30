#include "RotatingPlayerThread.hpp"

using namespace Bell;

CRotatingPlayerThread::CRotatingPlayerThread(std::function<void(const std::string&)>&& player) : m_player(std::move(player)) {
    m_thread = std::jthread([this] { run(); });
}

CRotatingPlayerThread::~CRotatingPlayerThread() {
    stop();
}

void CRotatingPlayerThread::queue(std::string data) {
    {
        std::lock_guard<std::mutex> lg(m_mtx);
        if (m_exit || m_busy || m_dataPending.has_value())
            return;

        m_dataPending = std::move(data);
    }

    m_cv.notify_one();
}

void CRotatingPlayerThread::stop() {
    {
        std::lock_guard<std::mutex> lg(m_mtx);
        m_exit = true;
    }

    m_cv.notify_one();

    if (!m_thread.joinable())
        return;

    m_thread.join();
}

void CRotatingPlayerThread::run() {
    while (true) {
        std::string data;
        {
            std::unique_lock<std::mutex> lock(m_mtx);
            m_cv.wait(lock, [this] { return m_exit || m_dataPending.has_value(); });

            if (m_exit)
                return;

            data   = std::move(*m_dataPending);
            m_busy = true;
            m_dataPending.reset();
        }

        m_player(data);

        std::lock_guard<std::mutex> lg(m_mtx);
        m_busy = false;
    }
}
