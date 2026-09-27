cmake_minimum_required(VERSION 3.21)
project(imdj LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE "RelWithDebInfo" CACHE STRING "Build type" FORCE)
endif()

if(WIN32)
    add_compile_definitions(NOMINMAX WIN32_LEAN_AND_MEAN)
endif()

if(APPLE)
    enable_language(OBJC OBJCXX)
    add_compile_definitions(GL_SILENCE_DEPRECATION)
endif()

include(FetchContent)

set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG        3.4
    GIT_SHALLOW    TRUE
)

FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        master
    GIT_SHALLOW    TRUE
)

FetchContent_Declare(
    miniaudio
    GIT_REPOSITORY https://github.com/mackron/miniaudio.git
    GIT_TAG        master
    GIT_SHALLOW    TRUE
    SOURCE_SUBDIR  extras/do-not-build
)

FetchContent_Declare(
    stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG        master
    GIT_SHALLOW    TRUE
)

FetchContent_Declare(
    rtmidi
    GIT_REPOSITORY https://github.com/thestk/rtmidi.git
    GIT_TAG        master
    GIT_SHALLOW    TRUE
)
set(RTMIDI_BUILD_TESTING OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    bungee
    GIT_REPOSITORY https://github.com/bungee-audio-stretch/bungee.git
    GIT_TAG        main
    GIT_SHALLOW    TRUE
    GIT_SUBMODULES "submodules/eigen;submodules/pffft"
    SOURCE_SUBDIR  do-not-build
)

FetchContent_Declare(
    doctest
    GIT_REPOSITORY https://github.com/doctest/doctest.git
    GIT_TAG        v2.4.11
    GIT_SHALLOW    TRUE
    SOURCE_SUBDIR  do-not-build
)

FetchContent_Declare(
    taglib
    GIT_REPOSITORY https://github.com/taglib/taglib.git
    GIT_TAG        v2.1.1
    GIT_SHALLOW    TRUE
)
set(BUILD_BINDINGS OFF CACHE BOOL "" FORCE)
set(WITH_ZLIB OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    imgui_file_dialog
    GIT_REPOSITORY https://github.com/aiekick/ImGuiFileDialog.git
    GIT_TAG        d0e97b2adc3d3452d72c750c7305dc0291acd052
    SOURCE_SUBDIR  do-not-build
)

FetchContent_Declare(
    magic_enum
    GIT_REPOSITORY https://github.com/Neargye/magic_enum.git
    GIT_TAG        v0.9.8
    GIT_SHALLOW    TRUE
    SOURCE_SUBDIR  do-not-build
)

FetchContent_Declare(
    nlohmann_json
    URL https://github.com/nlohmann/json/releases/download/v3.11.3/include.zip
)

set(VST3_TREE_DIR ${CMAKE_BINARY_DIR}/_deps/vst3sdk_tree)

FetchContent_Declare(
    vst3_pluginterfaces
    GIT_REPOSITORY https://github.com/steinbergmedia/vst3_pluginterfaces.git
    GIT_TAG        master
    GIT_SHALLOW    TRUE
    SOURCE_DIR     ${VST3_TREE_DIR}/pluginterfaces
)
FetchContent_Declare(
    vst3_base
    GIT_REPOSITORY https://github.com/steinbergmedia/vst3_base.git
    GIT_TAG        master
    GIT_SHALLOW    TRUE
    SOURCE_DIR     ${VST3_TREE_DIR}/base
)
FetchContent_Declare(
    vst3_public_sdk
    GIT_REPOSITORY https://github.com/steinbergmedia/vst3_public_sdk.git
    GIT_TAG        master
    GIT_SHALLOW    TRUE
    SOURCE_DIR     ${VST3_TREE_DIR}/public.sdk
)

FetchContent_MakeAvailable(glfw imgui imgui_file_dialog miniaudio stb rtmidi taglib magic_enum nlohmann_json doctest bungee
                           vst3_pluginterfaces vst3_base vst3_public_sdk)

target_include_directories(tag SYSTEM INTERFACE $<BUILD_INTERFACE:${taglib_SOURCE_DIR}/taglib>
                                                $<BUILD_INTERFACE:${taglib_SOURCE_DIR}/taglib/toolkit>)

add_library(imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)
target_include_directories(imgui PUBLIC ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends)
target_link_libraries(imgui PUBLIC glfw)

