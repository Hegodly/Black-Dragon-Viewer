#ifndef KAMI_PROTOCOL_OFFICIAL_LLUDP_ZEROCODE_H
#define KAMI_PROTOCOL_OFFICIAL_LLUDP_ZEROCODE_H
#include <stdint.h>
#if defined(_WIN32)
# if defined(KAMI_SL_LLUDP_CODEC_BUILD)
#  define KAMI_SL_LLUDP_EXPORT __declspec(dllexport)
# else
#  define KAMI_SL_LLUDP_EXPORT __declspec(dllimport)
# endif
# define KAMI_SL_LLUDP_CALL __cdecl
#else
# define KAMI_SL_LLUDP_EXPORT
# define KAMI_SL_LLUDP_CALL
#endif
#ifdef __cplusplus
extern "C" {
#endif
// These are *bounded, memory-safe wrapper exports* around the unmodified
// LGPL LLZeroCode algorithm. Only the LLUDP packet's post-header message
// bytes are passed; KAMI retains all other packet, ACK and circuit handling.
// encode: bytes written >0, 0 when compression would not save bytes, -1 error.
// decode: bytes written >=0, -1 if malformed or over the supplied budget.
// Buffers may not overlap. No credentials or network I/O.
KAMI_SL_LLUDP_EXPORT int32_t KAMI_SL_LLUDP_CALL
kami_sl_official_zerocode_encode(const uint8_t* message,
    uint32_t message_length, uint8_t* destination,
    uint32_t destination_capacity);
KAMI_SL_LLUDP_EXPORT int32_t KAMI_SL_LLUDP_CALL
kami_sl_official_zerocode_decode(const uint8_t* compressed,
    uint32_t compressed_length, uint8_t* destination,
    uint32_t destination_capacity);
#ifdef __cplusplus
}
#endif
#endif
