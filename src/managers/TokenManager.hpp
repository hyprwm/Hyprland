#pragma once

#include <chrono>
#include <any>
#include <unordered_map>
#include <string>

#include "../helpers/memory/Memory.hpp"

class CUUIDToken {
  public:
    CUUIDToken(const std::string& uuid_, std::any data_, std::chrono::system_clock::duration expires);

    std::string getUUID();

    std::any    data;

  private:
    std::string                           uuid;

    std::chrono::system_clock::time_point expiresAt;

    friend class CTokenManager;
};

class CTokenManager {
  public:
    std::string    registerNewToken(std::any data, std::chrono::system_clock::duration expires);
    std::string    getRandomUUID();

    SP<CUUIDToken> getToken(const std::string& uuid);
    void           removeToken(SP<CUUIDToken> token);

  private:
    std::unordered_map<std::string, SP<CUUIDToken>> m_mTokens;
};

inline std::unique_ptr<CTokenManager> g_pTokenManager;