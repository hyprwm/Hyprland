#include "VarList.hpp"
#include <ranges>
#include <algorithm>

CVarList::CVarList(const std::string& in, const size_t lastArgNo, const char delim, const bool removeEmpty) {
    if (in.empty())
        m_vArgs.emplace_back("");

    std::string args{in};
    size_t      idx = 0;
    size_t      pos = 0;
    std::ranges::replace_if(
        args, [&](const char& c) { return delim == 's' ? std::isspace(c) : c == delim; }, 0);

    for (const auto& s : args | std::views::split(0)) {
        if (removeEmpty && s.empty())
            continue;
        if (++idx == lastArgNo) {
            m_vArgs.emplace_back(removeBeginEndSpacesTabs(in.substr(pos)));
            break;
        }
        pos += s.size() + 1;
        m_vArgs.emplace_back(removeBeginEndSpacesTabs(std::string_view{s}.data()));
    }
}

std::string CVarList::join(const std::string& joiner, size_t from, size_t to) const {
    size_t      last = to == 0 ? size() : to;

    std::string rolling;
    for (size_t i = from; i < last; ++i) {
        rolling += m_vArgs[i] + (i + 1 < last ? joiner : "");
    }

    return rolling;
}