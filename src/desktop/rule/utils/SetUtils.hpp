#pragma once

#include <unordered_set>

namespace Desktop::Rule {
    template <class T>
    bool setsIntersect(const std::unordered_set<T>& A, const std::unordered_set<T>& B) {
        if (A.size() > B.size())
            return setsIntersect(B, A);

        for (const auto& e : A) {
            if (B.contains(e))
                return true;
        }
        return false;
    }
};