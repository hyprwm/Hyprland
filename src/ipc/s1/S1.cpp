#include "S1.hpp"

#include "Commands.hpp"
#include "Unix.hpp"
#include "../../debug/log/Logger.hpp"

#include <algorithm>
#include <optional>
#include <ranges>
#include <sstream>
#include <hyprutils/string/String.hpp>

using namespace IPC::Socket1;

static constexpr std::string_view BATCH_TOKEN     = "[[BATCH]]";
static constexpr std::string_view BATCH_DELIMITER = "\n\n\n";

SResponse::SResponse() : result(std::string{}) {
    ;
}

SResponse::SResponse(std::string response, eReplyMode mode_) : result(std::move(response)), mode(mode_) {
    ;
}

SResponse::SResponse(const char* response, eReplyMode mode_) : result(std::string{response}), mode(mode_) {
    ;
}

SResponse::SResponse(SP<CPromise<std::string>> promise, eReplyMode mode_) : result(std::move(promise)), mode(mode_) {
    ;
}

UP<CSocket1>& IPC::Socket1::sock() {
    static UP<CSocket1> socket;
    return socket;
}

CSocket1::CSocket1() : m_impl(makeUnique<CUnixImpl>()) {
    registerBuiltinCommands(*this);
    m_impl->start([this](std::string&& request, pid_t pid) { return dispatch(std::move(request), pid); });
}

CSocket1::~CSocket1() = default;

SRequest CSocket1::parseRequest(std::string request, pid_t pid) const {
    SRequest parsed{
        .command = std::move(request),
        .pid     = pid,
    };

    if (!parsed.command.contains('/'))
        return parsed;

    size_t separator = 0;
    for (const auto& character : parsed.command) {
        if (character == '/')
            break;

        if (character == ' ') {
            separator = parsed.command.size();
            break;
        }

        ++separator;

        if (character == 'j')
            parsed.format = FORMAT_JSON;
        else if (character == 'r')
            parsed.refresh = true;
        else if (character == 'a')
            parsed.all = true;
        else if (character == 'c')
            parsed.includeConfig = true;
        else if (character == 'f')
            parsed.follow = true;
    }

    if (separator < parsed.command.size())
        parsed.command = parsed.command.substr(separator + 1);

    return parsed;
}

SResponse CSocket1::dispatchSingle(std::string request, pid_t pid) {
    const auto   parsed = parseRequest(std::move(request), pid);

    SP<SCommand> matched;

    for (const auto& command : m_commands) {
        if (command->match == COMMAND_MATCH_EXACT && command->name == parsed.command) {
            matched = command;
            break;
        }
    }

    if (!matched) {
        for (const auto& command : m_commands) {
            if (command->match == COMMAND_MATCH_PREFIX && parsed.command.starts_with(command->name)) {
                matched = command;
                break;
            }
        }
    }

    if (!matched)
        return "unknown request";

    auto response = matched->handler(parsed);

    if (parsed.refresh)
        refreshState();

    return response;
}

static std::string joinBatchReplies(const std::vector<std::string>& replies) {
    std::string result;

    for (size_t i = 0; i < replies.size(); ++i) {
        if (i != 0)
            result += BATCH_DELIMITER;
        result += replies[i];
    }

    return result;
}

SResponse CSocket1::dispatchBatch(std::string request, pid_t pid) {
    request = request.substr(BATCH_TOKEN.size());

    std::vector<std::string> commands;
    std::stringstream        parsedCommand("");

    for (size_t i = 0; i <= request.size(); ++i) {
        const bool atEnd = i == request.size();
        if (atEnd || request[i] == ';') {
            commands.emplace_back(Hyprutils::String::trim(parsedCommand.str()));
            parsedCommand.str("");
            parsedCommand.clear();
            continue;
        }

        if (request[i] == '\\') {
            if (i < request.size() && (request[i + 1] == '\\' || request[i + 1] == ';'))
                ++i;
            else
                LOG(Log::ERR, "Malformed socket1 request: invalid escape sequence {} at position {}, using it verbatim", request.subview(i, 2), i);
        }
        parsedCommand << request[i];
    }

    std::vector<SResponse> responses;
    responses.reserve(commands.size());

    bool hasDeferred = false;
    for (auto& command : commands) {
        auto response = dispatchSingle(std::move(command), pid);
        if (response.mode == REPLY_MODE_FOLLOW)
            return "follow mode is unavailable in batch requests";

        hasDeferred |= std::holds_alternative<SP<CPromise<std::string>>>(response.result);
        responses.emplace_back(std::move(response));
    }

    if (!hasDeferred) {
        std::vector<std::string> replies;
        replies.reserve(responses.size());
        for (auto& response : responses)
            replies.emplace_back(std::move(std::get<std::string>(response.result)));
        return joinBatchReplies(replies);
    }

    auto aggregate = CPromise<std::string>::make([responses = std::move(responses)](SP<CPromiseResolver<std::string>> resolver) mutable {
        struct SState {
            std::vector<std::string>          replies;
            size_t                            remaining = 0;
            SP<CPromiseResolver<std::string>> resolver;
        };

        auto state = makeShared<SState>();
        state->replies.resize(responses.size());
        state->resolver  = resolver;
        state->remaining = std::ranges::count_if(responses, [](const auto& response) { return std::holds_alternative<SP<CPromise<std::string>>>(response.result); });

        for (size_t i = 0; i < responses.size(); ++i) {
            if (std::holds_alternative<std::string>(responses[i].result)) {
                state->replies[i] = std::move(std::get<std::string>(responses[i].result));
                continue;
            }

            const auto promise = std::get<SP<CPromise<std::string>>>(responses[i].result);
            promise->then([state, i](SP<CPromiseResult<std::string>> result) {
                state->replies[i] = result->hasError() ? result->error() : result->result();

                if (--state->remaining == 0)
                    state->resolver->resolve(joinBatchReplies(state->replies));
            });
        }
    });

    return aggregate;
}

SResponse CSocket1::dispatch(std::string request, pid_t pid) {
    try {
        if (request.starts_with(BATCH_TOKEN))
            return dispatchBatch(std::move(request), pid);

        return dispatchSingle(std::move(request), pid);
    } catch (const std::exception& error) {
        LOG(Log::ERR, "Error in socket1 request: {}", error.what());
        return std::format("Err: {}", error.what());
    }
}

std::string CSocket1::invoke(const std::string& request) {
    auto response = dispatch(request);
    if (std::holds_alternative<SP<CPromise<std::string>>>(response.result))
        return "deferred response unavailable for in-process invocation";
    return std::get<std::string>(std::move(response.result));
}

SP<SCommand> CSocket1::registerCommand(SCommand command) {
    const auto duplicate = std::ranges::find_if(m_commands, [&command](const auto& registered) { return registered->name == command.name && registered->match == command.match; });
    if (duplicate != m_commands.end())
        return nullptr;

    return m_commands.emplace_back(makeShared<SCommand>(std::move(command)));
}

void CSocket1::unregisterCommand(const SP<SCommand>& command) {
    std::erase(m_commands, command);
}
