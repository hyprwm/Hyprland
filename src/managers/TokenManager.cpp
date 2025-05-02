#include "TokenManager.hpp"
#include <uuid/uuid.h>
#include <algorithm>

CUUIDToken::CUUIDToken(const std::string& uuid_, std::any data_, Time::steady_dur expires) : m_data(data_), m_uuid(uuid_) {
    m_expiresAt = Time::steadyNow() + expires;
}

std::string CUUIDToken::getUUID() {
    return m_uuid;
}

std::string CTokenManager::getRandomUUID() {
    std::string uuid;
    do {
        uuid_t uuid_;
        uuid_generate_random(uuid_);
        uuid = std::format("{:02x}{:02x}{:02x}{:02x}-{:02x}{:02x}-{:02x}{:02x}-{:02x}{:02x}-{:02x}{:02x}{:02x}{:02x}{:02x}{:02x}", (uint16_t)uuid_[0], (uint16_t)uuid_[1],
                           (uint16_t)uuid_[2], (uint16_t)uuid_[3], (uint16_t)uuid_[4], (uint16_t)uuid_[5], (uint16_t)uuid_[6], (uint16_t)uuid_[7], (uint16_t)uuid_[8],
                           (uint16_t)uuid_[9], (uint16_t)uuid_[10], (uint16_t)uuid_[11], (uint16_t)uuid_[12], (uint16_t)uuid_[13], (uint16_t)uuid_[14], (uint16_t)uuid_[15]);
    } while (m_tokens.contains(uuid));

    return uuid;
}

std::string CTokenManager::registerNewToken(std::any data, Time::steady_dur expires) {
    std::string uuid = getRandomUUID();

    m_tokens[uuid] = makeShared<CUUIDToken>(uuid, data, expires);
    return uuid;
}

SP<CUUIDToken> CTokenManager::getToken(const std::string& uuid) {

    // cleanup expired tokens
    const auto NOW = Time::steadyNow();
    std::erase_if(m_tokens, [&NOW](const auto& el) { return el.second->m_expiresAt < NOW; });

    if (!m_tokens.contains(uuid))
        return {};

    return m_tokens.at(uuid);
}

void CTokenManager::removeToken(SP<CUUIDToken> token) {
    if (!token)
        return;
    m_tokens.erase(token->m_uuid);
}
