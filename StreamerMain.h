#pragma once

#include <glib.h>

#if ENABLE_BROWSER_UI
#include "WebRTSP/Http/Config.h"
#include "WebRTSP/Signalling/Config.h"
#endif

#include "Config.h"
#include "Types.h"

// should be thread safe
typedef std::function<void (const std::string& streamerId, NotificationType)> NotificationCallback;

int StreamerMain(
#if ENABLE_BROWSER_UI
    const http::Config&,
    const signalling::Config&,
#endif
    const Config&,
    const NotificationCallback& = NotificationCallback(),
    GMainContext* = nullptr);

void StartStreamerThread(
#if ENABLE_BROWSER_UI
    const http::Config&,
    const signalling::Config&,
#endif
    const Config&,
    const NotificationCallback&);
void StopStreamerThread();

void PostConfigChanges(std::unique_ptr<ConfigChanges>&&);
