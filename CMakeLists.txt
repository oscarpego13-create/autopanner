cmake_minimum_required(VERSION 3.14)
project(NoisePanner VERSION 1.0.0)

# iPlug2 root — set via -DIPLUG2_DIR=... or defaults to ../../ (when inside Examples/)
if(NOT DEFINED IPLUG2_DIR)
  set(IPLUG2_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../.." CACHE PATH "iPlug2 root directory")
endif()

include(${IPLUG2_DIR}/iPlug2.cmake)
find_package(iPlug2 REQUIRED)

iplug_add_plugin(${PROJECT_NAME}
  FORMATS VST3 AU
  SOURCES
    AutoPanner.cpp
    AutoPanner.h
    resources/resource.h
  RESOURCES
    resources/fonts/Roboto-Regular.ttf
    resources/NoisePanner-VST3-Info.plist
    resources/NoisePanner-AU-Info.plist
)
