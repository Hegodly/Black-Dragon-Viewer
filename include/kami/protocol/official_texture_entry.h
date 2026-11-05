#ifndef KAMI_PROTOCOL_OFFICIAL_TEXTURE_ENTRY_H
#define KAMI_PROTOCOL_OFFICIAL_TEXTURE_ENTRY_H
/*
 * Replaceable LGPL 2.1 ONLY compatibility module for Linden Lab's
 * LLPrimitive::unpack_TEField / parseTEMessage wire semantics.
 *
 * Original: https://github.com/secondlife/viewer/blob/
 * bb143726c780303e8302bfd7699d3d2d95935c39/
 * indra/llprimitive/llprimitive.cpp
 * This fixed-size, versioned C ABI does not expose STL/LL classes.
 */
#include <stdint.h>
#if defined(_WIN32)
# if defined(KAMI_SL_TECODEC_BUILD)
#  define KAMI_SL_TE_API __declspec(dllexport)
# else
#  define KAMI_SL_TE_API __declspec(dllimport)
# endif
# define KAMI_SL_TE_CALL __cdecl
#else
# define KAMI_SL_TE_API
# define KAMI_SL_TE_CALL
#endif
#ifdef __cplusplus
extern "C" {
#endif
enum { KAMI_SL_TE_MAX_FACES=45, KAMI_SL_TE_MAX_BYTES=4096 };
typedef struct KamiSlTEFaceV1 {
    uint8_t image_id[16];
    uint8_t inverted_rgba[4];
    float scale_s;
    float scale_t;
    int16_t offset_s;
    int16_t offset_t;
    int16_t packed_rotation;
    uint8_t bump_shiny_fullbright;
    uint8_t media_texgen;
    uint8_t glow;
    uint8_t image_override;
    uint8_t material_id[16];
} KamiSlTEFaceV1;
typedef struct KamiSlTextureEntryV1 {
    uint32_t struct_size; /* Must be sizeof(KamiSlTextureEntryV1). */
    uint32_t face_count;
    uint8_t default_image_id[16];
    uint8_t default_inverted_rgba[4];
    KamiSlTEFaceV1 faces[KAMI_SL_TE_MAX_FACES];
} KamiSlTextureEntryV1;
/*
 * Returns 1 for a complete, valid official TextureEntry field sequence;
 * 0 for invalid or truncated data. Input is untrusted and strictly bounded.
 * No allocation, no network, no D3D12/OpenGL, no callbacks.
 * The source code for this DLL must remain available under LGPL 2.1.
 */
KAMI_SL_TE_API int32_t KAMI_SL_TE_CALL
kami_sl_decode_texture_entry_v1(const uint8_t* buffer,uint32_t buffer_length,
                                uint32_t face_count,
                                KamiSlTextureEntryV1* output);
#ifdef __cplusplus
}
#endif
#endif