add_library(imgui_file_dialog STATIC ${imgui_file_dialog_SOURCE_DIR}/ImGuiFileDialog.cpp)
target_include_directories(imgui_file_dialog PUBLIC ${imgui_file_dialog_SOURCE_DIR})
target_compile_definitions(imgui_file_dialog PUBLIC USE_STD_FILESYSTEM)
target_link_libraries(imgui_file_dialog PUBLIC imgui)

if(WIN32)
    target_link_libraries(imgui PUBLIC opengl32)
elseif(APPLE)
    find_library(COCOA_LIB Cocoa REQUIRED)
    find_library(OPENGL_LIB OpenGL REQUIRED)
    find_library(IOKIT_LIB IOKit REQUIRED)
    find_library(COREVIDEO_LIB CoreVideo REQUIRED)
    target_link_libraries(imgui PUBLIC ${OPENGL_LIB} ${COCOA_LIB} ${IOKIT_LIB} ${COREVIDEO_LIB})
else()
    find_package(OpenGL REQUIRED)
    target_link_libraries(imgui PUBLIC OpenGL::GL)
endif()

add_library(miniaudio INTERFACE)
target_include_directories(miniaudio INTERFACE ${miniaudio_SOURCE_DIR})

add_library(stb INTERFACE)
target_include_directories(stb INTERFACE ${stb_SOURCE_DIR})

add_library(json INTERFACE)
target_include_directories(json INTERFACE ${nlohmann_json_SOURCE_DIR}/include ${magic_enum_SOURCE_DIR}/include)
target_compile_definitions(json INTERFACE JSON_DISABLE_ENUM_SERIALIZATION=1)

add_library(doctest INTERFACE)
target_include_directories(doctest SYSTEM INTERFACE ${doctest_SOURCE_DIR})

