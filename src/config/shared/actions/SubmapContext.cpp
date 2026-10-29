#include "SubmapContext.hpp"

#include <ranges>

using namespace Config::Actions;

SP<CSubmapContext> CSubmapContext::snapshot() {
    auto snapshot       = makeShared<CSubmapContext>();
    snapshot->m_submaps = m_submaps;
    return snapshot;
}

bool CSubmapContext::empty() const {
    return m_submaps.empty();
}

bool CSubmapContext::contains(const std::string& submap) const {
    const auto NAMES = m_submaps | std::views::transform(&Keybinds::CSubmap::name);
    return std::ranges::contains(NAMES, submap);
}

std::optional<Keybinds::PSubmap> CSubmapContext::find(const std::string& name, const SP<IHID>& device) const {
    const auto MATCH = std::ranges::find_if(m_submaps, [device, name](const Keybinds::PSubmap& submap) { return submap->name() == name && submap->matchesDevice(device); });

    if (MATCH == m_submaps.end()) {
        return std::nullopt;
    }

    return *MATCH;
}

void CSubmapContext::reset() {
    m_submaps.clear();
}

void CSubmapContext::add(Keybinds::PSubmap&& submap) {
    m_submaps.insert(m_submaps.begin(), std::move(submap));
}

void CSubmapContext::remove(const Keybinds::PSubmap& submap) {
    std::erase(m_submaps, submap);
}

void CSubmapContext::remove(const std::string& submapName) {
    std::erase_if(m_submaps, [submapName](const auto submap) { return submap->name() == submapName; });
}

void CSubmapContext::toggle(Keybinds::PSubmap&& submap) {
    const std::size_t HAS_REMOVED = std::erase(m_submaps, submap);

    if (HAS_REMOVED > 0)
        return;

    add(std::move(submap));
}

void CSubmapContext::uniset(Keybinds::PSubmap&& submap) {
    reset();
    add(std::move(submap));
}

const std::vector<Keybinds::PSubmap>& CSubmapContext::submaps() const {
    return m_submaps;
}
