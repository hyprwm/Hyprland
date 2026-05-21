#include "EventBus.hpp"
#include <expected>

using namespace Event;

UP<CEventBus>& Event::bus() {
    static UP<CEventBus> p = makeUnique<CEventBus>();
    return p;
}

CEventBus::CCustomEvent::CCustomEvent(std::string name, std::vector<eType> argTypes) : m_name(name), m_argTypes(argTypes), m_event({}) {}
CEventBus::CCustomEvent::~CCustomEvent() = default;

std::expected<void, std::string> CEventBus::CCustomEvent::emit(const std::vector<ValidVariant>& args) {
    if (args.size() != m_argTypes.size())
        return std::unexpected(std::format("Too {} args, expected {}", args.size() > m_argTypes.size() ? "many" : "few", m_argTypes.size()));

    for (size_t i = 0; i < args.size(); i++) {
        const auto& arg = args[i];
        if (arg.index() != m_argTypes[i])
            return std::unexpected(std::format("Invalid type for arg {}", i));
    }

    m_event.emit(args);
    return {};
}
