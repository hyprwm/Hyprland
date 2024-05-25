#pragma once

#include "../protocols/types/DataDevice.hpp"

struct SXSelection;

class CXDataSource : public IDataSource {
  public:
    CXDataSource(SXSelection&);

    virtual std::vector<std::string> mimes();
    virtual void                     send(const std::string& mime, uint32_t fd);
    virtual void                     accepted(const std::string& mime);
    virtual void                     cancelled();
    virtual void                     error(uint32_t code, const std::string& msg);
    virtual eDataSourceType          type();

  private:
    SXSelection&             selection;
    std::vector<std::string> mimeTypes; // these two have shared idx
    std::vector<uint32_t>    mimeAtoms; //
};