#pragma once

#include <string>
#include <vector>
#include <memory>

class SubmapOptions {

  public:
    SubmapOptions(std::string name) {
        this->name    = name;
        this->persist = true;
        this->consume = false;

        this->hasAtLeastOneResetBinding = false;
    }

    std::string getName() const {
        return this->name;
    }

    bool getPersist() const {
        return this->persist;
    }

    bool getConsume() const {
        return this->consume;
    }

    bool getHasAtLeastOneResetBinding() const {
        return this->hasAtLeastOneResetBinding;
    }

    void setConsume(bool consume) {
        this->consume = consume;
    }

    void setPersist(bool persist) {
        this->persist = persist;
    }

    void addedOneReset() {
        this->hasAtLeastOneResetBinding = true;
    }

  private:
    bool        hasAtLeastOneResetBinding;

    std::string name;
    bool        persist;
    bool        consume;
};

inline std::unique_ptr<std::vector<SubmapOptions>> g_pSubmaps;
