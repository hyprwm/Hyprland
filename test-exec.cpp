#include "hyprpm/src/helpers/ProcessHelper.cpp"
#include <iostream>

int main() {
    std::cout << "Testing ProcessHelper..." << std::endl;

    // Test simple command
    std::cout << "\nTest 1: echo hello" << std::endl;
    auto result1 = CProcessHelper::exec("echo hello", 5);
    std::cout << "Exit code: " << result1.exitCode << std::endl;
    std::cout << "Output: " << result1.output << std::endl;
    std::cout << "Timed out: " << (result1.timedOut ? "yes" : "no") << std::endl;

    // Test longer command
    std::cout << "\nTest 2: ls -la /tmp | head -5" << std::endl;
    auto result2 = CProcessHelper::exec("ls -la /tmp | head -5", 5);
    std::cout << "Exit code: " << result2.exitCode << std::endl;
    std::cout << "Output: " << result2.output << std::endl;
    std::cout << "Timed out: " << (result2.timedOut ? "yes" : "no") << std::endl;

    // Test command with timeout
    std::cout << "\nTest 3: sleep 10 (should timeout)" << std::endl;
    auto result3 = CProcessHelper::exec("sleep 10", 2);
    std::cout << "Exit code: " << result3.exitCode << std::endl;
    std::cout << "Output: " << result3.output << std::endl;
    std::cout << "Timed out: " << (result3.timedOut ? "yes" : "no") << std::endl;

    // Test make command
    std::cout << "\nTest 4: cd /home/sandwich/Develop/hyprexpo-plus && make --version" << std::endl;
    auto result4 = CProcessHelper::exec("cd /home/sandwich/Develop/hyprexpo-plus && make --version", 5);
    std::cout << "Exit code: " << result4.exitCode << std::endl;
    std::cout << "Output: " << result4.output << std::endl;
    std::cout << "Timed out: " << (result4.timedOut ? "yes" : "no") << std::endl;

    return 0;
}