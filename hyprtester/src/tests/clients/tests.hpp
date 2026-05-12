#pragma once
#include <map>

#include "../../shared.hpp"

inline std::map<const char*, CTestCase&> clientTestCases;

// Where `TEST_CASE` macros will store generated test cases:
#define TEST_CASES_STORAGE clientTestCases
