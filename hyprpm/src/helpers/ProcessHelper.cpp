#include "ProcessHelper.hpp"
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <signal.h>
#include <cstring>
#include <cstdio>
#include <chrono>
#include <thread>
#include <fcntl.h>
#include <poll.h>
#include <iostream>

CProcessHelper::ProcessResult CProcessHelper::exec(const std::string& command, int timeoutSecs) {
    ProcessResult result = {-1, "", false, false};

    int pipefd[2];
    if (pipe(pipefd) == -1) {
        result.output = "Error: pipe() failed";
        return result;
    }

    pid_t pid = fork();

    if (pid == -1) {
        close(pipefd[0]);
        close(pipefd[1]);
        result.output = "Error: fork() failed";
        return result;
    }

    if (pid == 0) {
        // Child process
        close(pipefd[0]); // Close read end

        // Redirect stdout and stderr to pipe
        dup2(pipefd[1], STDOUT_FILENO);
        dup2(pipefd[1], STDERR_FILENO);
        close(pipefd[1]);

        // Execute command through shell
        execl("/bin/sh", "sh", "-c", command.c_str(), nullptr);

        // If execl returns, it failed
        std::cerr << "execl failed: " << strerror(errno) << std::endl;
        _exit(127);
    }

    // Parent process
    close(pipefd[1]); // Close write end

    // Set read end to non-blocking
    int flags = fcntl(pipefd[0], F_GETFL, 0);
    fcntl(pipefd[0], F_SETFL, flags | O_NONBLOCK);

    // Read output with timeout
    auto startTime = std::chrono::steady_clock::now();
    char buffer[4096];

    while (true) {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - startTime).count();

        if (elapsed >= timeoutSecs) {
            // Timeout - kill the process
            kill(pid, SIGTERM);
            usleep(100000); // Give it 100ms to terminate gracefully
            kill(pid, SIGKILL);
            result.timedOut = true;
            break;
        }

        // Check if process is still running
        int status;
        pid_t wpid = waitpid(pid, &status, WNOHANG);

        if (wpid == pid) {
            // Process finished
            if (WIFEXITED(status)) {
                result.exitCode = WEXITSTATUS(status);
            } else if (WIFSIGNALED(status)) {
                result.exitCode = -WTERMSIG(status);
            }

            // Read any remaining output
            while (true) {
                ssize_t bytesRead = read(pipefd[0], buffer, sizeof(buffer) - 1);
                if (bytesRead <= 0) break;
                buffer[bytesRead] = '\0';
                result.output += buffer;
            }
            break;
        }

        // Read available output
        ssize_t bytesRead = read(pipefd[0], buffer, sizeof(buffer) - 1);
        if (bytesRead > 0) {
            buffer[bytesRead] = '\0';
            result.output += buffer;
        }

        // Small sleep to avoid busy waiting
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    close(pipefd[0]);

    // Make sure child is reaped
    int status;
    waitpid(pid, &status, 0);

    return result;
}

CProcessHelper::ProcessResult CProcessHelper::execInterruptible(const std::string& command,
                                                                 std::function<bool()> shouldInterrupt,
                                                                 int timeoutSecs) {
    ProcessResult result = {-1, "", false, false};

    int pipefd[2];
    if (pipe(pipefd) == -1) {
        result.output = "Error: pipe() failed";
        return result;
    }

    pid_t pid = fork();

    if (pid == -1) {
        close(pipefd[0]);
        close(pipefd[1]);
        result.output = "Error: fork() failed";
        return result;
    }

    if (pid == 0) {
        // Child process
        close(pipefd[0]); // Close read end

        // Redirect stdout and stderr to pipe
        dup2(pipefd[1], STDOUT_FILENO);
        dup2(pipefd[1], STDERR_FILENO);
        close(pipefd[1]);

        // Execute command through shell
        execl("/bin/sh", "sh", "-c", command.c_str(), nullptr);

        // If execl returns, it failed
        std::cerr << "execl failed: " << strerror(errno) << std::endl;
        _exit(127);
    }

    // Parent process
    close(pipefd[1]); // Close write end

    // Set read end to non-blocking
    int flags = fcntl(pipefd[0], F_GETFL, 0);
    fcntl(pipefd[0], F_SETFL, flags | O_NONBLOCK);

    // Read output with timeout and interruption support
    auto startTime = std::chrono::steady_clock::now();
    char buffer[4096];

    while (true) {
        // Check for interruption request
        if (shouldInterrupt()) {
            // Kill the process
            kill(pid, SIGTERM);
            usleep(50000); // Give it 50ms to terminate gracefully
            kill(pid, SIGKILL);
            result.interrupted = true;
            break;
        }

        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - startTime).count();

        if (elapsed >= timeoutSecs) {
            // Timeout - kill the process
            kill(pid, SIGTERM);
            usleep(100000); // Give it 100ms to terminate gracefully
            kill(pid, SIGKILL);
            result.timedOut = true;
            break;
        }

        // Check if process is still running
        int status;
        pid_t wpid = waitpid(pid, &status, WNOHANG);

        if (wpid == pid) {
            // Process finished
            if (WIFEXITED(status)) {
                result.exitCode = WEXITSTATUS(status);
            } else if (WIFSIGNALED(status)) {
                result.exitCode = -WTERMSIG(status);
            }

            // Read any remaining output
            while (true) {
                ssize_t bytesRead = read(pipefd[0], buffer, sizeof(buffer) - 1);
                if (bytesRead <= 0) break;
                buffer[bytesRead] = '\0';
                result.output += buffer;
            }
            break;
        }

        // Read available output
        ssize_t bytesRead = read(pipefd[0], buffer, sizeof(buffer) - 1);
        if (bytesRead > 0) {
            buffer[bytesRead] = '\0';
            result.output += buffer;
        }

        // Small sleep to avoid busy waiting
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    close(pipefd[0]);

    // Make sure child is reaped
    int status;
    waitpid(pid, &status, 0);

    return result;
}

bool CProcessHelper::waitForProcess(pid_t pid, int& status, int timeoutSecs) {
    auto startTime = std::chrono::steady_clock::now();

    while (true) {
        pid_t wpid = waitpid(pid, &status, WNOHANG);

        if (wpid == pid) {
            return true; // Process finished
        }

        if (wpid == -1) {
            return false; // Error
        }

        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - startTime).count();

        if (elapsed >= timeoutSecs) {
            return false; // Timeout
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}