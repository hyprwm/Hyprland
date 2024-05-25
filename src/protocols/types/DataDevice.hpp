#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include "../../helpers/signal/Signal.hpp"

enum eDataSourceType {
    DATA_SOURCE_TYPE_WAYLAND = 0,
    DATA_SOURCE_TYPE_X11,
};

class IDataSource {
  public:
    IDataSource() {}
    virtual ~IDataSource() {}

    virtual std::vector<std::string> mimes()                                    = 0;
    virtual void                     send(const std::string& mime, uint32_t fd) = 0;
    virtual void                     accepted(const std::string& mime)          = 0;
    virtual void                     cancelled()                                = 0;
    virtual bool                     hasDnd();
    virtual bool                     dndDone();
    virtual bool                     used();
    virtual void                     markUsed();
    virtual void                     error(uint32_t code, const std::string& msg) = 0;
    virtual eDataSourceType          type();

    struct {
        CSignal destroy;
    } events;

  private:
    bool wasUsed = false;
};
