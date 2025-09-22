#pragma once

#include <string_view>
#include <set>
#include <map>
#include <deque>
#include <optional>

#include <spdlog/common.h>


struct Config
{
    static constexpr std::string_view KeyPlaceholder = "{key}";

    struct ReStreamer;

    spdlog::level::level_enum logLevel = spdlog::level::info;

#if VK_VIDEO_STREAMER
    const static constexpr std::string_view targetUrlTemplate = "rtmp://ovsu.okcdn.ru/input/{key}";
#elif YOUTUBE_LIVE_STREAMER
    const static constexpr std::string_view targetUrlTemplate = "rtmp://a.rtmp.youtube.com/live2/{key}";
#endif

    std::map<std::string, ReStreamer> reStreamers; // uniqueId -> ReStreamer
    std::deque<std::string> reStreamersOrder;
};

struct Config::ReStreamer {
    static std::string BuildTargetUrl(const char* targetUrl, const char* key);
    static std::string BuildTargetUrl(const char* key)
        { return BuildTargetUrl(nullptr, key); }
    static std::string BuildTargetUrl(const std::string& key)
        { return BuildTargetUrl(key.c_str()); }

    std::string sourceUrl;
    std::string description;
    std::string targetUrl;
    bool enabled;
    std::string forceH264ProfileLevelId = "42c015";
};

struct ConfigChanges
{
    struct ReStreamerChanges;

    std::map<std::string, ReStreamerChanges> reStreamersChanges; // uniqueId -> ReStreamer
};

struct ConfigChanges::ReStreamerChanges {
    std::optional<bool> enabled;
};

std::string UserConfigPath(const std::string& userConfigDir);

std::optional<std::string> AppConfigPath();
void SaveAppConfig(const Config& appConfig);
