# naplxcontrol needs its own direct link to the rtmidi thirdparty lib (MidiHotplugMonitor
# constructs its own RtMidiIn to poll for available ports) - napmidi's own RtMidiIn usage
# isn't re-exported from napmidi.dll, and its MidiPortInfo polling helper has no NAPAPI export,
# so neither is usable from outside napmidi. Mirrors system_modules/napmidi/module_extra.cmake.
if(NOT TARGET rtmidi)
    list(APPEND CMAKE_MODULE_PATH "${NAP_ROOT}/system_modules/napmidi/thirdparty/cmake_find_modules")
    find_package(rtmidi REQUIRED)
endif()

target_include_directories(${PROJECT_NAME} PRIVATE ${RTMIDI_INCLUDE_DIR})
target_link_libraries(${PROJECT_NAME} debug ${RTMIDI_LIBRARIES_DEBUG} optimized ${RTMIDI_LIBRARIES_RELEASE})

if(WIN32)
    target_link_libraries(${PROJECT_NAME} winmm)
endif()
