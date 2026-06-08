#pragma once
#include <map>
#include <string>

#include "../../shared.hpp"

inline std::map<std::string, CTestCase&> clientTestCases;

#ifndef INCLUDED_FROM_MAIN
// Where `TEST_CASE` macros will store generated test cases:
#define TEST_CASES_STORAGE clientTestCases
#endif // !defined(INCLUDED_FROM_MAIN)
