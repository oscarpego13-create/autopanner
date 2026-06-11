#pragma once
#include "IPlugAPIBase.h"

#define PLUG_NAME "AutoPanner"
#define PLUG_MFR  "AudioDev"
#define PLUG_VERSION_HEX 0x00010000
#define PLUG_VERSION_STR "1.0.0"
#define PLUG_UNIQUE_ID 'APnr'
#define PLUG_MFR_ID    'AuDv'
#define PLUG_URL_STR   ""
#define PLUG_EMAIL_STR ""
#define PLUG_COPYRIGHT_STR "Copyright 2024"
#define PLUG_CLASS_NAME AutoPanner

// Channel I/O: stereo in → stereo out
#define PLUG_CHANNEL_IO "2-2"

#define PLUG_LATENCY        0
#define PLUG_TYPE           1   // 1 = effect
#define PLUG_DOES_MIDI_IN   0
#define PLUG_DOES_MIDI_OUT  0
#define PLUG_DOES_MPE       0
#define PLUG_DOES_STATE_CHUNKS 0
#define PLUG_HAS_UI         1
#define PLUG_WIDTH          480
#define PLUG_HEIGHT         280
#define PLUG_FPS            60
#define PLUG_SHARED_RESOURCES 0
#define PLUG_HOST_RESIZE    0

#define AUV2_ENTRY         AutoPanner_Entry
#define AUV2_ENTRY_STR     "AutoPanner_Entry"
#define AUV2_FACTORY       AutoPanner_Factory
#define AUV2_FACTORY_STR   "AutoPanner_Factory"
#define AUV2_VIEW_CLASS    AutoPannerView
#define AUV2_VIEW_CLASS_STR "AutoPannerView"

// MIDI / Sysex (unused)
#define PLUG_MIN_HEIGHT PLUG_HEIGHT
#define PLUG_MIN_WIDTH  PLUG_WIDTH
