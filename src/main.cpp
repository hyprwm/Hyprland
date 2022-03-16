#include "defines.hpp"
#include "debug/Log.hpp"

int main(int argc, char** argv) {

    

    if (!getenv("XDG_RUNTIME_DIR"))
        RIP("XDG_RUNTIME_DIR not set!");



    return EXIT_SUCCESS;
}
