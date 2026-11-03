#include "kami/features/inventory.hpp"
int main() {
    using namespace kami::inventory;
    const std::vector<Item> list={
        {L"[Hayabusa] Reed FATPACK UNPACKER",L"unknown",false},
        {L"tshirt DEMO",L"Brand",false},
        {L"democracy print",L"Brand",false},
        {L"Test folder",L"",true},
        {L"Clip",L"Acme",false}
    };
    auto moves=preview_smart_sort(list);
    if(moves.size()!=4||moves[0].destination!=L"[HAYABUSA]") return 1;
    if(moves.back().destination!=L"[ACME]")return 2;
    auto del=preview_demo_deletions(list);
    if(del.size()!=1||del[0]!=1)return 3;
    if(is_demo_item(L"DEMOGRAPHICS"))return 4;
    if(folder_for({L"[BAD/STORE] Test",L""})!=L"[UNCATEGORIZED]")return 5;
    return 0;
}
