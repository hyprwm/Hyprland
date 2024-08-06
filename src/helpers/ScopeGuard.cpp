#include "ScopeGuard.hpp"

CScopeGuard::CScopeGuard(const std::function<void()>& fn_) : fn(fn_) {
    ;
}

CScopeGuard::~CScopeGuard() {
    if (fn)
        fn();
}
