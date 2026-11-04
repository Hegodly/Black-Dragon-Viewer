/*
 * Thin, replaceable LGPL 2.1 DLL around Linden Lab's original
 * indra/llmessage/llzerocode.h, pinned at
 * https://github.com/secondlife/viewer/commit/5a43248759bd1b2e8d7ebf4ce00fe55c181824ac
 *
 * Copyright (C) Linden Research, Inc. for upstream LLZeroCode.
 * Thin adapter copyright (C) KAMI contributors; LGPL v2.1 only.
 * License: third_party/secondlife-lgpl/LICENSE.LGPL-2.1.txt
 * This component is NOT the full LLMessageSystem/session backend.
 */
#include "llzerocode.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

namespace {
constexpr std::uint32_t kMaxWire=8192U;
constexpr std::uint32_t kMaxBody=16384U;
constexpr std::uint32_t kMaxHeader=261U;
bool frame_ok(const std::uint8_t* s,std::uint32_t n,
              std::uint32_t header) noexcept {
    return s&&n>=header+1U&&n<=kMaxWire &&
           header>=6U&&header<=kMaxHeader&&
           header==6U+static_cast<std::uint32_t>(s[5]);
}
// Validate each input run before entering LLZeroCode::decode.
// The official codec predates modern bounds-checked containers and expects
// valid frames. This rejects truncated runs and decompression bombs.
bool valid_encoded_body(const std::uint8_t* src,std::uint32_t n,
                        std::uint32_t h,std::uint32_t& expanded) noexcept {
    std::uint32_t body=0U;
    std::uint32_t i=h;
    while(i<n){
        std::uint32_t add=0U;
        if(src[i++]!=0U) add=1U;
        else {
            if(i>=n)return false;
            while(i<n&&src[i]==0U){
                if(add>kMaxBody-255U)return false;
                add+=255U;
                ++i;
            }
            if(i>=n)return false;
            const auto tail=src[i++];
            if(tail==0U||add>kMaxBody-tail)return false;
            add+=tail;
        }
        if(body>kMaxBody-add)return false;
        body+=add;
    }
    if(body==0U)return false;
    expanded=h+body;
    return true;
}
}
extern "C" __declspec(dllexport) int __cdecl
kami_sl_official_zero_encode_v1(
    const std::uint8_t* source,std::uint32_t size,
    std::uint32_t header_size,std::uint8_t* destination,
    std::uint32_t capacity,std::uint32_t* written) noexcept {
    if(written)*written=0U;
    if(!written||!destination||!frame_ok(source,size,header_size)||
       capacity<size||capacity>2U*kMaxWire)return 0;
    // Preserve header bits except the zero-coded flag, whose setting is
    // dictated by the official compressor and may be absent for small data.
    std::vector<U8> plain(source,source+size);
    plain[0]&=static_cast<U8>(~LLZeroCode::FLAG);
    std::vector<U8> candidate(size*2U);
    const auto result=LLZeroCode::encode(
        plain.data(),size,candidate.data(),
        static_cast<U32>(candidate.size()),header_size);
    if(result>0){
        if(static_cast<std::uint32_t>(result)>capacity)return 0;
        std::memcpy(destination,candidate.data(),
                    static_cast<std::size_t>(result));
        *written=static_cast<std::uint32_t>(result);
    }else{
        std::memcpy(destination,plain.data(),size);
        *written=size;
    }
    return 1;
}
extern "C" __declspec(dllexport) int __cdecl
kami_sl_official_zero_decode_v1(
    const std::uint8_t* source,std::uint32_t size,
    std::uint32_t header_size,std::uint8_t* destination,
    std::uint32_t capacity,std::uint32_t* written) noexcept {
    if(written)*written=0U;
    if(!written||!destination||!frame_ok(source,size,header_size)||
       capacity<header_size+1U||capacity>kMaxBody+kMaxHeader)return 0;
    if(!(source[0]&LLZeroCode::FLAG)){
        if(size>capacity)return 0;
        std::memcpy(destination,source,size);
        *written=size;
        return 1;
    }
    std::uint32_t expected=0U;
    if(!valid_encoded_body(source,size,header_size,expected)||
       expected>capacity)return 0;
    // LLZeroCode::decode expects enough room to handle a 255-byte run.
    if(capacity<256U+header_size)return 0;
    bool overflow=false;
    const auto n=LLZeroCode::decode(source,size,destination,
                                   capacity,header_size,overflow);
    if(overflow||n!=expected)return 0;
    *written=n;
    return 1;
}

// Official LLViewerObject::unpackParameterEntry from
// indra/newview/llviewerobject.cpp normalizes PARAMS_MESH to PARAMS_SCULPT.
// The renderer must obtain the UUID from the 17-byte sculpt payload only;
// the type alias alone does not prove an object has mesh geometry.
extern "C" __declspec(dllexport) int __cdecl
kami_sl_official_extra_sculpt_alias_v1(std::uint16_t type) noexcept {
    return type==0x30U||type==0x60U;
}
