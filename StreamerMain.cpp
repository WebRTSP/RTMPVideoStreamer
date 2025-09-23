#include "StreamerMain.h"

#include <string>
#include <deque>
#include <optional>

#include <gst/gst.h>

#include "CxxPtr/GlibPtr.h"

#if ENABLE_BROWSER_UI
#include "WebRTSP/Http/Config.h"
#include "WebRTSP/Http/HttpMicroServer.h"
#include "WebRTSP/Signalling/Config.h"
#include "WebRTSP/Signalling/WsServer.h"
#include "WebRTSP/Signalling/ServerSession.h"
#include "WebRTSP/RtStreaming/GstRtStreaming/GstReStreamer2.h"
#endif

#include "Log.h"
#include "Defines.h"
#include "Config.h"
#include "ReStreamer.h"

#if ENABLE_SSDP
#include "SSDP.h"
#endif

#if ENABLE_BROWSER_UI
#include "RestApi.h"
#endif


namespace {

enum {
    RECONNECT_INTERVAL = 5,
};

const auto Log = ReStreamerLog;

typedef std::map<std::string, ReStreamer> RTMPReStreamers;
#if ENABLE_BROWSER_UI
typedef std::map<std::string, std::unique_ptr<GstStreamingSource>> ReStreamers;
#endif
struct Context {
    Config config;
#if ENABLE_BROWSER_UI
    ReStreamers reStreamers;
#endif
    RTMPReStreamers rtmpReStreamers;
    std::map<std::string, GSourcePtr> restarting; // reStreamerId -> timer GSource*
};
thread_local Context* streamContext = nullptr;

void AddIdle(
    GSourceFunc function,
    gpointer data,
    GDestroyNotify notify)
{
    GSource* source = g_idle_source_new();
    g_source_set_callback(
        source,
        function,
        data,
        notify);
    g_source_attach(source, g_main_context_get_thread_default());
    g_source_unref(source);
}

GSource* addSecondsTimeout(
    guint interval,
    GSourceFunc function,
    gpointer data,
    GDestroyNotify notify)
{
    GSource* source = g_timeout_source_new_seconds(interval);
    g_source_set_callback(
        source,
        function,
        data,
        notify);
    g_source_attach(source, g_main_context_get_thread_default());

    return source;
}

void StopReStream(Context* context, const std::string& reStreamerId)
{
    auto restartingIt = context->restarting.find(reStreamerId);
    if(restartingIt != context->restarting.end()) {
        Log()->info("Cancelling pending reStreaming restart for \"{}\"...", reStreamerId);
        g_source_destroy(restartingIt->second.get());
        context->restarting.erase(restartingIt);
    }

    RTMPReStreamers* reStreamers = &(context->rtmpReStreamers);
    const auto& it = reStreamers->find(reStreamerId);
    if(it != reStreamers->end()) {
        Log()->info("Stopping active reStreaming \"{}\" (\"{}\")...", it->second.sourceUrl(), reStreamerId);
        reStreamers->erase(it);
    }
}

void ScheduleStartReStream(Context* context, const std::string& reStreamerId);

void StartReStream(
    Context* context,
    const std::string& reStreamerId)
{
    assert(context == ::streamContext);

    const Config& config = context->config;
    RTMPReStreamers* reStreamers = &(context->rtmpReStreamers);

    assert(reStreamers->find(reStreamerId) == reStreamers->end());
    StopReStream(context, reStreamerId);

    const auto configIt = config.reStreamers.find(reStreamerId);
    if(configIt == config.reStreamers.end()) {
        Log()->error("Can't find reStreamer with id \"{}\"", reStreamerId);
        return;
    }

    const Config::ReStreamer& reStreamerConfig = configIt->second;

    const auto reStreamerIt = reStreamers->find(reStreamerId);

    if(reStreamerConfig.enabled) {
        if(reStreamerIt == reStreamers->end()) {
            Log()->info("ReStreaming \"{}\" (\"{}\")", reStreamerConfig.sourceUrl, reStreamerId);
        } else {
            Log()->warn(
                "Ignoring reStreaming request for already reStreaming source \"{}\" (\"{}\")...",
                reStreamerConfig.sourceUrl,
                reStreamerId);
        }
    } else {
        Log()->debug(
            "Ignoring reStreaming request for disabled source \"{}\" (\"{}\")...",
            reStreamerConfig.sourceUrl,
            reStreamerId);
        assert(reStreamerIt == reStreamers->end());
        return;
    }

    auto [it, inserted] = reStreamers->emplace(
        std::piecewise_construct,
        std::forward_as_tuple(reStreamerId),
        std::forward_as_tuple(
            reStreamerConfig.sourceUrl,
            reStreamerConfig.targetUrl,
            [context, reStreamerId] () {
                // it's required to do reStreamerId copy
                // since ReStreamer instance
                // will be destroyed inside ScheduleStartReStream
                // and as consequence current lambda with all captures
                // will be destroyed too
                ScheduleStartReStream(context, std::string(reStreamerId));
            }
        ));
    assert(inserted);

    it->second.start();
}

void ScheduleStartReStream(
    Context* context,
    const std::string& reStreamerId)
{
    assert(context == ::streamContext);

    if(context->restarting.find(reStreamerId) != context->restarting.end()) {
        Log()->debug("ReStreamer restart already pending. Ignoring new request...");
        return;
    }

    Log()->info("ReStreaming restart pending...");

    const Config& config = context->config;
    RTMPReStreamers* reStreamers = &(context->rtmpReStreamers);

    assert(reStreamers->find(reStreamerId) != reStreamers->end());
    StopReStream(context, reStreamerId);

    typedef std::tuple<
        Context*,
        std::string> Data;

    auto reconnect =
        [] (gpointer userData) -> gboolean {
            const auto& [context, reStreamerId] = *reinterpret_cast<Data*>(userData);

            context->restarting.erase(reStreamerId);

            StartReStream(context, reStreamerId);

            return false;
        };

    GSource* timeoutSource = addSecondsTimeout(
        RECONNECT_INTERVAL,
        GSourceFunc(reconnect),
        new Data(context, reStreamerId),
        [] (gpointer userData) {
            delete reinterpret_cast<Data*>(userData);
        });

    context->restarting.emplace(reStreamerId, timeoutSource);
}

#if ENABLE_BROWSER_UI
static std::unique_ptr<WebRTCPeer> CreateWebRTCPeer(
    const ReStreamers& reStreamers,
    const std::string& uri) noexcept
{
    auto streamerIt = reStreamers.find(uri);
    if(streamerIt != reStreamers.end()) {
        return streamerIt->second->createPeer();
    } else
        return nullptr;
}

std::unique_ptr<ServerSession> CreateWebRTSPSession(
    const WebRTCConfigPtr& webRTCConfig,
    const ReStreamers& reStreamers,
    const rtsp::Session::SendRequest& sendRequest,
    const rtsp::Session::SendResponse& sendResponse) noexcept
{
    return
        std::make_unique<ServerSession>(
            webRTCConfig,
            std::bind(CreateWebRTCPeer, std::ref(reStreamers), std::placeholders::_1),
            sendRequest,
            sendResponse);
}

void ConfigChanged(Context* context, const std::unique_ptr<ConfigChanges>& changes)
{
    Config& config = context->config;

    const auto& reStreamersChanges = changes->reStreamersChanges;
    for(const auto& pair: reStreamersChanges) {
        const std::string& uniqueId = pair.first;
        const ConfigChanges::ReStreamerChanges& reStreamerChanges = pair.second;

        const auto& it = config.reStreamers.find(uniqueId);
        if(it == config.reStreamers.end()) {
            Log()->warn("Got change request for unknown reStreamer \"{}\"", uniqueId);
            return;
        }

        Config::ReStreamer& reStreamerConfig = it->second;

        if(reStreamerChanges.enabled) {
            if(reStreamerConfig.enabled != *reStreamerChanges.enabled) {
                reStreamerConfig.enabled = *reStreamerChanges.enabled;
                if(reStreamerConfig.enabled) {
                    StartReStream(context, uniqueId);
                } else {
                    StopReStream(context, uniqueId);
                }
            }
        }
    }

    // FIXME? add config save to disk
}

void PostConfigChanges(std::unique_ptr<ConfigChanges>&& changes)
{
    typedef std::tuple<std::unique_ptr<ConfigChanges>> Data;

    AddIdle(
        [] (gpointer userData) -> gboolean {
            Data& data = *static_cast<Data*>(userData);
            assert(::streamContext);
            if(::streamContext) {
                ConfigChanged(::streamContext, std::get<0>(data));
            }
            return G_SOURCE_REMOVE;
        },
        new Data(std::move(changes)),
        [] (gpointer userData) {
            delete static_cast<Data*>(userData);
        });
}
#endif

}

