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
        this->hasAtLeastOneResetBinding = false;
    }

    std::string getName() {
        return this->options.name;
    }

    const SubmapOptions getOptions() {
        return this->options;
    }

    void setConsume(bool consume) {
        this->options.consume = consume;
    }
    
    void setPersist(bool persist) {
        this->options.persist = persist;
    }

    void addedOneReset() {
        this->hasAtLeastOneResetBinding = true;
    }

    bool getHasAtLeastOneResetBinding() {
        return this->hasAtLeastOneResetBinding;
    }

  private:
    bool                     hasAtLeastOneResetBinding;

    SubmapOptions            options;
};

inline std::unique_ptr<std::vector<SubmapOptions>> g_pSubmaps;