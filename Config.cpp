#include "Config.h"

#include <algorithm>

#include <glib.h>

#include <libconfig.h>

#include "Log.h"
#include "ConfigHelpers.h"

namespace {

const auto Log = ReStreamerLog;

#if VK_VIDEO_STREAMER
const char* ConfigFileName = "vk-streamer.conf";
const char* AppConfigFileName = "vk-streamer.app.conf";
#elif YOUTUBE_LIVE_STREAMER
const char* ConfigFileName = "live-streamer.conf";
const char* AppConfigFileName = "live-streamer.app.conf";
#else
const char* ConfigFileName = "rtmp-streamer.conf";
const char* AppConfigFileName = "rtmp-streamer.app.conf";
#endif

G_DEFINE_AUTO_CLEANUP_CLEAR_FUNC(config_t, config_destroy)

}

std::string Config::ReStreamer::BuildTargetUrl(
    const char* targetUrl,
    const char* key)
{
#if VK_VIDEO_STREAMER || YOUTUBE_LIVE_STREAMER
    std::string outTargetUrl(targetUrl ? std::string_view(targetUrl) : Config::targetUrlTemplate);
#else
    std::string outTargetUrl(targetUrl ? std::string_view(targetUrl) : std::string_view());
#endif

    std::string::size_type placeholderPos = outTargetUrl.find(
        Config::KeyPlaceholder.data(),
        Config::KeyPlaceholder.size());
    if(placeholderPos == std::string::npos) {
        return outTargetUrl;
    } else if(key) {
        return outTargetUrl.replace(
            placeholderPos,
            Config::KeyPlaceholder.size(),
            key);
    } else {
        assert(false);
        return std::string();
    }
}

std::map<std::string, Config::ReStreamer>::const_iterator
Config::addReStreamer(
    const std::string& id,
    const Config::ReStreamer& reStreamer)
{
    const auto& emplaceResult = reStreamers.emplace(id, reStreamer);
    if(emplaceResult.second) {
        reStreamersOrder.emplace_back(id);
    }

    return emplaceResult.first;
}

void Config::removeReStreamer(const std::string& id)
{
    reStreamers.erase(id);
    reStreamersOrder.erase(
        std::remove(
            reStreamersOrder.begin(),
            reStreamersOrder.end(),
            id),
        reStreamersOrder.end());
}

Config::ReStreamer ConfigChanges::ReStreamerChanges::makeReStreamer() const
{
    return Config::ReStreamer {
        sourceUrl ? *sourceUrl : std::string(),
        description ? *description : std::string(),
        targetUrl ? *targetUrl : std::string(),
        enabled ? *enabled : false,
    };
}

std::string UserConfigPath(const std::string& userConfigDir)
{
    return userConfigDir + "/" + ConfigFileName;
}

std::optional<std::string> AppConfigPath()
{
#ifdef SNAPCRAFT_BUILD
    if(const gchar* snapData = g_getenv("SNAP_DATA")) {
        std::string configFile = snapData;
        configFile += "/";
        configFile += AppConfigFileName;
        return configFile;
    }
#endif

    const std::deque<std::string> configDirs = ::ConfigDirs();
    if(!configDirs.empty())
        return *configDirs.rbegin() + "/" + AppConfigFileName;

    return {};
}

void SaveAppConfig(const Config& appConfig)
{
    const std::optional<std::string>& targetPath = AppConfigPath();
    if(!targetPath) return;

    const auto& reStreamers = appConfig.reStreamers;

    Log()->info("Writing config to \"{}\"", *targetPath);

    g_auto(config_t) config;
    config_init(&config);

    config_setting_t* root = config_root_setting(&config);
    config_setting_t* streamers = config_setting_add(root, "streamers", CONFIG_TYPE_LIST);

    for(auto it = reStreamers.begin(); it != reStreamers.end(); ++it) {
        config_setting_t* streamer = config_setting_add(streamers, nullptr, CONFIG_TYPE_GROUP);

        config_setting_t* id = config_setting_add(streamer, "id", CONFIG_TYPE_STRING);
        config_setting_set_string(id, it->first.c_str());

        config_setting_t* source = config_setting_add(streamer, "source", CONFIG_TYPE_STRING);
        config_setting_set_string(source, it->second.sourceUrl.c_str());

        config_setting_t* target = config_setting_add(streamer, "target", CONFIG_TYPE_STRING);
        config_setting_set_string(target, it->second.targetUrl.c_str());

        config_setting_t* description = config_setting_add(streamer, "description", CONFIG_TYPE_STRING);
        config_setting_set_string(description, it->second.description.c_str());
    }

    if(!config_write_file(&config, targetPath->c_str())) {
        Log()->error("Fail save config. {}. {}:{}",
            config_error_text(&config),
            *targetPath,
            config_error_line(&config));
    };
}
