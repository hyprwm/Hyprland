#pragma once
#include <vector>
#include <string>
#include "../macros.hpp"

class CVarList {
  public:
    /* passing 's' as a separator will use std::isspace */
    CVarList(const std::string& in, long unsigned int lastArgNo = 0, const char separator = ',');

    ~CVarList() = default;

    size_t size() const {
        return m_vArgs.size();
    }

    std::string join(const std::string& joiner, size_t from = 0, size_t to = 0) const;

    std::string operator[](const long unsigned int& idx) const {
        if (idx >= m_vArgs.size())
            return "";
        return m_vArgs[idx];
    }

    // for range-based loops
    std::vector<std::string>::iterator begin() {
        return m_vArgs.begin();
    }
    std::vector<std::string>::const_iterator begin() const {
        return m_vArgs.begin();
    }
    std::vector<std::string>::iterator end() {
        return m_vArgs.end();
    }
    std::vector<std::string>::const_iterator end() const {
        return m_vArgs.end();
    }

  private:
    std::vector<std::string> m_vArgs;
};