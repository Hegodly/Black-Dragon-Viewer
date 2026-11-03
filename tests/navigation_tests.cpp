#include "kami/features/navigation.hpp"
int main(){
 using namespace kami::navigation;
 Planner p;
 if(p.follow()!=Action::NotConnected)return 1;
 p.set_connected(true);
 if(p.teleport_to()!=Action::TargetUnavailable)return 2;
 p.track({"avatar",128.f,42.f,50.f,true,false});
 if(p.follow()!=Action::PermissionRequired)return 3;
 p.track({"avatar",128.f,42.f,50.f,true,true});
 if(p.follow()!=Action::RequestReady)return 4;
 if(p.teleport_to()!=Action::RequestReady)return 5;
 return 0;
}