int StreamerMain(
#if ENABLE_BROWSER_UI
    const http::Config& httpConfig,
    const signalling::Config& wsConfig,
#endif
    const Config& config)
{
    Context context { config };
    ::streamContext = &context;

    gst_init(nullptr, nullptr);

    GMainContext* mainContext = g_main_context_new();
    g_main_context_push_thread_default(mainContext);
    GMainLoopPtr loopPtr(g_main_loop_new(mainContext, FALSE));
    GMainLoop* loop = loopPtr.get();

    for(const auto& pair: context.config.reStreamers) {
        const std::string& uniqueId = pair.first;

#if ENABLE_BROWSER_UI
        const Config::ReStreamer& reStreamer = pair.second;
        context.reStreamers.emplace(
            reStreamer.sourceUrl,
            std::make_unique<GstReStreamer2>(
                reStreamer.sourceUrl,
                reStreamer.forceH264ProfileLevelId));
#endif

        StartReStream(&context, uniqueId);
    }

#if ENABLE_BROWSER_UI
    std::unique_ptr<http::MicroServer> httpServerPtr;
    if(httpConfig.port) {
        std::string configJs =
            fmt::format(
                "const APIPort = {};\r\n"
                "const WebRTSPPort = {};\r\n",
                httpConfig.port,
                wsConfig.port);
        httpServerPtr =
            std::make_unique<http::MicroServer>(
                httpConfig,
                configJs,
                http::MicroServer::OnNewAuthToken(),
                std::bind(
                    &rest::HandleRequest,
                    std::make_shared<Config>(context.config),
                    [] (std::unique_ptr<ConfigChanges>&& changes) {
                        PostConfigChanges(std::move(changes));
                    },
                    std::placeholders::_1,
                    std::placeholders::_2,
                    std::placeholders::_3),
                nullptr);
        httpServerPtr->init();
    }

    std::unique_ptr<signalling::WsServer> wsServerPtr;
    if(wsConfig.port) {
        wsServerPtr = std::make_unique<signalling::WsServer>(
            wsConfig,
            loop,
            std::bind(
                CreateWebRTSPSession,
                std::make_shared<WebRTCConfig>(),
                std::ref(context.reStreamers),
                std::placeholders::_1,
                std::placeholders::_2));
        wsServerPtr->init();
    }
#endif

#if ENABLE_SSDP
    SSDPContext ssdpContext;

#ifdef SNAPCRAFT_BUILD
    const gchar* snapData = g_getenv("SNAP_DATA");
    g_autofree gchar* deviceUuidFilePath = nullptr;
    if(snapData) {
        deviceUuidFilePath =
            g_build_path(G_DIR_SEPARATOR_S, snapData, DEVICE_UUID_FILE_NAME, NULL);
        g_autofree gchar* deviceUuid = nullptr;
        if(g_file_get_contents(deviceUuidFilePath, &deviceUuid, nullptr, nullptr) &&
            g_uuid_string_is_valid(deviceUuid))
        {
            ssdpContext.deviceUuid = deviceUuid;
        }
    }
    const bool hadDeviceUuid = ssdpContext.deviceUuid.has_value();
#endif

    SSDPPublish(&ssdpContext);

#ifdef SNAPCRAFT_BUILD
    if(!hadDeviceUuid && ssdpContext.deviceUuid.has_value() && deviceUuidFilePath) {
        if(!g_file_set_contents_full(
            deviceUuidFilePath,
            ssdpContext.deviceUuid.value().c_str(),
            -1,
            GFileSetContentsFlags(G_FILE_SET_CONTENTS_CONSISTENT | G_FILE_SET_CONTENTS_ONLY_EXISTING),
            0644,
            nullptr))
        {
            Log()->warn("Failed to save device uuid to \"{}\"", deviceUuidFilePath);
        }
    }
#endif
#endif

    g_main_loop_run(loop);

    g_main_context_pop_thread_default(mainContext);
    g_main_context_unref(mainContext);

    ::streamContext = nullptr;

    return 0;
}
