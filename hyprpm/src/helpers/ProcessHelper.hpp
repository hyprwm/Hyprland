#pragma once

#include <string>
#include <vector>
#include <optional>
#include <functional>

class CProcessHelper {
  public:
    struct ProcessResult {
        int         exitCode;
        std::string output;
        bool        timedOut;
        bool        interrupted;
    };

    // Execute a command with optional timeout (in seconds)
    static ProcessResult exec(const std::string& command, int timeoutSecs = 30);

    // Execute a command with interruption support
    static ProcessResult execInterruptible(const std::string& command,
                                           std::function<bool()> shouldInterrupt,
                                           int timeoutSecs = 30);

  private:
    static bool waitForProcess(pid_t pid, int& status, int timeoutSecs);
};