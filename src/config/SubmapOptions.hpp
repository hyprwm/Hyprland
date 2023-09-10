#pragma once

#include <string>
#include <vector>
#include <memory>

class SubmapOptions {

  public:
    SubmapOptions(std::string name) {
        this->name = name;
        persist    = true;
        consume    = false;

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

    void setConsume(bool consume) {
        this->consume = consume;
    }

    void setPersist(bool persist) {
        this->persist = persist;
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