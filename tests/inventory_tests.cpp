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
    kami::protocol::caps::InventoryEntry movable{};
    movable.id="cccccccc-cccc-4ccc-8ccc-cccccccccccc";
    movable.parent_id="aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa";
    movable.owner_id="12345678-90ab-4cde-8fab-000000000001";
    movable.base_mask=kPermissionMove;movable.owner_mask=kPermissionMove;
    movable.permissions_valid=true;
    const std::vector<std::string> protected_ids={"aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa"};
    const std::string destination="bbbbbbbb-bbbb-4bbb-8bbb-bbbbbbbbbbbb";
    if(!can_request_move(movable,movable.owner_id,destination,protected_ids))return 6;
    auto unsafe=movable;unsafe.owner_mask=0U;
    if(can_request_move(unsafe,movable.owner_id,destination,protected_ids))return 7;
    unsafe=movable;unsafe.owner_id="dddddddd-dddd-4ddd-8ddd-dddddddddddd";
    if(can_request_move(unsafe,movable.owner_id,destination,protected_ids))return 8;
    if(can_request_move(movable,movable.owner_id,movable.parent_id,protected_ids))return 9;
    if(can_request_move(movable,movable.owner_id,protected_ids.front(),protected_ids))return 10;
    unsafe=movable;unsafe.permissions_valid=false;
    if(can_request_move(unsafe,movable.owner_id,destination,protected_ids))return 11;
    if(can_request_move(movable,"not-a-uuid",destination,protected_ids))return 12;
    if(can_request_move(movable,movable.owner_id,"not-a-uuid",protected_ids))return 13;
    auto invalid_protected=protected_ids;invalid_protected.push_back("bad");
    if(can_request_move(movable,movable.owner_id,destination,invalid_protected))return 14;
    return 0;
}
