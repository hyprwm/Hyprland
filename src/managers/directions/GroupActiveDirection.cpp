#include "GroupActiveDirection.hpp"

GroupActiveDirection::GroupActiveDirection(std::string direction) : direction(direction) {}

GroupActiveDirection GroupActiveDirection::fromFocusDirection(std::string direction) {
    if(direction == "l") {
        return GroupActiveDirection("b");
    }
    
    return GroupActiveDirection("f");
}

bool GroupActiveDirection::isForward() {
    return !isBackwards();
}
bool GroupActiveDirection::isBackwards() {
    return direction == "b" || direction == "prev";
}
std::string GroupActiveDirection::asString() {
    return direction;
}
