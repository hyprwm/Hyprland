#include "VarList.hpp"

CVarList::CVarList(const std::string& in, long unsigned int lastArgNo, const char separator) {
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
}

std::string CVarList::join(const std::string& joiner, size_t from, size_t to) const {
    size_t      last = to == 0 ? size() : to;

    std::string rolling;
    for (size_t i = from; i < last; ++i) {
        rolling += m_vArgs[i] + (i + 1 < last ? joiner : "");
    }

    return rolling;
}