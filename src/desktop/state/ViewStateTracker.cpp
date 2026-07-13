#include "ViewStateTracker.hpp"
#include "ViewHitTester.hpp"
#include "ViewQuery.hpp"

using namespace Desktop;

CViewQuery IViewStateTracker::query() const {
    return CViewQuery{*this};
}

CViewHitTester IViewStateTracker::hitTest() const {
    return CViewHitTester{*this};
}
