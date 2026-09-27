#pragma once

#include <string>
#include <vector>

#include "midi/midi_controller.h"
#include "ui/notifications.h"
#include "vst/vst_host.h"

namespace imdj {

struct VstLibraryUiState {
    std::vector<VstPluginDescriptor> plugins;
    char extraDirBuffer[512] = "";
    std::string lastScanError;

    void rescan() { plugins = ScanVst3Plugins(extraDirBuffer); }
};

std::string AddPluginToChain(VstChain& chain, const VstPluginDescriptor& descriptor, bool startBypassed = false);

void DrawVstChainEditor(
    VstChain& chain, const VstLibraryUiState& library, Notifications& notifications, const char* chainLabel,
    MidiController& midi, FxChainTarget chainTarget, int deckCount
);

} // namespace imdj
