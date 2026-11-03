#include "kami/features/social.hpp"
int main() {
 using namespace kami::social;
 Outbox box;
 if(box.compose(L"",L"Hi")!=0)return 1;
 auto id=box.compose(L"resident",L"Hello");
 if(!id||!box.queue(id))return 2;
 if(box.confirm_by_transport(id,Receipt::Read,false))return 3;
 if(box.messages().front().receipt!=Receipt::Queued)return 4;
 if(!box.confirm_by_transport(id,Receipt::Transmitted,true))return 5;
 if(box.confirm_by_transport(id,Receipt::Queued,true))return 6;
 if(!box.confirm_by_transport(id,Receipt::Delivered,true))return 7;
 if(!box.confirm_by_transport(id,Receipt::Read,true))return 8;
 if(!visible_viewer({L"KAMI",true,false}).empty())return 9;
 if(visible_viewer({L"KAMI",true,true})!=L"KAMI")return 10;
 return 0;
}
