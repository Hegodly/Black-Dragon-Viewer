// KAMI bundled installer launch: starts a trusted local script only after explicit consent.
// The helper must wait for this process to exit before touching the installation.
// SPDX-License-Identifier: LGPL-2.1-only
#pragma once
#if LL_WINDOWS
#include <windows.h>
#include <filesystem>
#include <string>

namespace kami { namespace updates {
inline bool launchTestInstall()
{
    wchar_t exe[MAX_PATH] = {};
    DWORD size = GetModuleFileNameW(nullptr, exe, MAX_PATH);
    if (!size || size >= MAX_PATH) return false;
    try {
        const auto install = std::filesystem::path(exe).parent_path();
        const auto bundled = install / L"app_settings" / L"kami-test-updater.ps1";
        if (!std::filesystem::is_regular_file(bundled)) return false;
        if (!std::filesystem::is_regular_file(
                install / L"app_settings" / L"KAMI-Test-Update.zip")) return false;
        const auto temp = std::filesystem::temp_directory_path()
            / (L"KAMI-Test-Updater-" + std::to_wstring(GetCurrentProcessId()) + L".ps1");
        std::filesystem::copy_file(bundled, temp, std::filesystem::copy_options::overwrite_existing);
        wchar_t sysdir[MAX_PATH] = {};
        const UINT count = GetSystemDirectoryW(sysdir, MAX_PATH);
        if (!count || count >= MAX_PATH) return false;
        const auto ps = std::filesystem::path(sysdir)
            / L"WindowsPowerShell" / L"v1.0" / L"powershell.exe";
        if (!std::filesystem::is_regular_file(ps)) return false;
        std::wstring command = L"\"" + ps.wstring()
            + L"\" -NoProfile -ExecutionPolicy Bypass -File \"" + temp.wstring()
            + L"\" -InstallDir \"" + install.wstring()
            + L"\" -ParentPid " + std::to_wstring(GetCurrentProcessId());
        STARTUPINFOW startup{};
        startup.cb = sizeof(startup);
        PROCESS_INFORMATION proc{};
        if (!CreateProcessW(ps.c_str(), command.data(), nullptr, nullptr,
                FALSE, CREATE_NEW_CONSOLE, nullptr, nullptr, &startup, &proc))
            return false;
        CloseHandle(proc.hThread);
        CloseHandle(proc.hProcess);
        return true;
    }
    catch (...) { return false; }
}

inline bool launchInstall(const std::string& tag)
{
    std::array<unsigned,3> parts{};
    if (!parseSemver(tag, parts) || tag.empty()) return false;
    wchar_t exe[MAX_PATH] = {};
    DWORD size = GetModuleFileNameW(nullptr, exe, MAX_PATH);
    if (!size || size >= MAX_PATH) return false;
    try {
        const std::filesystem::path install = std::filesystem::path(exe).parent_path();
        const auto bundled = install / L"app_settings" / L"kami-updater.ps1";
        if (!std::filesystem::is_regular_file(bundled)) return false;
        const auto temp = std::filesystem::temp_directory_path()
            / (L"KAMI-Updater-" + std::to_wstring(GetCurrentProcessId()) + L".ps1");
        std::filesystem::copy_file(bundled, temp, std::filesystem::copy_options::overwrite_existing);
        wchar_t system[MAX_PATH] = {};
        const UINT count = GetSystemDirectoryW(system, MAX_PATH);
        if (!count || count >= MAX_PATH) return false;
        const auto ps = std::filesystem::path(system) / L"WindowsPowerShell" / L"v1.0" / L"powershell.exe";
        if (!std::filesystem::is_regular_file(ps)) return false;
        const std::wstring wideTag(tag.begin(), tag.end());
        const std::wstring args = L"\"" + ps.wstring()
            + L"\" -NoProfile -ExecutionPolicy Bypass -File \"" + temp.wstring()
            + L"\" -InstallDir \"" + install.wstring()
            + L"\" -ParentPid " + std::to_wstring(GetCurrentProcessId())
            + L" -TargetTag \"" + wideTag + L"\"";
        std::wstring command(args);
        STARTUPINFOW si{};
        si.cb = sizeof(si);
        PROCESS_INFORMATION pi{};
        const BOOL ok = CreateProcessW(ps.c_str(), command.data(),
            nullptr, nullptr, FALSE, CREATE_NEW_CONSOLE, nullptr, nullptr, &si, &pi);
        if (!ok) return false;
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        return true;
    }
    catch (...) { return false; }
}
}} // namespace kami::updates
#endif
