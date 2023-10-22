#pragma once
#include <functional>
#include <vector>
#include <string>
#include "../macros.hpp"

class CVarList {
  public:
    /** Split string into arg list
        @param lastArgNo stop splitting after argv reaches maximum size, last arg will contain rest of unsplit args
        @param delim if delimiter is 's', use std::isspace
        @param removeEmpty remove empty args from argv
    */
    CVarList(const std::string& in, const size_t maxSize = 0, const char delim = ',', const bool removeEmpty = false);

    ~CVarList() = default;

    size_t size() const {
        return m_vArgs.size();
    }

    std::string join(const std::string& joiner, size_t from = 0, size_t to = 0) const;

    void        map(std::function<void(std::string&)> func) {
        for (auto& s : m_vArgs)
            func(s);
    }

    void append(const std::string arg) {
        m_vArgs.emplace_back(arg);
    }

    std::string operator[](const size_t& idx) const {
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