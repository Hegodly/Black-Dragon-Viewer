#include "kami/official_login_identity.hpp"
#include <cstring>
#include <iostream>

int main(){
    char out[33]{};
    if(!kami_sl_hash_volume_serial(0x12345678U,out,sizeof(out)))return 1;
    if(std::strcmp(out,"f6532621685700b88aecbf202dc8f9d2")!=0)return 2;
    if(!kami_sl_hash_volume_serial(0U,out,sizeof(out)))return 3;
    if(std::strcmp(out,"f1d3ff8443297732862df21dc4e57262")!=0)return 4;
    char short_out[4]{};
    if(kami_sl_hash_volume_serial(1U,short_out,sizeof(short_out)))return 5;
    if(kami_sl_hash_volume_serial(1U,nullptr,0))return 6;
    // Never access machine identifiers during tests or in CI.
    std::cout<<"PASS: LL Windows volume-ID hash matches independent vectors\n";
    return 0;
}
