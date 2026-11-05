cmake_minimum_required(VERSION 3.25)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
project(EqTune VERSION 0.2.8 LANGUAGES CXX)

include(FetchContent)
set(SMTG_ENABLE_VSTGUI_SUPPORT OFF CACHE BOOL "" FORCE)
set(SMTG_ENABLE_VST3_PLUGIN_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SMTG_ENABLE_VST3_HOSTING_EXAMPLES ON CACHE BOOL "" FORCE)
set(SMTG_RUN_VST_VALIDATOR ON CACHE BOOL "" FORCE)
set(SMTG_CREATE_PLUGIN_LINK OFF CACHE BOOL "" FORCE)
set(SMTG_USE_STATIC_CRT ON CACHE BOOL "" FORCE)
FetchContent_Declare(vst3sdk
    GIT_REPOSITORY https://github.com/steinbergmedia/vst3sdk.git
    GIT_TAG v3.8.0_build_66
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(vst3sdk)
set(public_sdk_SOURCE_DIR "${vst3sdk_SOURCE_DIR}/public.sdk")

smtg_add_vst3plugin(EqTune
    Source/Plugin.cpp
    Source/Dsp.h
    Source/HeadphoneProfiles.h
    Source/Editor.cpp
    Source/Editor.h
)
target_compile_features(EqTune PRIVATE cxx_std_17)
target_compile_definitions(EqTune PRIVATE UNICODE _UNICODE NOMINMAX)
target_link_libraries(EqTune PRIVATE sdk comctl32)
smtg_target_configure_version_file(EqTune)
smtg_target_add_plugin_resources(EqTune RESOURCES Resources/WarzoneAudioSetup.ps1)

enable_testing()
add_executable(EqTuneDspTests Tests/DspTests.cpp)
target_compile_features(EqTuneDspTests PRIVATE cxx_std_17)
add_test(NAME EqTuneDspTests COMMAND EqTuneDspTests)

add_executable(EqTuneHostTests Tests/HostTests.cpp
    "${vst3sdk_SOURCE_DIR}/public.sdk/source/common/memorystream.cpp")
target_compile_features(EqTuneHostTests PRIVATE cxx_std_17)
target_compile_definitions(EqTuneHostTests PRIVATE UNICODE _UNICODE NOMINMAX)
target_link_libraries(EqTuneHostTests PRIVATE sdk sdk_hosting comctl32)
add_dependencies(EqTuneHostTests EqTune)
add_test(NAME EqTuneHostTests COMMAND EqTuneHostTests "$<TARGET_FILE:EqTune>")
