#pragma once

#include <memory>
#include <string>
#include <functional>

#include <CxxPtr/GstPtr.h>

class ReStreamer
{
public:
    enum class EosReason {
        Disconnect,
        RtspSourceError,
        RtmpTargetError,
        OtherError,
    };
    typedef std::function<void (EosReason reason)> EosCallback;

    ReStreamer(
        const std::string& sourceUrl,
        const std::string& targetUrl,
        const EosCallback& onEos);
    ~ReStreamer();

    const std::string& sourceUrl() const { return _sourceUrl; };

    void start() noexcept;

private:
    void setState(GstState) noexcept;
    void pause() noexcept;
    void play() noexcept;
    void stop() noexcept;

    gboolean onBusMessage(GstMessage*);

    void unknownType(
        GstElement* decodebin,
        GstPad*,
        GstCaps*);
    void srcPadAdded(GstElement* decodebin, GstPad*);
    void noMorePads(GstElement* decodebin);

    static void postEos(
        GstElement* rtcbin,
        gboolean error);

    void onEos(EosReason);

private:
    EosCallback _onEos;

    const std::string _sourceUrl;
    const std::string _targetUrl;

    GType _rtspSrcType = 0;
    GType _rtmpSinkType = 0;

    GstElementPtr _pipelinePtr;
    GstElementPtr _flvMuxPtr;
    GstPadPtr _flvVideoSinkPad;
    GstPadPtr _flvAudioSinkPad;

    GstCapsPtr _h264CapsPtr;
    GstCapsPtr _audioRawCapsPtr;

    bool _videoLinked = false;
    bool _audioLinked = false;
};
