#include "VarList.hpp"
#include <ranges>
#include <algorithm>
#include <assert.h>

auto old_func(const std::string& in, const size_t lastArgNo, const char separator, const bool removeEmpty) {
    std::vector<std::string> m_vArgs;
    //
    std::string curitem  = "";
    std::string argZ     = in;
    const bool  SPACESEP = separator == 's';

    auto        nextItem = [&]() {
        auto idx = lastArgNo != 0 && m_vArgs.size() >= lastArgNo - 1 ? std::string::npos : ([&]() -> size_t {
            if (!SPACESEP)
                return argZ.find_first_of(separator);

            uint64_t pos = -1;
            while (!std::isspace(argZ[++pos]) && pos < argZ.length())
                ;

            return pos < argZ.length() ? pos : std::string::npos;
        }());

        if (idx != std::string::npos) {
            curitem = argZ.substr(0, idx);
            argZ    = argZ.substr(idx + 1);
        } else {
            curitem = argZ;
            argZ    = STRVAL_EMPTY;
        }
    };

    nextItem();

    while (curitem != STRVAL_EMPTY) {
        m_vArgs.push_back(removeBeginEndSpacesTabs(curitem));
        nextItem();
    }
    if (removeEmpty)
        std::erase_if(m_vArgs, [](std::string a) { return a.empty(); });

    return m_vArgs;
}

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
#if HYPRLAND_DEBUG
    auto argv = old_func(in, lastArgNo, delim, removeEmpty);
    assert(m_vArgs.size() == argv.size());
    for (size_t i = 0; i < m_vArgs.size(); i++) {
        assert(m_vArgs[i] == argv[i]);
    }
#endif
}

std::string CVarList::join(const std::string& joiner, size_t from, size_t to) const {
    size_t      last = to == 0 ? size() : to;

    std::string rolling;
    for (size_t i = from; i < last; ++i) {
        rolling += m_vArgs[i] + (i + 1 < last ? joiner : "");
    }

    return rolling;
}