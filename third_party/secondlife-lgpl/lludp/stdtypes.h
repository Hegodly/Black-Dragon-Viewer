#pragma once
// KAMI adapter's minimal fixed-width compatibility typedefs for the
// unmodified upstream LLZeroCode header. This file is not copied upstream.
#include <cstdint>
using U8 = std::uint8_t;
using U32 = std::uint32_t;
using S32 = std::int32_t;
static_assert(sizeof(U8)==1 && sizeof(U32)==4 && sizeof(S32)==4);
