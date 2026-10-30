#pragma once
#include <cstdint>

namespace apcs {
// SDK calls can pump a nested event loop. Never replace controls or run another
// navigation action while the first action is still using the current page.
class UiLifecycle {
    bool action=false,rebuilding=false;
    std::uint64_t revision=0;
public:
    bool busy()const{return action||rebuilding;}
    bool beginAction(){if(busy())return false;action=true;return true;}
    void endAction(){action=false;}
    bool beginRender(){if(busy())return false;rebuilding=true;++revision;return true;}
    void endRender(){rebuilding=false;}
    std::uint64_t epoch()const{return revision;}
    bool accepts(std::uint64_t eventEpoch)const{return !busy()&&eventEpoch==revision;}
};
}
