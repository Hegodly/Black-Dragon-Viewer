#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
// Returns 0 on failure, 1 on success. Buffer lengths are caller-owned.
// The supplied packet includes the 6-byte LLUDP header, optional extra
// header, then the uncompressed or compressed message body (NO ACK trailer).
// On successful decoding, zero coding is cleared in the outgoing flags.
// All integers/lengths are bounded; no credentials or URLs are handled.
__declspec(dllimport) int __cdecl kami_sl_official_zero_encode_v1(
    const uint8_t* source, uint32_t size, uint32_t header_size,
    uint8_t* destination, uint32_t capacity, uint32_t* written);
__declspec(dllimport) int __cdecl kami_sl_official_zero_decode_v1(
    const uint8_t* source, uint32_t size, uint32_t header_size,
    uint8_t* destination, uint32_t capacity, uint32_t* written);
// Official LLViewerObject::unpackParameterEntry treats PARAMS_MESH (0x60)
// as PARAMS_SCULPT (0x30), using the same 17-byte sculpt payload.
__declspec(dllimport) int __cdecl
kami_sl_official_extra_sculpt_alias_v1(uint16_t type);
#ifdef __cplusplus
}
#endif
