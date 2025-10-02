#include <glib.h>

#include <libconfig.h>

#include "CxxPtr/GlibPtr.h"
#include "CxxPtr/libconfigDestroy.h"

#if ENABLE_BROWSER_UI
#include "WebRTSP/Http/Config.h"
#include "WebRTSP/Signalling/Config.h"
#endif

#include "Log.h"
#include "Config.h"
#include "ConfigHelpers.h"
#include "StreamerMain.h"

#if ENABLE_GUI
#include "gui/GuiMain.h"
#endif

#if ENABLE_BROWSER_UI
#include "RestApi.h"
#endif


namespace {

enum {
    DEFAULT_HTTP_PORT = 4080,
};

G_DEFINE_AUTO_CLEANUP_CLEAR_FUNC(config_t, config_destroy)

const auto Log = ReStreamerLog;

std::map<std::string, Config::ReStreamer>::const_iterator
FindStreamerId(
    const Config& appConfig,
    const std::string_view& sourceUrl,
    const std::string& targetUrl)
{
    const auto& reStreamers = appConfig.reStreamers;

    for(auto it = reStreamers.begin(); it != reStreamers.end(); ++it) {
        const Config::ReStreamer& reStreamer = it->second;
        if(reStreamer.sourceUrl == sourceUrl && reStreamer.targetUrl == targetUrl) {
            return it;
        }
    }

    return reStreamers.end();
}

void LoadStreamers(
    const config_t& config,
    Config* loadedConfig,
    const Config* appConfig)
{
    const bool userConfigLoading = appConfig != nullptr;

    auto& loadedReStreamers = loadedConfig->reStreamers;

    config_setting_t* streamersConfig = config_lookup(&config, "streamers");
    if(streamersConfig && CONFIG_TRUE == config_setting_is_list(streamersConfig)) {
        const int streamersCount = config_setting_length(streamersConfig);
        for(int streamerIdx = 0; streamerIdx < streamersCount; ++streamerIdx) {
            config_setting_t* streamerConfig =
                config_setting_get_elem(streamersConfig, streamerIdx);
            if(!streamerConfig || CONFIG_FALSE == config_setting_is_group(streamerConfig)) {
                Log()->warn("Wrong streamer config format. Streamer skipped.");
                break;
            }

            const char* id = nullptr;
            if(!userConfigLoading) {
                config_setting_lookup_string(streamerConfig, "id", &id);
            }
            const char* source = nullptr;
            config_setting_lookup_string(streamerConfig, "source", &source);
            const char* target = nullptr;
            config_setting_lookup_string(streamerConfig, "target", &target);
            const char* description = "";
            config_setting_lookup_string(streamerConfig, "description", &description);
            const char* key = nullptr;
#if YOUTUBE_LIVE_STREAMER
            if(userConfigLoading) {
                config_setting_lookup_string(streamerConfig, "youtube-stream-key", &key);
            }
#endif
            config_setting_lookup_string(streamerConfig, "key", &key);
            int enabled = TRUE;
            config_setting_lookup_bool(streamerConfig, "enable", &enabled);

            if(!source) {
                Log()->warn("\"source\" property is empty. Streamer skipped.");
                continue;
            }

            bool needsKey = true;
            if(target) {
                std::string_view targetView = target;
                needsKey = targetView.find(
                    Config::KeyPlaceholder.data(),
                    Config::KeyPlaceholder.size()) != std::string_view::npos;
            }
#if !VK_VIDEO_STREAMER && !YOUTUBE_LIVE_STREAMER
            else {
                Log()->warn("\"target\" property is missing. Streamer skipped.");
                continue;
            }
#endif

            if(needsKey && (!key || key[0] == '\0')) {
                Log()->warn("\"key\" property is missing. Streamer skipped.");
                continue;
            }

            const std::string targetUrl = Config::ReStreamer::BuildTargetUrl(target, key);
            if(loadedReStreamers.end() != FindStreamerId(*loadedConfig, source, targetUrl)) {
                Log()->warn("Found streamer with duplicated \"source\" and \"key\" properties. Streamer skipped.");
                continue;
            }

            if(appConfig) {
                const auto it = FindStreamerId(*appConfig, source, targetUrl);
                if(it != appConfig->reStreamers.end()) {
                    id = it->first.c_str(); // use id generated on some previous launch
                }
            }

            gchar* uniqueId = nullptr;
            if(!id) {
                uniqueId = g_uuid_string_random();
                id = uniqueId;
            }
            GCharPtr uniqueIdPtr(uniqueId);

            loadedConfig->addReStreamer(
                id,
                Config::ReStreamer {
                    source,
                    description,
                    targetUrl,
                    enabled != FALSE });
        }
    }
}

void LoadConfig(
#if ENABLE_BROWSER_UI
    http::Config* loadedHttpConfig,
    signalling::Config* loadedWsConfig,
#endif
    Config* loadedConfig,
    const Config* appConfig,
    const std::string& configFile)
{
    if(!g_file_test(configFile.c_str(),  G_FILE_TEST_IS_REGULAR)) {
        Log()->info("Config \"{}\" not found", configFile);
        return;
    }

    config_t config;
    config_init(&config);
    ConfigDestroy autoConfigDestroy(&config);

    Log()->info("Loading config \"{}\"", configFile);
    if(!config_read_file(&config, configFile.c_str())) {
        Log()->error("Fail load config. {}. {}:{}",
            config_error_text(&config),
            configFile,
            config_error_line(&config));
        return;
    }

    int logLevel = 0;
    if(CONFIG_TRUE == config_lookup_int(&config, "log-level", &logLevel)) {
        if(logLevel > 0) {
            loadedConfig->logLevel =
                static_cast<spdlog::level::level_enum>(
                    spdlog::level::critical - std::min<int>(logLevel, spdlog::level::critical));
        }
    }

#if ENABLE_BROWSER_UI
    const char* wwwRoot = nullptr;
    if(CONFIG_TRUE == config_lookup_string(&config, "www-root", &wwwRoot)) {
        loadedHttpConfig->wwwRoot = wwwRoot;
    }

    int loopbackOnly = false;
    if(CONFIG_TRUE == config_lookup_bool(&config, "loopback-only", &loopbackOnly)) {
        loadedHttpConfig->bindToLoopbackOnly = loopbackOnly != false;
    }

    int httpPort;
    if(CONFIG_TRUE == config_lookup_int(&config, "http-port", &httpPort)) {
        loadedHttpConfig->port = static_cast<unsigned short>(httpPort);
    }

    int wsPort;
    if(CONFIG_TRUE == config_lookup_int(&config, "ws-port", &wsPort)) {
        loadedWsConfig->port = static_cast<unsigned short>(wsPort);
    }
#endif

    const char* source = nullptr;
    config_lookup_string(&config, "source", &source);
    const char* key = nullptr;
    config_lookup_string(&config, "key", &key);

    if(source && key) {
        gchar* uniqueId = g_uuid_string_random();
        GCharPtr uniqueIdPtr(uniqueId);
        loadedConfig->addReStreamer(
            uniqueId,
            Config::ReStreamer {
                source,
                std::string(),
                Config::ReStreamer::BuildTargetUrl(key),
                true });
    }

    LoadStreamers(config, loadedConfig, appConfig);
}

bool LoadConfig(
#if ENABLE_BROWSER_UI
    http::Config* httpConfig,
    signalling::Config* wsConfig,
#endif
    Config* config)
{
    const std::deque<std::string> configDirs = ::ConfigDirs();
    if(configDirs.empty())
        return false;

#if ENABLE_BROWSER_UI
    http::Config loadedHttpConfig = *httpConfig;
    signalling::Config loadedWsConfig = *wsConfig;
#endif
    Config loadedConfig = *config;

#if ENABLE_GUI
    if(const auto& appConfigPath = AppConfigPath()) {
        LoadConfig(
#   if ENABLE_BROWSER_UI
            loadedHttpConfig,
            loadedWsConfig,
#   endif
            &loadedConfig,
            nullptr,
            *appConfigPath);
    }
#else
    Config loadedAppConfig;

    if(const auto& appConfigPath = AppConfigPath()) {
        if(g_file_test(appConfigPath->c_str(), G_FILE_TEST_IS_REGULAR)) {
            g_auto(config_t) config;
            config_init(&config);

            Log()->info("Loading config \"{}\"", *appConfigPath);
            if(!config_read_file(&config, appConfigPath->c_str())) {
                Log()->error("Fail load config. {}. {}:{}",
                    config_error_text(&config),
                    *appConfigPath,
                    config_error_line(&config));
                return false;
            }

            LoadStreamers(config, &loadedAppConfig, nullptr);
        }
    }

    for(const std::string& configDir: configDirs) {
        const std::string& configFile = UserConfigPath(configDir);
        LoadConfig(
#   if ENABLE_BROWSER_UI
            &loadedHttpConfig,
            &loadedWsConfig,
#   endif
            &loadedConfig,
            &loadedAppConfig,
            configFile);
    }
#endif

    bool success = true;
    if(loadedConfig.reStreamers.empty()) {
        Log()->warn("No streamers configured");
    }

    if(success) {
#if ENABLE_BROWSER_UI
        *httpConfig = loadedHttpConfig;
#endif
        *config = loadedConfig;

#if ENABLE_BROWSER_UI
        SaveAppConfig(*config);
#endif
    }

    assert(config->reStreamers.size() == config->reStreamersOrder.size());

    return success;
}

}

