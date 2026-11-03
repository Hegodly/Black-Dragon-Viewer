#pragma once
#include <cstdint>
#include <string>
#include <vector>
namespace kami::hotfix {
struct Manifest {
    std::uint32_t from_version = 0;
    std::uint32_t to_version = 0;
    std::string sha256_hex;
    std::uint64_t package_size = 0;
    bool trusted_release_signature = false;
};
enum class Result : std::uint8_t {
    ValidatedForStaging, InvalidVersion, UntrustedManifest, SizeMismatch, DigestMismatch
};
// A digest only guarantees integrity relative to the manifest. KAMI will
// refuse staging unless a separate trusted signature verifier has approved it.
// This module does NOT fetch files or execute an installer.
[[nodiscard]] bool verify_sha256(const std::vector<std::uint8_t>& payload,
                                 const std::string& expected_hex) noexcept;
[[nodiscard]] Result validate(const Manifest& manifest,
                               const std::vector<std::uint8_t>& payload) noexcept;
} // namespace kami::hotfix
