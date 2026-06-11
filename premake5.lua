--[[
  AutoPanner – iPlug2 premake5 build script
  Place this project inside iPlug2/Examples/AutoPanner/
  and run: premake5 xcode4   (macOS)
--]]

-- ────────── plugin metadata ──────────
local PLUG_NAME          = "AutoPanner"
local PLUG_MFR           = "AudioDev"
local PLUG_VERSION_HEX   = "0x00010000"
local PLUG_UNIQUE_ID     = "APnr"
local PLUG_MFR_ID        = "AuDv"
local PLUG_URL_STR       = ""
local PLUG_EMAIL_STR     = ""
local PLUG_COPYRIGHT_STR = "Copyright 2024"
local BUNDLE_NAME        = "AutoPanner"
local BUNDLE_MFR         = "AudioDev"
local BUNDLE_DOMAIN      = "com.audiodev"

-- ────────── paths ──────────
IPLUG2_ROOT          = "../../"
local SCRIPTS_ROOT   = path.join(IPLUG2_ROOT, "Scripts")
local PLUG_RES_PATH  = "resources/"
local PLUG_EXTRAS    = {}   -- extra source files outside the plug folder

-- ────────── formats to build ──────────
-- On macOS we build VST3 + AU + standalone App
local formats = { "vst3", "au", "app" }

-- ────────── common iPlug2 configuration ──────────
dofile(path.join(SCRIPTS_ROOT, "IPlug_common.lua"))

workspace (PLUG_NAME)
  configurations { "Debug", "Release" }
  platforms      { "x64" }
  location       ("build")

  -- C++17 required for structured bindings / std::clamp etc.
  cppdialect "C++17"

  filter "configurations:Debug"
    defines  { "DEBUG", "_DEBUG" }
    symbols  "On"
    optimize "Off"

  filter "configurations:Release"
    defines  { "NDEBUG" }
    optimize "Speed"
    flags    { "LinkTimeOptimization" }

  filter {}

-- ────────── per-format sub-projects ──────────
for _, fmt in ipairs(formats) do
  -- Each IPlug_<fmt>.lua creates a project() inside this workspace
  include(path.join(SCRIPTS_ROOT, "IPlug_" .. fmt .. ".lua"))
end
