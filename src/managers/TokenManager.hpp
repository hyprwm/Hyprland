#pragma once

#include <any>
#include <unordered_map>
#include <string>

#include "../helpers/memory/Memory.hpp"
#include "../helpers/time/Time.hpp"

class CUUIDToken {
  public:
    CUUIDToken(const std::string& uuid_, std::any data_, Time::steady_dur expires);

    std::string getUUID();

    std::any    m_data;

  private:
    std::string     m_uuid;
    Time::steady_tp m_expiresAt;

    friend class CTokenManager;
};

class CTokenManager {
  public:
    std::string    registerNewToken(std::any data, std::chrono::steady_clock::duration expires);
    std::string    getRandomUUID();

    SP<CUUIDToken> getToken(const std::string& uuid);
    void           removeToken(SP<CUUIDToken> token);

  private:
    std::unordered_map<std::string, SP<CUUIDToken>> m_mTokens;
};

inline UP<CTokenManager> g_pTokenManager;