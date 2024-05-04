#include "TokenManager.hpp"
#include <uuid/uuid.h>
#include <algorithm>

CUUIDToken::CUUIDToken(const std::string& uuid_, std::any data_, std::chrono::system_clock::duration expires) : data(data_), uuid(uuid_) {
    expiresAt = std::chrono::system_clock::now() + expires;
}

std::string CUUIDToken::getUUID() {
    return uuid;
}

std::string CTokenManager::getRandomUUID() {
    std::string uuid;
    do {
        uuid_t uuid_;
        uuid_generate_random(uuid_);
        uuid = std::format("{:02x}{:02x}{:02x}{:02x}-{:02x}{:02x}-{:02x}{:02x}-{:02x}{:02x}-{:02x}{:02x}{:02x}{:02x}{:02x}{:02x}", (uint16_t)uuid_[0], (uint16_t)uuid_[1],
                           (uint16_t)uuid_[2], (uint16_t)uuid_[3], (uint16_t)uuid_[4], (uint16_t)uuid_[5], (uint16_t)uuid_[6], (uint16_t)uuid_[7], (uint16_t)uuid_[8],
                           (uint16_t)uuid_[9], (uint16_t)uuid_[10], (uint16_t)uuid_[11], (uint16_t)uuid_[12], (uint16_t)uuid_[13], (uint16_t)uuid_[14], (uint16_t)uuid_[15]);
    } while (m_mTokens.contains(uuid));

    return uuid;
}

std::string CTokenManager::registerNewToken(std::any data, std::chrono::system_clock::duration expires) {
    std::string uuid = getRandomUUID();

    m_mTokens[uuid] = makeShared<CUUIDToken>(uuid, data, expires);
    return uuid;
}

SP<CUUIDToken> CTokenManager::getToken(const std::string& uuid) {

    // cleanup expired tokens
    const auto NOW = std::chrono::system_clock::now();
    std::erase_if(m_mTokens, [&NOW](const auto& el) { return el.second->expiresAt < NOW; });

    if (!m_mTokens.contains(uuid))
        return {};

    return m_mTokens.at(uuid);
}

void CTokenManager::removeToken(SP<CUUIDToken> token) {
    if (!token)
        return;
    m_mTokens.erase(token->uuid);
}