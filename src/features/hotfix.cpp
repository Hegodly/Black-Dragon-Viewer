#include "kami/features/hotfix.hpp"
#include <windows.h>
#include <bcrypt.h>
#include <array>
#include <algorithm>
#include <limits>
namespace kami::hotfix {
namespace {
char hex_digit(std::uint8_t x) noexcept {
    return x < 10 ? static_cast<char>('0'+x):static_cast<char>('a'+x-10);
}
} // namespace
bool verify_sha256(const std::vector<std::uint8_t>& payload,
                   const std::string& expected_hex) noexcept {
    if(expected_hex.size()!=64 || payload.size()>
       static_cast<std::size_t>(std::numeric_limits<ULONG>::max()))return false;
    BCRYPT_ALG_HANDLE algorithm=nullptr;
    BCRYPT_HASH_HANDLE hashing=nullptr;
    std::array<UCHAR,32> digest{};
    bool ok=false;
    if(BCRYPT_SUCCESS(BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,
                                                  nullptr,0))) {
        if(BCRYPT_SUCCESS(BCryptCreateHash(algorithm,&hashing,nullptr,0,nullptr,0,0))) {
            auto* data=payload.empty()?nullptr:
                 const_cast<UCHAR*>(reinterpret_cast<const UCHAR*>(payload.data()));
            if(BCRYPT_SUCCESS(BCryptHashData(hashing,data,static_cast<ULONG>(payload.size()),0)) &&
               BCRYPT_SUCCESS(BCryptFinishHash(hashing,digest.data(),
                                               static_cast<ULONG>(digest.size()),0))) {
                ok=true;
                for(std::size_t i=0;i<digest.size();++i) {
                    char lo=hex_digit(digest[i]&15),hi=hex_digit(digest[i]>>4);
                    const char expected_hi=expected_hex[i*2],expected_lo=expected_hex[i*2+1];
                    const auto lower=[](char c)->char{
                        return c>='A'&&c<='F'?static_cast<char>(c+32):c;
                    };
                    ok&=(lower(expected_hi)==hi && lower(expected_lo)==lo);
                }
            }
            BCryptDestroyHash(hashing);
        }
        BCryptCloseAlgorithmProvider(algorithm,0);
    }
    return ok;
}
Result validate(const Manifest& manifest,
                const std::vector<std::uint8_t>& payload) noexcept {
    if(manifest.to_version<=manifest.from_version)return Result::InvalidVersion;
    if(!manifest.trusted_release_signature)return Result::UntrustedManifest;
    if(manifest.package_size!=payload.size())return Result::SizeMismatch;
    if(!verify_sha256(payload,manifest.sha256_hex))return Result::DigestMismatch;
    return Result::ValidatedForStaging;
}
} // namespace kami::hotfix
