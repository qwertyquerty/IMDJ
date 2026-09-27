#include "midi/midi_controller.h"

#include <algorithm>
#include <chrono>
#include <optional>

#include "RtMidi.h"
#include "app/app_paths.h"
#include "app/session_state.h"
#include "core/text.h"
#include "midi/midi_bindings_json.h"
#include "midi/midi_control_table.h"
#include "core/strings.h"

namespace imdj {

namespace {

constexpr float TEMPO_FADER_RANGE_PERCENT = 0.20f;
constexpr const char* LAST_DEVICE_KEY = "midi_device";
constexpr int MIDI_BINDINGS_FILE_VERSION = 1;

double NowSeconds()
{
    using namespace std::chrono;
    return duration<double>(steady_clock::now().time_since_epoch()).count();
}

float TempoFaderToRate(uint8_t ccValue)
{
    float t = std::clamp((static_cast<float>(ccValue) - 64.0f) / 63.0f, -1.0f, 1.0f);
    float pct = t * TEMPO_FADER_RANGE_PERCENT;

    return std::clamp(1.0f + pct, 0.25f, 4.0f);
}

std::string MidiMappingFilePath(const std::string& deviceName)
{
    return AppDataDir() + "/midi_" + SanitizeFileName(deviceName) + ".json";
}

template <typename Table>
auto Slot(Table& table, int deckIndex, int index) -> decltype(&table[0][0])
{
    const bool valid = deckIndex >= 0 && deckIndex < static_cast<int>(table.size()) && index >= 0 &&
                       index < static_cast<int>(table[0].size());

    return valid ? &table[deckIndex][index] : nullptr;
}

template <typename Fn>
void ForEachPluginReference(MidiController::BindingSnapshot& state, FxChainTarget chain, Fn fn)
{
    for (auto& knobs : state.fxKnobAssignments) {
        for (FxKnobAssignment& knob : knobs) {
            if (knob.assigned && knob.chain == chain) {
                fn(knob);
            }
        }
    }

    for (auto& pads : state.padAssignments) {
        for (PadAction& pad : pads) {
            if (pad.assigned && pad.kind == PadActionKind::TogglePluginBypass && pad.chain == chain) {
                fn(pad);
            }
        }
    }
}

} // namespace

struct MidiController::Impl {
    RtMidiIn midiIn;
};

MidiController::MidiController() { ControlInfoFor(MidiControl::DeckA_Play); }
MidiController::~MidiController() { disconnect(); }

std::vector<std::string> MidiController::listPorts() const
{
    std::vector<std::string> result;
    try {
        RtMidiIn probe;
        for (unsigned int i = 0; i < probe.getPortCount(); ++i) {
            result.push_back(probe.getPortName(i));
        }
    }
    catch (const RtMidiError&) {
    }

    return result;
}

Status MidiController::connect(int portIndex, AudioEngine* engine)
{
    disconnect();
    impl_ = std::make_unique<Impl>();
    try {
        impl_->midiIn.openPort(static_cast<unsigned int>(portIndex));
        impl_->midiIn.ignoreTypes(true, true, true);
        impl_->midiIn.setCallback(&MidiController::RtMidiCallbackThunk, this);
    }
    catch (const RtMidiError& e) {
        impl_.reset();
        return Status::Fail(e.getMessage());
    }

    engine_ = engine;
    connectedPortName_ = impl_->midiIn.getPortName(static_cast<unsigned int>(portIndex));
    connected_ = true;
    loadBindings(connectedPortName_);
    SetSessionValue(LAST_DEVICE_KEY, connectedPortName_);

    return Status::Ok();
}

void MidiController::disconnect()
{
    if (impl_) {
        impl_->midiIn.cancelCallback();
        impl_->midiIn.closePort();
        impl_.reset();
    }

    connected_ = false;
    connectedPortName_.clear();
    engine_ = nullptr;
}

bool MidiController::autoConnectLastDevice(AudioEngine* engine)
{
    const std::string lastName = SessionValue(LAST_DEVICE_KEY);
    const std::vector<std::string> ports = listPorts();
    const auto port = std::ranges::find(ports, lastName);

    return !lastName.empty() && port != ports.end() && connect(static_cast<int>(port - ports.begin()), engine).ok();
}

void MidiController::armLearn(MidiControl control)
{
    std::lock_guard lock(bindingsMutex_);
    learnArmedControl_ = control;
}

void MidiController::cancelLearn() { armLearn(MidiControl::None); }

bool MidiController::isLearning() const { return learningControl() != MidiControl::None; }

MidiControl MidiController::learningControl() const
{
    std::lock_guard lock(bindingsMutex_);
    return learnArmedControl_;
}

MidiController::LastMessage MidiController::lastMessage() const
{
    std::lock_guard lock(bindingsMutex_);
    return lastMessage_;
}

MidiBinding MidiController::binding(MidiControl control) const
{
    std::lock_guard lock(bindingsMutex_);
    return state_.bindings[static_cast<size_t>(control)];
}

void MidiController::clearBinding(MidiControl control)
{
    std::lock_guard lock(bindingsMutex_);
    state_.bindings[static_cast<size_t>(control)] = MidiBinding{};
}

void MidiController::assignFxKnobVstParam(
    int deckIndex, int knobIndex, FxChainTarget chain, size_t pluginIndex, int32_t paramId, std::string paramLabel
)
{
    std::lock_guard lock(bindingsMutex_);
    if (FxKnobAssignment* slot = Slot(state_.fxKnobAssignments, deckIndex, knobIndex)) {
        *slot = {true, KnobTargetKind::VstParam, chain, pluginIndex, paramId, std::move(paramLabel)};
    }
}

void MidiController::assignFxKnobBuiltin(int deckIndex, int knobIndex, KnobTargetKind kind)
{
    std::lock_guard lock(bindingsMutex_);
    FxKnobAssignment* slot = Slot(state_.fxKnobAssignments, deckIndex, knobIndex);
    if (slot && kind != KnobTargetKind::VstParam) {
        *slot = {true, kind, FxChainTarget::DeckA, 0, 0, {}};
    }
}

void MidiController::clearFxKnobAssignment(int deckIndex, int knobIndex)
{
    std::lock_guard lock(bindingsMutex_);
    if (FxKnobAssignment* slot = Slot(state_.fxKnobAssignments, deckIndex, knobIndex)) {
        *slot = {};
    }
}

FxKnobAssignment MidiController::fxKnobAssignment(int deckIndex, int knobIndex) const
{
    std::lock_guard lock(bindingsMutex_);
    const FxKnobAssignment* slot = Slot(state_.fxKnobAssignments, deckIndex, knobIndex);

    return slot ? *slot : FxKnobAssignment{};
}

void MidiController::assignPad(int deckIndex, int padIndex, PadAction action)
{
    std::lock_guard lock(bindingsMutex_);
    if (PadAction* slot = Slot(state_.padAssignments, deckIndex, padIndex)) {
        *slot = std::move(action);
        slot->assigned = true;
    }
}

void MidiController::clearPadAssignment(int deckIndex, int padIndex)
{
    std::lock_guard lock(bindingsMutex_);
    if (PadAction* slot = Slot(state_.padAssignments, deckIndex, padIndex)) {
        *slot = {};
    }
}

PadAction MidiController::padAssignment(int deckIndex, int padIndex) const
{
    std::lock_guard lock(bindingsMutex_);
    const PadAction* slot = Slot(state_.padAssignments, deckIndex, padIndex);

    return slot ? *slot : PadAction{};
}

void MidiController::swapPluginAssignments(FxChainTarget chain, size_t indexA, size_t indexB)
{
    std::lock_guard lock(bindingsMutex_);
    ForEachPluginReference(state_, chain, [&](auto& assignment) {
        if (assignment.pluginIndex == indexA) {
            assignment.pluginIndex = indexB;
        }
        else if (assignment.pluginIndex == indexB) {
            assignment.pluginIndex = indexA;
        }
    });
}

void MidiController::removePluginAssignments(FxChainTarget chain, size_t removedIndex)
{
    std::lock_guard lock(bindingsMutex_);
    ForEachPluginReference(state_, chain, [&](auto& assignment) {
        if (assignment.pluginIndex == removedIndex) {
            assignment = {};
        }
        else if (assignment.pluginIndex > removedIndex) {
            --assignment.pluginIndex;
        }
    });
}

VstChain* MidiController::chainForTarget(FxChainTarget target) const
{
    if (!engine_) {
        return nullptr;
    }

    return target == FxChainTarget::Master ? &engine_->masterVstChain()
                                           : &engine_->deck(FxChainTargetDeck(target)).vstChain;
}

void MidiController::releaseScratch(int deckIndex)
{
    if (!engine_) {
        return;
    }

    double endingRate = engine_->deck(deckIndex).endScratch() / SAMPLE_RATE;
    engine_->deck(deckIndex).startSpinback(endingRate, engine_->jogSpinbackDurationSec());
}

void MidiController::RtMidiCallbackThunk(double timeStamp, std::vector<unsigned char>* message, void* userData)
{
    if (message) {
        static_cast<MidiController*>(userData)->handleMessage(timeStamp, *message);
    }
}

void MidiController::handleMessage(double, const std::vector<unsigned char>& message)
{
    {
        std::lock_guard lock(bindingsMutex_);
        lastMessage_ = {
            true,
            message.empty() ? uint8_t{0} : message[0],
            message.size() > 1 ? message[1] : uint8_t{0},
            message.size() > 2 ? message[2] : uint8_t{0},
            message.size(),
            false,
            {}
        };
    }

    if (message.size() < 2) {
        return;
    }

    uint8_t status = message[0];
    uint8_t statusHighNibble = status & 0xF0;
    uint8_t channel = status & 0x0F;
    uint8_t data1 = message[1];
    uint8_t data2 = message.size() >= 3 ? message[2] : 0;

    if (statusHighNibble != 0x80 && statusHighNibble != 0x90 && statusHighNibble != 0xB0) {
        return;
    }

    MidiControl target = MidiControl::None;
    {
        std::lock_guard lock(bindingsMutex_);
        if (learnArmedControl_ != MidiControl::None) {
            bool isRealPress = statusHighNibble == 0x90 && data2 > 0;
            bool isCC = statusHighNibble == 0xB0;
            if (isRealPress || isCC) {
                state_.bindings[static_cast<size_t>(learnArmedControl_)] =
                    MidiBinding{true, statusHighNibble, channel, data1};
                learnArmedControl_ = MidiControl::None;
            }

            return;
        }

        uint8_t matchNibble = (statusHighNibble == 0x90 && data2 == 0) ? 0x80 : statusHighNibble;
        for (size_t i = 0; i < state_.bindings.size(); ++i) {
            const MidiBinding& b = state_.bindings[i];
            if (!b.bound || b.channel != channel || b.data1 != data1) {
                continue;
            }

            if (b.statusHighNibble == matchNibble || (b.statusHighNibble == 0x90 && matchNibble == 0x80)) {
                target = static_cast<MidiControl>(i);
                lastMessage_.matchedControl = true;
                lastMessage_.matchedLabel = MidiControlKey(target);
                break;
            }
        }
    }

    if (target != MidiControl::None) {
        dispatch(target, statusHighNibble, data2);
    }
}

void MidiController::applyFxKnob(int deckIndex, int knobIndex, float value)
{
    FxKnobAssignment assignment = fxKnobAssignment(deckIndex, knobIndex);
    if (!assignment.assigned) {
        return;
    }

    Deck& deck = engine_->deck(deckIndex);
    const float bipolar = value * 2.0f - 1.0f;
    switch (assignment.targetKind) {
        case KnobTargetKind::VstParam: {
            VstChain* chain = chainForTarget(assignment.chain);
            if (chain && assignment.pluginIndex < chain->size()) {
                chain->at(assignment.pluginIndex).setParameterNormalized(assignment.paramId, value);
            }

            return;
        }
        case KnobTargetKind::DeckGain: deck.setGain(value * 2.0f); return;
        case KnobTargetKind::DeckPan: deck.setPan(bipolar); return;
        case KnobTargetKind::EqLow: deck.setEqLow(bipolar * ThreeBandEq::MAX_GAIN_DB); return;
        case KnobTargetKind::EqMid: deck.setEqMid(bipolar * ThreeBandEq::MAX_GAIN_DB); return;
        case KnobTargetKind::EqHigh: deck.setEqHigh(bipolar * ThreeBandEq::MAX_GAIN_DB); return;
        case KnobTargetKind::Filter: deck.setFilter(bipolar); return;
    }
}

void MidiController::applyPad(int deckIndex, int padIndex)
{
    PadAction action = padAssignment(deckIndex, padIndex);
    if (!action.assigned) {
        return;
    }

    switch (action.kind) {
        case PadActionKind::TogglePluginBypass: {
            VstChain* chain = chainForTarget(action.chain);
            if (chain && action.pluginIndex < chain->size()) {
                VstPluginInstance& plugin = chain->at(action.pluginIndex);
                plugin.setBypassed(!plugin.bypassed());
            }

            return;
        }
        case PadActionKind::JumpToMarker: engine_->deck(deckIndex).jumpToMarker(action.markerIndex); return;
        case PadActionKind::None: return;
    }
}

void MidiController::dispatch(MidiControl control, uint8_t statusHighNibble, uint8_t data2)
{
    if (!engine_) {
        return;
    }

    const ControlInfo& info = ControlInfoFor(control);
    const ControlEvent event{
        info.deckIndex,
        info.slot,
        statusHighNibble == 0x90 && data2 > 0,
        statusHighNibble == 0xB0,
        data2 / 127.0f,
        data2
    };
    apply(info.action, event);
}

void MidiController::apply(ActionKind action, const ControlEvent& event)
{
    AudioEngine& engine = *engine_;
    const int index = event.deckIndex;
    Deck& deck = engine.deck(index);

    switch (action) {
        case ActionKind::Cue:
            if (event.pressed) {
                deck.cuePress();
            }
            else if (!event.continuous) {
                deck.cueRelease();
            }

            return;
        case ActionKind::JogRotate:
            if (event.continuous && event.raw != 64) {
                const std::optional<Seconds> target = jog_[index].rotate(
                    NowSeconds(),
                    static_cast<int>(event.raw) - 64,
                    deck.positionSeconds(),
                    deck.duration(),
                    deck.scratching.load()
                );
                if (target) {
                    deck.setScratchTarget(*target);
                }
            }

            return;
        default: break;
    }

    if (event.continuous) {
        switch (action) {
            case ActionKind::Volume: deck.setVolume(event.value); break;
            case ActionKind::Gain: deck.setGain(event.value * 2.0f); break;
            case ActionKind::Tempo: deck.setPlaybackRate(TempoFaderToRate(event.raw)); break;
            case ActionKind::Crossfader: engine.setCrossfade(event.value); break;
            case ActionKind::FxKnob: applyFxKnob(index, event.slot, event.value); break;
            default: break;
        }

        return;
    }

    if (!event.pressed) {
        return;
    }

    switch (action) {
        case ActionKind::Play: deck.togglePlay(); break;
        case ActionKind::Stop: deck.stop(); break;
        case ActionKind::Sync: engine.syncTempoToReferenceDeck(index); break;
        case ActionKind::SyncLock: engine.toggleSyncEnabled(index); break;
        case ActionKind::AlignPlay:
            engine.alignBeatToOtherDeck(index);
            deck.play();
            break;
        case ActionKind::SetHotCue: deck.setHotCue(); break;
        case ActionKind::JumpHotCue: engine.jumpToHotCue(index); break;
        case ActionKind::LoopIn: deck.setLoopInHere(); break;
        case ActionKind::LoopOut: deck.setLoopOutHere(); break;
        case ActionKind::LoopToggle: deck.setLoopEnabled(!deck.loopEnabled.load()); break;
        case ActionKind::Pad: applyPad(index, event.slot); break;
        case ActionKind::MonitorToggle: engine.deck(event.slot).toggleCue(); break;
        case ActionKind::MonitorToggleMaster: engine.setCueMaster(!engine.cueMaster()); break;
        default: break;
    }
}

void MidiController::update()
{
    if (!engine_) {
        return;
    }

    double now = NowSeconds();
    for (int deck = 0; deck < MAX_DECK_COUNT; ++deck) {
        if (engine_->deck(deck).scratching.load() && jog_[deck].idle(now)) {
            releaseScratch(deck);
        }
    }
}

bool MidiController::loadBindings(const std::string& deviceName)
{
    Json root;
    if (!LoadJsonFile(MidiMappingFilePath(deviceName), root)) {
        return false;
    }

    importBindings(ReadMidiBindings(root));
    return true;
}

Status MidiController::saveBindings() const
{
    if (connectedPortName_.empty()) {
        return Status::Fail(TrFormat("error.no_midi_device"));
    }

    Json root;
    WriteMidiBindings(root, exportBindings());

    return SaveVersionedJson(MidiMappingFilePath(connectedPortName_), root, MIDI_BINDINGS_FILE_VERSION);
}

MidiController::BindingSnapshot MidiController::exportBindings() const
{
    std::lock_guard lock(bindingsMutex_);
    return state_;
}

void MidiController::importBindings(const BindingSnapshot& snapshot)
{
    std::lock_guard lock(bindingsMutex_);
    state_ = snapshot;
}

} // namespace imdj
