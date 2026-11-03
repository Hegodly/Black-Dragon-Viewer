// KAMI native release discovery: HTTPS metadata only; never executes downloads.
// SPDX-License-Identifier: LGPL-2.1-only
#pragma once

#include <array>
#include <chrono>
#include <future>
#include <limits>
#include <string>
#include <curl/curl.h>
#include <boost/json.hpp>

namespace kami { namespace updates {
enum class Status { NetworkUnavailable, NoPublishedRelease, Current, Available };
struct Result {
    Status status = Status::NetworkUnavailable;
    std::string version;
    bool installable = false;
};

inline bool parseSemver(const std::string& input, std::array<unsigned,3>& parts)
{
    size_t pos = (!input.empty() && (input[0] == 'v' || input[0] == 'V')) ? 1 : 0;
    for (size_t i = 0; i < parts.size(); ++i)
    {
        const size_t start = pos;
        unsigned value = 0;
        while (pos < input.size() && input[pos] >= '0' && input[pos] <= '9')
        {
            const unsigned digit = static_cast<unsigned>(input[pos++] - '0');
            if (value > (std::numeric_limits<unsigned>::max() - digit) / 10)
                return false;
            value = value * 10 + digit;
        }
        if (start == pos) return false;
        parts[i] = value;
        if (i != 2 && (pos >= input.size() || input[pos++] != '.'))
            return false;
    }
    // Do not classify beta or oddly tagged releases as mandatory upgrades.
    return pos == input.size() && input.size() < 34;
}

inline size_t receive(char* bytes, size_t size, size_t count, void* user)
{
    if (size && count > 131072 / size) return 0;
    const size_t length = size * count;
    auto* output = static_cast<std::string*>(user);
    if (length > 131072 - output->size()) return 0;
    output->append(bytes, length);
    return length;
}

inline Result check(const std::string local)
{
    Result result;
    CURL* curl = curl_easy_init();
    if (!curl) return result;
    std::string json;
    curl_easy_setopt(curl, CURLOPT_URL,
        "https://api.github.com/repos/Hegodly/Black-Dragon-Viewer/releases/latest");
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "KAMIViewer-ReleaseCheck/1.0");
    curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "");
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &receive);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &json);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 4L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 8L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
    const CURLcode code = curl_easy_perform(curl);
    long http = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http);
    curl_easy_cleanup(curl);
    if (code != CURLE_OK) return result;
    if (http == 404) { result.status = Status::NoPublishedRelease; return result; }
    if (http != 200) return result;

    try {
        const auto document = boost::json::parse(json);
        if (!document.is_object()) return result;
        const auto& object = document.as_object();
        const auto* tag = object.if_contains("tag_name");
        const auto* link = object.if_contains("html_url");
        const auto* draft = object.if_contains("draft");
        const auto* prerelease = object.if_contains("prerelease");
        if (!tag || !tag->is_string() || !link || !link->is_string()
            || !draft || !draft->is_bool() || !prerelease || !prerelease->is_bool()
            || draft->as_bool() || prerelease->as_bool()) return result;
        const std::string remoteVersion(tag->as_string().c_str());
        const std::string releasePage(link->as_string().c_str());
        // Never follow a malicious release metadata link to another host.
        if (releasePage.rfind(
                "https://github.com/Hegodly/Black-Dragon-Viewer/releases/tag/", 0) != 0)
            return result;
        std::array<unsigned,3> localParts{}, remoteParts{};
        if (!parseSemver(local, localParts)
            || !parseSemver(remoteVersion, remoteParts)) return result;
        result.version = remoteVersion;
        const auto* assets = object.if_contains("assets");
        if (assets && assets->is_array())
        {
            const std::string expectedUrl =
                "https://github.com/Hegodly/Black-Dragon-Viewer/releases/download/"
                + remoteVersion + "/KAMI-Windows-Portable.zip";
            for (const auto& value : assets->as_array())
            {
                if (!value.is_object()) continue;
                const auto& asset = value.as_object();
                const auto* name = asset.if_contains("name");
                const auto* digest = asset.if_contains("digest");
                const auto* download = asset.if_contains("browser_download_url");
                if (!name || !name->is_string() || !digest || !digest->is_string()
                    || !download || !download->is_string()) continue;
                const std::string hash(digest->as_string().c_str());
                if (name->as_string() == "KAMI-Windows-Portable.zip"
                    && std::string(download->as_string().c_str()) == expectedUrl
                    && hash.size() == 71 && hash.compare(0, 7, "sha256:") == 0
                    && hash.find_first_not_of("0123456789abcdefABCDEF", 7) == std::string::npos)
                {
                    result.installable = true;
                    break;
                }
            }
        }
        result.status = remoteParts > localParts ? Status::Available : Status::Current;
    }
    catch (...) { return result; }
    return result;
}

inline std::string& selectedVersion() { static std::string value; return value; }

inline std::future<Result>& pending()
{
    static std::future<Result> future;
    return future;
}
inline void begin(const std::string& installedVersion)
{
    auto& future = pending();
    if (!future.valid())
        future = std::async(std::launch::async, [installedVersion]() {
            return check(installedVersion);
        });
}
inline bool takeReady(Result& out)
{
    auto& future = pending();
    if (!future.valid()
        || future.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready)
        return false;
    try { out = future.get(); }
    catch (...) { out = Result{}; }
    return true;
}
}} // namespace kami::updates
