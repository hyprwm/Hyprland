#pragma once

#include <string>

struct ParseObject {
    const std::string command;
    const std::string value;
};

class Submap {

  public:
    Submap(std::string name) {
        this->name    = name;
        this->persist = true;
    }

    std::string getName() {
        return this->name;
    }

    bool isParsingSubmap() {
        return this->name != "";
    }

    void setPersist(bool persist) {
        this->persist = persist;
    }

    bool getPersist() {
        return this->persist;
    }

    void addToDelayList(const std::string COMMAND, const std::string VALUE) {
        if (COMMAND == "persist") {
            this->setPersist(VALUE == "true");
        } else
            this->toParseList.push_back(ParseObject{.command = COMMAND, .value = VALUE});
    }

    const std::vector<ParseObject>& getDelayList() {
        return this->toParseList;
    }

  private:
    std::string              name;
    bool                     persist;

    std::vector<ParseObject> toParseList;
};
