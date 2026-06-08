#pragma once
#include <map>
#include <string>

#include "../../shared.hpp"

inline std::map<std::string, CTestCase&> miscTestCases;

#ifndef INCLUDED_FROM_MAIN
// Where `TEST_CASE` macros will store generated test cases:
#define TEST_CASES_STORAGE miscTestCases
#endif // !defined(INCLUDED_FROM_MAIN)
