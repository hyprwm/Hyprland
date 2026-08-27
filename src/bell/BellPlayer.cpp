#include "BellPlayer.hpp"

#include "impl/Canberra.hpp"

using namespace Bell;

UP<CBellPlayer>& Bell::player() {
    static auto p = makeUnique<CBellPlayer>();
    return p;
}

CBellPlayer::CBellPlayer() : m_impl(makeUnique<CCanberraImpl>()) {
    ;
}

void CBellPlayer::play() const {
    m_impl->play();
}
