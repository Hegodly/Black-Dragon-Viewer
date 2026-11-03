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
    if(!kami_sl_hash_machine_serial(
            L"ABCDEF12-3456-7890-ABCD-EF1234567890",out,sizeof(out)))
        return 7;
    if(std::strcmp(out,"3fe49412bfd944b073e0debb8c24da74")!=0)
        return 8;
    if(!kami_sl_hash_machine_serial(
            L"01234567-89AB-CDEF-0123-456789ABCDEF",out,sizeof(out)))
        return 9;
    if(std::strcmp(out,"daac1c6d2de08e702f2675cd95a34f52")!=0)
        return 10;
    if(kami_sl_hash_machine_serial(L"",out,sizeof(out)))return 11;
    if(kami_sl_hash_machine_serial(L"Test",out,4))return 12;
    // Never access machine identifiers during tests or in CI.
    std::cout<<"PASS: LL Windows volume and WMI machine-ID hashes match independent vectors\n";
    return 0;
}
