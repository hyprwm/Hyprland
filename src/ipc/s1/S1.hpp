#pragma once

#include "../../helpers/defer/Promise.hpp"
#include "../../helpers/memory/Memory.hpp"

#include <cstdint>
#include <functional>
#include <string>
#include <sys/types.h>
#include <variant>
#include <vector>

namespace IPC::Socket1 {
    enum class eOutputFormat : uint8_t {
        NORMAL = 0,
        JSON,
        FORMAT_NORMAL = NORMAL,
        FORMAT_JSON   = JSON,
    };

    enum class eCommandMatch : uint8_t {
        EXACT = 0,
        PREFIX,
    };

    enum class eReplyMode : uint8_t {
        CLOSE = 0,
        FOLLOW,
    };

    struct SRequest {
        std::string   command;
        eOutputFormat format        = eOutputFormat::NORMAL;
        bool          refresh       = false;
        bool          all           = false;
        bool          includeConfig = false;
        bool          follow        = false;
        pid_t         pid           = 0;
    };

    struct SResponse {
        using TResult = std::variant<std::string, SP<CPromise<std::string>>>;

        SResponse();
        SResponse(std::string response, eReplyMode mode = eReplyMode::CLOSE);
        SResponse(const char* response, eReplyMode mode = eReplyMode::CLOSE);
        SResponse(SP<CPromise<std::string>> promise, eReplyMode mode = eReplyMode::CLOSE);

        TResult    result;
        eReplyMode mode = eReplyMode::CLOSE;
    };

    struct SCommand {
        std::string                               name;
        eCommandMatch                             match = eCommandMatch::EXACT;
        std::function<SResponse(const SRequest&)> handler;
    };

    class IImplementation;

    class CSocket1 {
      public:
        CSocket1();
        ~CSocket1();

        SResponse    dispatch(std::string request, pid_t pid = 0);
        std::string  invoke(const std::string& request);
        SP<SCommand> registerCommand(SCommand command);
        void         unregisterCommand(const SP<SCommand>& command);

      private:
        SRequest                  parseRequest(std::string request, pid_t pid) const;
        SResponse                 dispatchSingle(std::string request, pid_t pid);
        SResponse                 dispatchBatch(std::string request, pid_t pid);

        std::vector<SP<SCommand>> m_commands;
        UP<IImplementation>       m_impl;
    };

    std::string   version(eOutputFormat format);
    std::string   systemInfo(eOutputFormat format, bool includeConfig = false);

    UP<CSocket1>& sock();
}
