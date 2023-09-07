#pragma once

#include <string>
#include <vector>
#include <memory>

struct ParseObject {
    const std::string command;
    const std::string value;
};

struct SubmapOptions {
    std::string name;
    bool        persist;
    bool        consume;
};

class SubmapBuilder {

  public:
    SubmapBuilder(std::string name) {
        this->options = SubmapOptions{.name=name, .persist=true,.consume=false};
    }

    std::string getName() {
        return this->options.name;
    }

    bool isParsingSubmap() {
        return this->options.name != "";
    }

    const SubmapOptions getOptions() {
        return this->options;
    }

    void addToDelayList(const std::string COMMAND, const std::string VALUE) {
        if (COMMAND == "persist")
            this->options.persist = VALUE == "true";
        else if (COMMAND == "consume")
            this->options.consume = VALUE == "true";
        else
            this->toParseList.push_back(ParseObject{.command = COMMAND, .value = VALUE});
    }

    const std::vector<ParseObject>& getDelayList() {
        return this->toParseList;
    }

  private:
    SubmapOptions            options;

    std::vector<ParseObject> toParseList;
};

inline std::unique_ptr<std::vector<SubmapOptions>> g_pSubmaps;