int main(int argc, char *argv[])
{
#if ENABLE_BROWSER_UI
    http::Config httpConfig {
        .port = DEFAULT_HTTP_PORT,
        .realm = "VideoStreamer",
        .opaque = "VideoStreamer",
        .apiPrefix = rest::ApiPrefix,
    };

    signalling::Config wsConfig;
#endif

    Config config;

#ifdef SNAPCRAFT_BUILD
    const gchar* snapPath = g_getenv("SNAP");
    const gchar* snapName = g_getenv("SNAP_NAME");
    if(snapPath && snapName) {
        GCharPtr wwwRootPtr(g_build_path(G_DIR_SEPARATOR_S, snapPath, "opt", snapName, "www", NULL));
        httpConfig.wwwRoot = wwwRootPtr.get();
    }
#endif

    if(!LoadConfig(
#if ENABLE_BROWSER_UI
        &httpConfig,
        &wsConfig,
#endif
        &config))
    {
        return -1;
    }

#if ENABLE_GUI
    StartStreamerThread(
#   if ENABLE_BROWSER_UI
        httpConfig,
        wsConfig,
#   endif
        config,
        [] (const std::string& streamerId, NotificationType type) {
            PostNotification(streamerId, type);
        });
    const int result = GuiMain(argc, argv, &config);
    StopStreamerThread();
    return result;
#else
    return StreamerMain(
#   if ENABLE_BROWSER_UI
        httpConfig,
        wsConfig,
#   endif
        config);
#endif
}
