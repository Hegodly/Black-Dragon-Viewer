#ifndef KAMI_PROTOCOL_OFFICIAL_SL_BRIDGE_API_H
#define KAMI_PROTOCOL_OFFICIAL_SL_BRIDGE_API_H
/* Renderer-neutral ABI contract for an upcoming *separate* LGPL
 * Second Life official-source backend.
 *
 * This interface does NOT implement Second Life networking and does not
 * assert that any official backend is currently installed or running.
 * It intentionally exposes neither Linden Lab C++ types nor GPU objects.
 * An official backend must own its session, protocol, CAPS and reliable
 * transport, and may send only bounded, decoded, typed events to KAMI.
 */
#include <stdint.h>

#if defined(_MSC_VER)
# define KAMI_SL_CALL __cdecl
#else
# define KAMI_SL_CALL
#endif
#define KAMI_SL_OFFICIAL_BACKEND_ABI_V1 UINT32_C(1)

#ifdef __cplusplus
extern "C" {
#endif

/* Event payload pointers belong to the backend; clients may copy them,
 * but may not retain these pointers after the callback returns.
 * All strings are length-prefixed UTF-8 (not necessarily NUL-terminated).
 * No account password, signed CAPS URL, bearer or raw UDP packet is an event.
 */
typedef enum KamiSlEventKindV1 {
    KAMI_SL_EVENT_SESSION = 1,
    KAMI_SL_EVENT_REGION = 2,
    KAMI_SL_EVENT_OBJECT = 3,
    KAMI_SL_EVENT_OBJECT_REMOVED = 4,
    KAMI_SL_EVENT_MESH_ASSET = 5,
    KAMI_SL_EVENT_TEXTURE_ASSET = 6,
    KAMI_SL_EVENT_TERRAIN = 7,
    KAMI_SL_EVENT_AVATAR = 8,
    KAMI_SL_EVENT_CHAT = 9,
    KAMI_SL_EVENT_INVENTORY = 10
} KamiSlEventKindV1;

typedef struct KamiSlEventV1 {
    uint32_t struct_size;
    uint32_t kind;
    uint32_t schema_version;
    uint32_t payload_bytes;
    const void* payload;
} KamiSlEventV1;

typedef void (KAMI_SL_CALL *KamiSlEventCallbackV1)(
    void* user_context, const KamiSlEventV1* event);

typedef struct KamiSlLoginV1 {
    uint32_t struct_size;
    uint32_t grid; /* 1=Agni, 2=Aditi, explicit user choice */
    const char* account_utf8;
    uint32_t account_bytes;
    const char* password_utf8;
    uint32_t password_bytes;
    const char* start_location_utf8;
    uint32_t start_location_bytes;
    uint32_t accepted_terms; /* must reflect user consent */
} KamiSlLoginV1;

/* Stable field-only object snapshot. Position/orientation use SL region
 * coordinates in meters, Z-up; KAMI's DX12 renderer owns Y-up conversion.
 * UUID bytes are binary, not printable strings.
 */
typedef struct KamiSlObjectV1 {
    uint32_t struct_size;
    uint32_t local_id;
    uint32_t parent_local_id;
    uint32_t flags;
    uint8_t object_uuid[16];
    uint8_t mesh_asset_uuid[16]; /* zero if not mesh */
    uint8_t default_texture_uuid[16];
    double position_xyz[3];
    float rotation_xyzw[4];
    float scale_xyz[3];
} KamiSlObjectV1;

typedef struct KamiSlAssetV1 {
    uint32_t struct_size;
    uint32_t asset_kind; /* MESH_ASSET or TEXTURE_ASSET */
    uint8_t asset_uuid[16];
    const uint8_t* bytes; /* owned by backend, callback-duration only */
    uint32_t byte_count;
} KamiSlAssetV1;

/* All function pointers must be non-null for a compatible provider.
 * connect() initiates official-side login/session setup. The implementation
 * must copy or use credentials synchronously, never persist/log them.
 * poll() delivers events to on_event; stop() closes the official circuit.
 * KAMI must not run its legacy simulator circuit in parallel.
 */
typedef struct KamiSlOfficialBackendApiV1 {
    uint32_t abi_version;
    uint32_t struct_size;
    void* (KAMI_SL_CALL *create)(KamiSlEventCallbackV1 on_event,
                                 void* user_context);
    int32_t (KAMI_SL_CALL *connect)(void* session,
                                   const KamiSlLoginV1* request);
    uint32_t (KAMI_SL_CALL *poll)(void* session,
                                 uint32_t event_budget);
    void (KAMI_SL_CALL *stop)(void* session);
    void (KAMI_SL_CALL *destroy)(void* session);
} KamiSlOfficialBackendApiV1;

/* The optional DLL exports:
 *  extern "C" const KamiSlOfficialBackendApiV1*
 *      __cdecl kami_sl_official_backend_api_v1(void);
 *
 * Do not claim "official backend active" merely because this header exists.
 */
typedef const KamiSlOfficialBackendApiV1*
    (KAMI_SL_CALL *KamiSlGetOfficialBackendApiV1)(void);

#ifdef __cplusplus
}
#endif
#endif /* KAMI_PROTOCOL_OFFICIAL_SL_BRIDGE_API_H */
