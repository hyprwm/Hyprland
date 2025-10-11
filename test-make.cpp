#include "hyprpm/src/helpers/ProcessHelper.cpp"
#include <iostream>

int main() {
    std::cout << "Testing make command..." << std::endl;

    // Test the exact command that's hanging
    std::string cmd = "cd /home/sandwich/Develop/hyprexpo-plus && PKG_CONFIG_PATH=\"/var/cache/hyprpm/sandwich/headersRoot/share/pkgconfig\" make all";

    std::cout << "Running: " << cmd << std::endl;
    std::cout << "With 30 second timeout..." << std::endl;

    auto result = CProcessHelper::exec(cmd, 30);

    std::cout << "\nResult:" << std::endl;
    std::cout << "Exit code: " << result.exitCode << std::endl;
    std::cout << "Timed out: " << (result.timedOut ? "yes" : "no") << std::endl;
    std::cout << "Output length: " << result.output.length() << " bytes" << std::endl;

    if (result.output.length() > 0) {
        std::cout << "\nFirst 500 chars of output:" << std::endl;
        std::cout << result.output.substr(0, 500) << std::endl;
    }

    return 0;
}