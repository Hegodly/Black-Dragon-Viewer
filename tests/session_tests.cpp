#include "kami/session.hpp"
#include "kami/palette.hpp"
int main() {
    kami::protocol::Session session;
    if(session.state()!=kami::protocol::SessionState::Offline) return 1;
    if(session.can_enter_world()) return 2;
    if(session.connect(L"",L"")!=kami::protocol::ConnectionResult::NotImplemented) return 3;
    const auto dark=kami::ui::colors(false);
    const auto light=kami::ui::colors(true);
    if(dark.background.r==light.background.r) return 4;
    if(dark.text.r==light.text.r) return 5;
    return 0;
}
