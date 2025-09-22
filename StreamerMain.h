#pragma once

#if ENABLE_BROWSER_UI
#include "WebRTSP/Http/Config.h"
#include "WebRTSP/Signalling/Config.h"
#endif

#include "Config.h"


int StreamerMain(
#if ENABLE_BROWSER_UI
    const http::Config&,
    const signalling::Config&,
#endif
    const Config&);
