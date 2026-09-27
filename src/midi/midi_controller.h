#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "audio/audio_engine.h"
#include "midi/jog_tracker.h"
#include "midi/midi_types.h"

namespace imdj {

class MidiController {
public:
    MidiController();
    ~MidiController();
    MidiController(const MidiController&) = delete;
    MidiController& operator=(const MidiController&) = delete;

    std::vector<std::string> listPorts() const;
    Status connect(int portIndex, AudioEngine* engine);
    void disconnect();
    bool isConnected() const { return connected_; }
    const std::string& connectedPortName() const { return connectedPortName_; }

    bool autoConnectLastDevice(AudioEngine* engine);

    void armLearn(MidiControl control);
    void cancelLearn();
    bool isLearning() const;
    MidiControl learningControl() const;

    MidiBinding binding(MidiControl control) const;
    void clearBinding(MidiControl control);

    void assignFxKnobVstParam(
        int deckIndex, int knobIndex, FxChainTarget chain, size_t pluginIndex, int32_t paramId, std::string paramLabel
    );
    void assignFxKnobBuiltin(int deckIndex, int knobIndex, KnobTargetKind kind);
    void clearFxKnobAssignment(int deckIndex, int knobIndex);
    FxKnobAssignment fxKnobAssignment(int deckIndex, int knobIndex) const;

    void assignPad(int deckIndex, int padIndex, PadAction action);
    void clearPadAssignment(int deckIndex, int padIndex);
    PadAction padAssignment(int deckIndex, int padIndex) const;

    void swapPluginAssignments(FxChainTarget chain, size_t indexA, size_t indexB);
    void removePluginAssignments(FxChainTarget chain, size_t removedIndex);

    struct LastMessage {
        bool valid = false;
        uint8_t statusByte = 0;
        uint8_t data1 = 0;
        uint8_t data2 = 0;
        size_t byteCount = 0;
        bool matchedControl = false;
        std::string matchedLabel;
    };
    LastMessage lastMessage() const;

    void update();

    bool loadBindings(const std::string& deviceName);
    Status saveBindings() const;

    struct BindingSnapshot {
        std::array<MidiBinding, static_cast<size_t>(MidiControl::Count)> bindings{};
        std::array<std::array<FxKnobAssignment, FX_KNOBS_PER_DECK>, MAX_DECK_COUNT> fxKnobAssignments{};
        std::array<std::array<PadAction, PADS_PER_DECK>, MAX_DECK_COUNT> padAssignments{};
    };
    BindingSnapshot exportBindings() const;
    void importBindings(const BindingSnapshot& snapshot);

private:
    static void RtMidiCallbackThunk(double timeStamp, std::vector<unsigned char>* message, void* userData);
    void handleMessage(double timeStamp, const std::vector<unsigned char>& message);
    struct ControlEvent {
        int deckIndex = 0;
        int slot = -1;
        bool pressed = false;
        bool continuous = false;
        float value = 0.0f;
        uint8_t raw = 0;
    };

    void dispatch(MidiControl control, uint8_t statusHighNibble, uint8_t data2);
    void apply(ActionKind action, const ControlEvent& event);
    void applyFxKnob(int deckIndex, int knobIndex, float value);
    void applyPad(int deckIndex, int padIndex);
    void releaseScratch(int deckIndex);
    VstChain* chainForTarget(FxChainTarget target) const;

    struct Impl;
    std::unique_ptr<Impl> impl_;

    AudioEngine* engine_ = nullptr;
    bool connected_ = false;
    std::string connectedPortName_;

    mutable std::mutex bindingsMutex_;
    BindingSnapshot state_;
    MidiControl learnArmedControl_ = MidiControl::None;
    LastMessage lastMessage_;

    JogTracker jog_[MAX_DECK_COUNT];
};

} // namespace imdj
