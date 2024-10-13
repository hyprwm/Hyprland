#pragma once

#include <string>

class CProgressBar {
  public:
    void        print();
    void        printMessageAbove(const std::string& msg);

    std::string m_szCurrentMessage = "";
    size_t      m_iSteps           = 0;
    size_t      m_iMaxSteps        = 0;
    float       m_fPercentage      = -1; // if != -1, use percentage

  private:
    bool m_bFirstPrint = true;
};
