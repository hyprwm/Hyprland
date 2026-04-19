#pragma once
#include <map>

#include "../shared.hpp"

inline std::map<const char*, CTestCase&> mainTestCases;

// Where `TEST_CASE` macros will store generated test cases:
#define TEST_CASES_STORAGE mainTestCases
