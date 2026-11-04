#ifndef KAMI_PROTOCOL_OFFICIAL_EXTRA_PARAMS_H
#define KAMI_PROTOCOL_OFFICIAL_EXTRA_PARAMS_H
/*
 * Versioned LGPL 2.1 ABI: SL ObjectUpdate ExtraParams parser.
 * The type codes and known payload-length rules follow Linden Lab
 * LLNetworkData in indra/llprimitive/llprimitive.h and llprimitive.cpp.
 * This is NOT the complete LLMessageSystem or world/asset decoder.
 */
#include <stdint.h>
#if defined(_WIN32)
# if defined(KAMI_SL_EXTRAPARAMS_BUILD)
#  define KAMI_SL_EXTRA_EXPORT __declspec(dllexport)
# else
#  define KAMI_SL_EXTRA_EXPORT __declspec(dllimport)
# endif
# define KAMI_SL_EXTRA_CALL __cdecl
#else
# define KAMI_SL_EXTRA_EXPORT
# define KAMI_SL_EXTRA_CALL
#endif
#ifdef __cplusplus
extern "C" {
#endif
typedef struct KamiSlExtraParamsV1 {
    uint32_t struct_size;
    uint32_t records;
    uint32_t flexible;
    uint32_t light;
    uint32_t sculpt;
    uint32_t mesh_sculpt;
    uint32_t light_image;
    uint32_t extended_mesh;
    uint32_t render_material;
    uint32_t reflection_probe;
    uint32_t unknown;
    uint8_t mesh_asset_uuid[16];
} KamiSlExtraParamsV1;

/* Return 1 if a complete, well-formed ExtraParams record set was decoded,
 * including a valid 0-record case, or 0 on malformed/incompatible input.
 * Unknown future parameter types are safely skipped, not mislabeled as
 * mesh; recognized types must match the official size contract.
 * Output is zeroed except struct_size on failure. */
KAMI_SL_EXTRA_EXPORT int32_t KAMI_SL_EXTRA_CALL
kami_sl_decode_extra_params_v1(const uint8_t* bytes,
                                uint32_t bytes_length,
                                KamiSlExtraParamsV1* output);
#ifdef __cplusplus
}
#endif
#endif
