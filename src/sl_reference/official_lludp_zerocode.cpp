// KAMI LGPL 2.1 adapter of Linden Lab's unmodified LLZeroCode functions.
// Copyright (C) 2026 KAMI project contributors.
// Licensed under the GNU Lesser General Public License v2.1 ONLY.
// The imported llzerocode.h retains its original Linden Lab notices.
// Keep this DLL replaceable and distribute these source files alongside it.
#include "kami/protocol/official_lludp_zerocode.h"
#include "llzerocode.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>


namespace {
constexpr U32 kWireHeaderSize=6U;
constexpr U32 kMaxMessage=16384U;
constexpr U32 kMaxCompressedMessage=8192U;

bool safe_message(const U8* p,U32 n) noexcept {
    return p!=nullptr&&n>0U&&n<=kMaxMessage;
}
}

extern "C" KAMI_SL_LLUDP_EXPORT int32_t KAMI_SL_LLUDP_CALL
kami_sl_official_zerocode_encode(const uint8_t* message,
    uint32_t message_length,uint8_t* destination,
    uint32_t destination_capacity) {
    if(!safe_message(message,message_length)||!destination||
       destination_capacity<2U)return -1;
    try {
        // LLZeroCode takes a six-byte LLUDP header and compresses the
        // remainder. KAMI passes the post-extra-header message section.
        std::array<U8,kWireHeaderSize+kMaxMessage> packet{};
        std::copy_n(message,message_length,
                    packet.begin()+kWireHeaderSize);
        // The official writer needs up to 2x input capacity even when
        // compression is not worthwhile.
        std::array<U8,2U*(kWireHeaderSize+kMaxMessage)> output{};
        const S32 n=LLZeroCode::encode(
            packet.data(),kWireHeaderSize+message_length,
            output.data(),static_cast<U32>(output.size()),
            kWireHeaderSize);
        if(n<0)return 0; // Keep original uncompressed datagram.
        if(n<=static_cast<S32>(kWireHeaderSize))return -1;
        const auto count=static_cast<U32>(n)-kWireHeaderSize;
        if(count>destination_capacity)return -1;
        std::memcpy(destination,output.data()+kWireHeaderSize,count);
        return static_cast<int32_t>(count);
    }catch(...){return -1;}
}
extern "C" KAMI_SL_LLUDP_EXPORT int32_t KAMI_SL_LLUDP_CALL
kami_sl_official_zerocode_decode(const uint8_t* compressed,
    uint32_t compressed_length,uint8_t* destination,
    uint32_t destination_capacity) {
    if(!compressed||!destination||compressed_length==0U||
       compressed_length>kMaxCompressedMessage||
       destination_capacity==0U||destination_capacity>kMaxMessage)
        return -1;
    // Validate *before* calling the upstream decoder: it treats 00 00 as
    // an unusual 255-run extension, and historically relies on padded
    // buffers. The producer never emits that extension; reject it and
    // check all output bounds instead of decoding ambiguous packet bytes.
    U32 expanded=0;
    for(U32 i=0;i<compressed_length;++i) {
        U32 count=1U;
        if(compressed[i]==0U) {
            if(i+1U>=compressed_length)return -1;
            count=compressed[++i];
            if(count==0U)return -1;
        }
        if(count>destination_capacity-expanded)return -1;
        expanded+=count;
    }
    if(!expanded)return -1;
    try {
        std::array<U8,kWireHeaderSize+kMaxCompressedMessage> input{};
        input[0]=LLZeroCode::FLAG;
        std::copy_n(compressed,compressed_length,
                    input.begin()+kWireHeaderSize);
        // Padding keeps legacy upstream LLZeroCode pointer checks defined.
        std::array<U8,kWireHeaderSize+kMaxMessage+256U> output{};
        bool overflow=false;
        const U32 actual=LLZeroCode::decode(
            input.data(),kWireHeaderSize+compressed_length,
            output.data(),static_cast<U32>(output.size()),
            kWireHeaderSize,overflow);
        if(overflow||actual!=kWireHeaderSize+expanded)return -1;
        std::memcpy(destination,output.data()+kWireHeaderSize,expanded);
        return static_cast<int32_t>(expanded);
    }catch(...){return -1;}
}
