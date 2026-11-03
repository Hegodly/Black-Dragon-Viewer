/*
 * KAMI Second Life Windows login device identity compatibility module.
 *
 * Adapted from the volume serial hashing behavior in Linden Lab's
 * official viewer, indra/newview/llappviewerwin32.cpp,
 * LLAppViewerWin32::generateSerialNumber(), at:
 * https://github.com/secondlife/viewer/tree/b645b767684ccabdfd285f0c2a55855697029909
 *
 * Original viewer component: Copyright (C) Linden Research, Inc.
 * This adapted compatibility module is available under GNU LGPL v2.1
 * (or, at your option, a later version) and is deliberately built as
 * a standalone DLL to retain KAMI's separate software boundary.
 * Full terms: third_party/secondlife-lgpl/LICENSE.LGPL-2.1.txt.
 *
 * No physical volume serial is transmitted. Only the 32-digit MD5 hash
 * required by Second Life's documented Windows viewer login behavior
 * is returned. This is NOT a security hash and is not for passwords.
 */
#include "kami/official_login_identity.hpp"
#include <windows.h>
#include <bcrypt.h>
#include <array>
#include <cstring>

extern "C" bool kami_sl_hash_volume_serial(
    std::uint32_t serial,char* output,std::size_t capacity) noexcept
{
    if(!output||capacity<33)return false;
    output[0]='\0';
    BCRYPT_ALG_HANDLE provider=nullptr;
    if(!BCRYPT_SUCCESS(BCryptOpenAlgorithmProvider(
        &provider,BCRYPT_MD5_ALGORITHM,nullptr,0)))return false;

    // On Windows, DWORD is 32 bits and uses little-endian storage. This
    // matches official LLMD5::update(&serial, sizeof(DWORD)).
    std::array<unsigned char,sizeof(std::uint32_t)> volume_bytes{};
    static_assert(sizeof(DWORD)==sizeof(std::uint32_t));
    std::memcpy(volume_bytes.data(),&serial,volume_bytes.size());
    std::array<unsigned char,16> digest{};
    const NTSTATUS status=BCryptHash(
        provider,nullptr,0,volume_bytes.data(),
        static_cast<ULONG>(volume_bytes.size()),digest.data(),
        static_cast<ULONG>(digest.size()));
    BCryptCloseAlgorithmProvider(provider,0);
    SecureZeroMemory(volume_bytes.data(),volume_bytes.size());
    if(!BCRYPT_SUCCESS(status)){
        SecureZeroMemory(digest.data(),digest.size());
        return false;
    }
    constexpr char digits[]="0123456789abcdef";
    for(std::size_t i=0;i<digest.size();++i){
        output[i*2]=digits[digest[i]>>4];
        output[i*2+1]=digits[digest[i]&15U];
    }
    output[32]='\0';
    SecureZeroMemory(digest.data(),digest.size());
    return true;
}

extern "C" bool kami_sl_host_volume_id(
    char* output,std::size_t capacity) noexcept
{
    if(!output||capacity<33)return false;
    output[0]='\0';
    DWORD volume_serial=0;
    // Official SL Windows viewer hashes this 4-byte C: volume serial.
    const BOOL ok=GetVolumeInformationW(L"C:\\",nullptr,0,
        &volume_serial,nullptr,nullptr,nullptr,0);
    if(!ok)return false;
    const bool result=kami_sl_hash_volume_serial(
        static_cast<std::uint32_t>(volume_serial),output,capacity);
    SecureZeroMemory(&volume_serial,sizeof(volume_serial));
    return result;
}
