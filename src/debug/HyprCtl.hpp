#pragma once

#include "../Compositor.hpp"
#include <fstream>
#include "../helpers/MiscFunctions.hpp"
#include <functional>

class CHyprCtl {
  public:
    CHyprCtl();

    enum eHyprCtlOutputFormat {
        FORMAT_NORMAL = 0,
        FORMAT_JSON
    };

    struct SCommand {
        std::string                                                   name = "";
        bool                                                          exact = true;
        std::function<std::string(eHyprCtlOutputFormat, std::string)> fn;
    };

    std::string               makeDynamicCall(const std::string& input);
    std::shared_ptr<SCommand> registerCommand(SCommand cmd);
    void                      unregisterCommand(const std::shared_ptr<SCommand>& cmd);
    std::string               getReply(std::string);

    int                       m_iSocketFD = -1;

  private:
    void                                   startHyprCtlSocket();

    std::vector<std::shared_ptr<SCommand>> m_vCommands;
};

inline std::unique_ptr<CHyprCtl> g_pHyprCtl;