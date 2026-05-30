#pragma once
#include <map>
#include <string>

#include "../shared.hpp"

inline std::map<std::string, CTestCase&> mainTestCases;

// Where `TEST_CASE` macros will store generated test cases:
#define TEST_CASES_STORAGE mainTestCases
