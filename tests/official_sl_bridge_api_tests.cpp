#include "kami/protocol/official_sl_bridge_api.h"
#include <cstdint>
#include <type_traits>

// Pure ABI test; NOT a connectivity test and NOT an official backend DLL.
static_assert(std::is_standard_layout_v<KamiSlEventV1>);
static_assert(std::is_trivially_copyable_v<KamiSlObjectV1>);
static_assert(std::is_standard_layout_v<KamiSlAssetV1>);
static_assert(std::is_standard_layout_v<KamiSlOfficialBackendApiV1>);
static_assert(sizeof(((KamiSlObjectV1*)nullptr)->mesh_asset_uuid)==16U);
static_assert(sizeof(((KamiSlObjectV1*)nullptr)->position_xyz)==24U);

namespace {
unsigned observed=0U;
void KAMI_SL_CALL on_event(void*,const KamiSlEventV1* event) {
    if(event&&event->struct_size==sizeof(KamiSlEventV1)&&
       event->kind==KAMI_SL_EVENT_OBJECT&&
       event->schema_version==1U&&
       event->payload_bytes==sizeof(KamiSlObjectV1))
        ++observed;
}
} // namespace
int main() {
    if(KAMI_SL_OFFICIAL_BACKEND_ABI_V1!=1U||
       KAMI_SL_EVENT_MESH_ASSET==KAMI_SL_EVENT_TEXTURE_ASSET||
       KAMI_SL_EVENT_INVENTORY==KAMI_SL_EVENT_OBJECT)return 1;
    KamiSlObjectV1 object{};
    object.struct_size=sizeof(object);
    object.local_id=77U;
    object.position_xyz[2]=22.5;
    object.scale_xyz[0]=1.f;
    KamiSlEventV1 event{};
    event.struct_size=sizeof(event);
    event.kind=KAMI_SL_EVENT_OBJECT;
    event.schema_version=1U;
    event.payload_bytes=sizeof(object);
    event.payload=&object;
    on_event(nullptr,&event);
    if(observed!=1U||object.local_id!=77U||
       object.position_xyz[2]!=22.5)return 2;
    KamiSlOfficialBackendApiV1 absent{};
    if(absent.abi_version!=0U||absent.connect||absent.poll)
        return 3;
    // A declared ABI must never be mistaken for a loaded official
    // implementation: no integration code must activate this null API.
    return 0;
}
