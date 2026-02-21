#pragma once

#include "../../helpers/memory/Memory.hpp"
#include "../Renderbuffer.hpp"
#include <aquamarine/buffer/Buffer.hpp>

class CMonitor;

class CGLRenderbuffer : public IRenderbuffer {
  public:
    CGLRenderbuffer(SP<Aquamarine::IBuffer> buffer, uint32_t format);
    ~CGLRenderbuffer();

    void bind() override;
    void unbind() override;

  private:
    void*  m_image = nullptr;
    GLuint m_rbo   = 0;
};
