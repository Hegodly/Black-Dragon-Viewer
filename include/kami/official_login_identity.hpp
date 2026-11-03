#pragma once
// C ABI for KAMI's separately licensed SL compatibility component.
// Refer to third_party/secondlife-lgpl/NOTICE.md for LGPL obligations.
#include <cstddef>
#include <cstdint>

#if defined(KAMI_SL_LOGIN_IDENTITY_EXPORT)
#define KAMI_SL_LOGIN_IDENTITY_API __declspec(dllexport)
#else
#define KAMI_SL_LOGIN_IDENTITY_API __declspec(dllimport)
#endif

extern "C" {
// Reproduce the official Second Life Windows viewer's volume-serial hash.
// Output is 32 lowercase hex bytes plus a NUL. Never log this pseudonymous ID.
KAMI_SL_LOGIN_IDENTITY_API bool kami_sl_host_volume_id(
    char* output,std::size_t capacity) noexcept;

// Pure deterministic offline test helper; never needs a real device ID.
KAMI_SL_LOGIN_IDENTITY_API bool kami_sl_hash_volume_serial(
    std::uint32_t serial,char* output,std::size_t capacity) noexcept;

// Modern official viewer machine ID uses a folded WMI product UUID
// (fallback: Win32_OperatingSystem SerialNumber), MD5 hashed to 32 hex.
// Returns false if WMI unavailable. No raw UUID or serial is returned.
KAMI_SL_LOGIN_IDENTITY_API bool kami_sl_host_machine_hash(
    char* output,std::size_t capacity) noexcept;

// Offline deterministic test helper for Linden Lab's six-byte fold.
KAMI_SL_LOGIN_IDENTITY_API bool kami_sl_hash_machine_serial(
    const wchar_t* serial,char* output,std::size_t capacity) noexcept;
}
