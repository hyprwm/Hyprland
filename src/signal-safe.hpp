#pragma once

#include "defines.hpp"
#include <cstring>

template <uint16_t N>
class MaxLengthCString {
  public:
    MaxLengthCString() : m_strPos(0), m_boundsExceeded(false) {
        m_str[0] = '\0';
    }
    inline void operator+=(char const* rhs) {
        write(rhs, strlen(rhs));
    }
    void write(char const* data, size_t len) {
        if (m_boundsExceeded || m_strPos + len >= N) {
            m_boundsExceeded = true;
            return;
        }
        memcpy(m_str + m_strPos, data, len);
        m_strPos += len;
        m_str[m_strPos] = '\0';
    }
    void write(char c) {
        if (m_boundsExceeded || m_strPos + 1 >= N) {
            m_boundsExceeded = true;
            return;
        }
        m_str[m_strPos] = c;
        m_strPos++;
    }
    void write_num(size_t num) {
        size_t d = 1;
        while (num / 10 >= d)
            d *= 10;
        while (num > 0) {
            char c = '0' + (num / d);
            write(c);
            num %= d;
            d /= 10;
        }
    }
    char const* get_str() {
        return m_str;
    };
    bool boundsExceeded() {
        return m_boundsExceeded;
    };

  private:
    char   m_str[N];
    size_t m_strPos;
    bool   m_boundsExceeded;
};

template <uint16_t BUFSIZE>
class BufFileWriter {
  public:
    inline BufFileWriter(int fd_) : m_writeBufPos(0), m_fd(fd_) {}
    ~BufFileWriter() {
        flush();
    }
    void write(char const* data, size_t len) {
        while (len > 0) {
            size_t to_add = std::min(len, (size_t)BUFSIZE - m_writeBufPos);
            memcpy(m_writeBuf + m_writeBufPos, data, to_add);
            data += to_add;
            len -= to_add;
            m_writeBufPos += to_add;
            if (m_writeBufPos == BUFSIZE)
                flush();
        }
    }
    inline void write(char c) {
        if (m_writeBufPos == BUFSIZE)
            flush();
        m_writeBuf[m_writeBufPos] = c;
        m_writeBufPos++;
    }
    inline void operator+=(char const* str) {
        write(str, strlen(str));
    }
    inline void operator+=(std::string_view str) {
        write(str.data(), str.size());
    }
    inline void operator+=(char c) {
        write(c);
    }
    void writeNum(size_t num) {
        size_t d = 1;
        while (num / 10 >= d)
            d *= 10;
        while (num > 0) {
            char c = '0' + (num / d);
            write(c);
            num %= d;
            d /= 10;
        }
    }
    void writeCmdOutput(const char* cmd) {
        int pipefd[2];
        if (pipe(pipefd) < 0) {
            *this += "<pipe(pipefd) failed with";
            writeNum(errno);
            *this += ">\n";
            return;
        }
        // terminate child instead of waiting
        {
            struct sigaction act;
            act.sa_handler = SIG_DFL;
            sigemptyset(&act.sa_mask);
            act.sa_flags = SA_NOCLDWAIT;
#ifdef SA_RESTORER
            act.sa_restorer = NULL;
#endif
            sigaction(SIGCHLD, &act, NULL);
        }
        pid_t pid = fork();
        if (pid < 0) {
            *this += "<fork() failed with ";
            writeNum(errno);
            *this += ">\n";
            return;
        }
        if (pid == 0) {
            close(pipefd[0]);
            dup2(pipefd[1], STDOUT_FILENO);
            char const* const argv[] = {"/bin/sh", "-c", cmd, NULL};
            execv("/bin/sh", (char* const*)argv);

            BufFileWriter<64> failmsg(pipefd[1]);
            failmsg += "<execv(";
            failmsg += cmd;
            failmsg += ") resulted in errno ";
            failmsg.write(errno);
            failmsg += ">\n";
            close(pipefd[1]);
            abort();
        } else {
            close(pipefd[1]);
            long len;
            char readbuf[256];
            while ((len = read(pipefd[0], readbuf, 256)) > 0) {
                write(readbuf, len);
            }
            if (len < 0) {
                *this += "<interrupted, read() resulted in errno ";
                writeNum(errno);
                *this += ">\n";
            }
            close(pipefd[0]);
        }
    }
    void flush() {
        size_t i = 0;
        while (i < m_writeBufPos) {
            auto written = ::write(m_fd, m_writeBuf + i, m_writeBufPos - i);
            if (written <= 0) {
                return;
            }
            i += written;
        }
        m_writeBufPos = 0;
    }

  private:
    char   m_writeBuf[BUFSIZE];
    size_t m_writeBufPos;
    int    m_fd;
};

char const* sig_getenv(char const* name);

char const* sig_strsignal(int sig);