file(GLOB BUNGEE_SOURCES ${bungee_SOURCE_DIR}/src/*.cpp)
add_library(bungee STATIC
    ${BUNGEE_SOURCES}
    ${bungee_SOURCE_DIR}/submodules/pffft/pffft.c
    ${bungee_SOURCE_DIR}/submodules/pffft/fftpack.c
)
target_include_directories(bungee
    PUBLIC ${bungee_SOURCE_DIR}
    PRIVATE ${bungee_SOURCE_DIR}/submodules ${bungee_SOURCE_DIR}/submodules/eigen
)
target_compile_features(bungee PUBLIC cxx_std_20)
target_compile_definitions(bungee PRIVATE
    BUNGEE_VISIBILITY=
    BUNGEE_SELF_TEST=0
    eigen_assert=BUNGEE_ASSERT1
    EIGEN_DONT_PARALLELIZE=1
)
set_source_files_properties(${bungee_SOURCE_DIR}/src/version.cpp PROPERTIES
    COMPILE_DEFINITIONS BUNGEE_VERSION="imdj"
)
if(MSVC)
    target_compile_options(bungee PRIVATE /wd4244 /wd4267 /wd4305 /wd4996)
    target_compile_definitions(bungee PRIVATE _CRT_SECURE_NO_WARNINGS _USE_MATH_DEFINES)
else()
    target_compile_options(bungee PRIVATE -fwrapv -w)
endif()

set(VST3_HOSTING_SOURCES
    ${vst3_pluginterfaces_SOURCE_DIR}/base/coreiids.cpp
    ${vst3_pluginterfaces_SOURCE_DIR}/base/funknown.cpp
    ${vst3_base_SOURCE_DIR}/source/baseiids.cpp
    ${vst3_base_SOURCE_DIR}/source/fbuffer.cpp
    ${vst3_base_SOURCE_DIR}/source/fdebug.cpp
    ${vst3_base_SOURCE_DIR}/source/fdynlib.cpp
    ${vst3_base_SOURCE_DIR}/source/fobject.cpp
    ${vst3_base_SOURCE_DIR}/source/fstreamer.cpp
    ${vst3_base_SOURCE_DIR}/source/fstring.cpp
    ${vst3_base_SOURCE_DIR}/source/timer.cpp
    ${vst3_base_SOURCE_DIR}/source/updatehandler.cpp
    ${vst3_public_sdk_SOURCE_DIR}/source/common/commoniids.cpp
    ${vst3_public_sdk_SOURCE_DIR}/source/common/commonstringconvert.cpp
    ${vst3_public_sdk_SOURCE_DIR}/source/common/memorystream.cpp
    ${vst3_public_sdk_SOURCE_DIR}/source/vst/vstinitiids.cpp
    ${vst3_public_sdk_SOURCE_DIR}/source/vst/utility/stringconvert.cpp
    ${vst3_public_sdk_SOURCE_DIR}/source/vst/hosting/connectionproxy.cpp
    ${vst3_public_sdk_SOURCE_DIR}/source/vst/hosting/eventlist.cpp
    ${vst3_public_sdk_SOURCE_DIR}/source/vst/hosting/hostclasses.cpp
    ${vst3_public_sdk_SOURCE_DIR}/source/vst/hosting/module.cpp
    ${vst3_public_sdk_SOURCE_DIR}/source/vst/hosting/parameterchanges.cpp
    ${vst3_public_sdk_SOURCE_DIR}/source/vst/hosting/pluginterfacesupport.cpp
    ${vst3_public_sdk_SOURCE_DIR}/source/vst/hosting/processdata.cpp
)
if(WIN32)
    list(APPEND VST3_HOSTING_SOURCES ${vst3_public_sdk_SOURCE_DIR}/source/vst/hosting/module_win32.cpp)
elseif(APPLE)
    list(APPEND VST3_HOSTING_SOURCES ${vst3_public_sdk_SOURCE_DIR}/source/vst/hosting/module_mac.mm)
    set_source_files_properties(
        ${vst3_public_sdk_SOURCE_DIR}/source/vst/hosting/module_mac.mm PROPERTIES COMPILE_OPTIONS -fobjc-arc
    )
else()
    list(APPEND VST3_HOSTING_SOURCES ${vst3_public_sdk_SOURCE_DIR}/source/vst/hosting/module_linux.cpp)
endif()

add_library(vst3_hosting STATIC ${VST3_HOSTING_SOURCES})
target_include_directories(vst3_hosting PUBLIC ${VST3_TREE_DIR})

set_target_properties(vst3_hosting PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
if(MSVC)
    target_compile_definitions(vst3_hosting PRIVATE _CRT_SECURE_NO_WARNINGS)
else()
    target_compile_options(vst3_hosting PRIVATE -w)
endif()

if(WIN32)
    target_link_libraries(vst3_hosting PUBLIC shell32 ole32)
elseif(APPLE)
    find_library(FOUNDATION_LIB Foundation REQUIRED)
    find_library(APPKIT_LIB AppKit REQUIRED)
    target_link_libraries(vst3_hosting PUBLIC ${FOUNDATION_LIB} ${APPKIT_LIB})
else()
    find_package(Threads REQUIRED)
    target_link_libraries(vst3_hosting PUBLIC Threads::Threads ${CMAKE_DL_LIBS})
endif()

add_executable(imdj
    src/main.cpp
    src/app/app_paths.cpp
    src/app/app_settings.cpp
    src/app/app_shell.cpp
    src/app/session_state.cpp
    src/audio/audio_engine_controls.cpp
    src/audio/audio_engine_device.cpp
    src/audio/miniaudio_impl.cpp
    src/audio/audio_engine_io.cpp
    src/audio/audio_engine_render.cpp
    src/audio/audio_engine_transport.cpp
    src/audio/deck.cpp
    src/audio/deck_processors.cpp
    src/audio/loudness.cpp
    src/audio/metronome.cpp
    src/audio/pitch_stretcher.cpp
    src/audio/preview.cpp
    src/audio/sample_buffer.cpp
    src/audio/sample_player.cpp
    src/audio/track_loader.cpp
    src/audio/track_metadata.cpp
    src/audio/waveform.cpp
    src/core/crash_guard.cpp
    src/core/json_io.cpp
    src/core/strings.cpp
    src/library/audio_tags.cpp
    src/library/session_preset.cpp
    src/library/song_library.cpp
    src/midi/jog_tracker.cpp
    src/midi/midi_bindings_json.cpp
    src/midi/midi_control_table.cpp
    src/midi/midi_controller.cpp
    src/ui/app_settings_ui.cpp
    src/ui/app_ui_settings.cpp
    src/ui/bindings_ui.cpp
    src/ui/dancer.cpp
    src/ui/deck_panel.cpp
    src/ui/library_panel.cpp
    src/ui/midi_controller_ui.cpp
    src/ui/mixer_panel.cpp
    src/ui/session_preset_ui.cpp
    src/ui/signal_chain_ui.cpp
    src/ui/modal.cpp
    src/ui/notifications.cpp
    src/ui/theme.cpp
    src/ui/transport_panel.cpp
    src/ui/ui_common.cpp
    src/ui/vst_library_ui.cpp
    src/vst/vst_chain.cpp
    src/vst/vst_host.cpp
    src/vst/vst_host_glfw.cpp
    src/vst/vst_host_win32.cpp
    src/vst/vst_scan.cpp
)
target_include_directories(imdj PRIVATE src)
target_link_libraries(imdj PRIVATE imgui imgui_file_dialog miniaudio stb json bungee vst3_hosting rtmidi tag)

if(UNIX AND NOT APPLE)
    find_package(Threads REQUIRED)
    target_link_libraries(imdj PRIVATE Threads::Threads dl m)
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9.0)
    target_link_libraries(imdj PRIVATE stdc++fs)
endif()

set_target_properties(imdj PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

if(WIN32)
    set_target_properties(imdj PROPERTIES WIN32_EXECUTABLE $<NOT:$<CONFIG:Debug>>)
    if(MSVC)
        target_link_options(imdj PRIVATE /ENTRY:mainCRTStartup)
    endif()
endif()

if(MSVC)
    target_compile_options(imdj PRIVATE /W4 /EHsc)
    target_compile_definitions(imdj PRIVATE _CRT_SECURE_NO_WARNINGS)
else()
    target_compile_options(imdj PRIVATE -Wall -Wextra)
endif()

if(MSVC)
    set(IMDJ_VENDOR_WARNING_FLAGS /wd4244 /wd4267 /wd4305 /wd4996)
else()
    set(IMDJ_VENDOR_WARNING_FLAGS -w)
endif()

set_source_files_properties(src/audio/miniaudio_impl.cpp tests/miniaudio_impl.cpp
    PROPERTIES COMPILE_OPTIONS "${IMDJ_VENDOR_WARNING_FLAGS}")

option(IMDJ_BUILD_TESTS "Build the unit tests" ON)

if(IMDJ_BUILD_TESTS)
    enable_testing()

    add_executable(imdj_tests
        tests/miniaudio_impl.cpp
        tests/test_dsp.cpp
        tests/test_files.cpp
        tests/test_midi.cpp
        tests/test_strings.cpp
        src/audio/deck.cpp
        src/audio/loudness.cpp
        src/audio/metronome.cpp
        src/audio/sample_buffer.cpp
        src/audio/sample_player.cpp
        src/audio/track_metadata.cpp
        src/audio/waveform.cpp
        src/core/json_io.cpp
        src/core/strings.cpp
        src/library/audio_tags.cpp
        src/midi/midi_control_table.cpp
    )
    target_include_directories(imdj_tests PRIVATE src)
    target_compile_definitions(imdj_tests PRIVATE IMDJ_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
    target_link_libraries(imdj_tests PRIVATE miniaudio json doctest tag)

    if(MSVC)
        target_compile_options(imdj_tests PRIVATE /W4 /EHsc /external:W0)
        target_compile_definitions(imdj_tests PRIVATE _CRT_SECURE_NO_WARNINGS)
    else()
        target_compile_options(imdj_tests PRIVATE -Wall -Wextra)
    endif()

    set_target_properties(imdj_tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
    add_test(NAME imdj_tests COMMAND imdj_tests)
endif()

install(TARGETS imdj RUNTIME DESTINATION . COMPONENT app)
install(DIRECTORY res/ DESTINATION . COMPONENT app)
install(FILES README.md LICENSE DESTINATION . COMPONENT app)

set(CPACK_GENERATOR ZIP)
set(CPACK_PACKAGE_NAME imdj)
set(CPACK_COMPONENTS_ALL app)
set(CPACK_ARCHIVE_COMPONENT_INSTALL ON)
set(CPACK_COMPONENTS_GROUPING ALL_COMPONENTS_IN_ONE)
set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY ON)
set(CPACK_COMPONENT_INCLUDE_TOPLEVEL_DIRECTORY ON)
include(CPack)
