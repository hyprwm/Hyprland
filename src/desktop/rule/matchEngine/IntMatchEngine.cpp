#include "IntMatchEngine.hpp"
#include "../../../debug/Log.hpp"

using namespace Desktop::Rule;

CIntMatchEngine::CIntMatchEngine(const std::string& s) {
    try {
        m_value = std::stoi(s);
    } catch (...) { Debug::log(ERR, "CIntMatchEngine: invalid input {}", s); }
}

bool CIntMatchEngine::match(int other) {
    return m_value == other;
}
