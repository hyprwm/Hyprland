#pragma once

#include <string>
#include <vector>
#include <memory>

class CSubmapOptions {

  public:
    CSubmapOptions(std::string submapName) {
        name    = submapName;
        persist = true;
        consume = false;

        hasAtLeastOneResetBinding = false;
    }

    std::string getName() const {
        return name;
    }

    bool getPersist() const {
        return persist;
    }

    bool getConsume() const {
        return consume;
    }

    bool getHasAtLeastOneResetBinding() const {
        return hasAtLeastOneResetBinding;
    }

    void setConsume(bool val) {
        consume = val;
    }

    void setPersist(bool val) {
        persist = val;
    }

    void addedOneReset() {
        hasAtLeastOneResetBinding = true;
    }

  private:
    bool        hasAtLeastOneResetBinding;

    std::string name;
    bool        persist;
    bool        consume;
};