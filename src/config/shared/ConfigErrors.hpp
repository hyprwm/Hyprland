#pragma once

#include <expected>
#include <cstdint>
#include <string>
#include <utility>

namespace Config {
    enum class eConfigErrorLevel : uint8_t {
        SILENT = 0,
        INFO,
        WARNING,
        ERROR,
    };

    enum class eConfigErrorCode : uint8_t {
        UNKNOWN = 0,
        INVALID_ARGUMENT,
        NOT_FOUND,
        NO_TARGET,
        INVALID_STATE,
        UNAVAILABLE,
        EXECUTION_FAILED,
        LUA_ERROR,
        INTERNAL,
    };

    struct SConfigError {
        SConfigError() = default;
        SConfigError(std::string msg, eConfigErrorLevel lvl = eConfigErrorLevel::ERROR, eConfigErrorCode c = eConfigErrorCode::UNKNOWN) :
            message(std::move(msg)), level(lvl), code(c) {}
        SConfigError(const char* msg) : message(msg ? msg : "") {}

        std::string       message;
        eConfigErrorLevel level = eConfigErrorLevel::ERROR;
        eConfigErrorCode  code  = eConfigErrorCode::UNKNOWN;
    };

    using ErrorResult = std::expected<void, SConfigError>;

    inline const char* toString(eConfigErrorLevel level) {
        switch (level) {
            case eConfigErrorLevel::SILENT: return "silent";
            case eConfigErrorLevel::INFO: return "info";
            case eConfigErrorLevel::WARNING: return "warning";
            case eConfigErrorLevel::ERROR: return "error";
        }
        return "unknown";
    }

    inline const char* toString(eConfigErrorCode code) {
        switch (code) {
            case eConfigErrorCode::UNKNOWN: return "unknown";
            case eConfigErrorCode::INVALID_ARGUMENT: return "invalid_argument";
            case eConfigErrorCode::NOT_FOUND: return "not_found";
            case eConfigErrorCode::NO_TARGET: return "no_target";
            case eConfigErrorCode::INVALID_STATE: return "invalid_state";
            case eConfigErrorCode::UNAVAILABLE: return "unavailable";
            case eConfigErrorCode::EXECUTION_FAILED: return "execution_failed";
            case eConfigErrorCode::LUA_ERROR: return "lua_error";
            case eConfigErrorCode::INTERNAL: return "internal";
        }
        return "unknown";
    }

    inline std::unexpected<SConfigError> configError(std::string message, eConfigErrorLevel level = eConfigErrorLevel::ERROR, eConfigErrorCode code = eConfigErrorCode::UNKNOWN) {
        return std::unexpected(SConfigError{std::move(message), level, code});
    }
}
