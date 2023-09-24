#ifndef HYPRLAND_GROUPACTIVEDIRECTION_HPP
#define HYPRLAND_GROUPACTIVEDIRECTION_HPP

#include <string>

class GroupActiveDirection {
    std::string direction;
    
  public:
    explicit GroupActiveDirection(std::string);
    static GroupActiveDirection fromFocusDirection(std::string);
    
    bool isForward();
    bool isBackwards();
    std::string asString();
};

#endif //HYPRLAND_GROUPACTIVEDIRECTION_HPP
