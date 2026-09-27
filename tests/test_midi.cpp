#include <set>
#include <string>

#include "doctest/doctest.h"
#include "midi/midi_control_table.h"
#include "midi/midi_types.h"

using namespace imdj;

TEST_CASE("MIDI control keys mapping check")
{
    CHECK(std::string(MidiControlKey(MidiControl::DeckA_Play)) == "DeckA_Play");
    CHECK(std::string(MidiControlKey(MidiControl::DeckA_JogRotate)) == "DeckA_JogRotate");
    CHECK(std::string(MidiControlKey(DeckMidiControl(1, MidiControl::DeckA_Play))) == "DeckB_Play");
    CHECK(std::string(MidiControlKey(DeckMidiControl(2, MidiControl::DeckA_Pad1, 4))) == "DeckC_Pad5");
    CHECK(std::string(MidiControlKey(DeckMidiControl(3, MidiControl::DeckA_FxKnob1, 3))) == "DeckD_FxKnob4");
    CHECK(std::string(MidiControlKey(MidiControl::Crossfader)) == "Crossfader");
    CHECK(std::string(MidiControlKey(MidiControl::MonitorToggleC)) == "MonitorToggleC");
    CHECK(std::string(MidiControlKey(MidiControl::MonitorToggleMaster)) == "MonitorToggleMaster");

    std::set<std::string> keys;
    for (int i = 0; i < static_cast<int>(MidiControl::Count); ++i) {
        std::string key = MidiControlKey(static_cast<MidiControl>(i));
        CHECK_FALSE(key.empty());
        keys.insert(key);
    }

    CHECK(keys.size() == static_cast<size_t>(MidiControl::Count));
}

TEST_CASE("Control table routes every deck block by stride")
{
    for (int deck = 0; deck < MAX_DECK_COUNT; ++deck) {
        const ControlInfo& play = ControlInfoFor(DeckMidiControl(deck, MidiControl::DeckA_Play));
        CHECK(play.deckIndex == deck);
        CHECK(play.action == ActionKind::Play);
        CHECK(play.slot == -1);

        const ControlInfo& pad = ControlInfoFor(DeckMidiControl(deck, MidiControl::DeckA_Pad1, 2));
        CHECK(pad.deckIndex == deck);
        CHECK(pad.action == ActionKind::Pad);
        CHECK(pad.slot == 2);

        const ControlInfo& monitor =
            ControlInfoFor(static_cast<MidiControl>(static_cast<int>(MidiControl::MonitorToggleA) + deck));
        CHECK(monitor.deckIndex == -1);
        CHECK(monitor.action == ActionKind::MonitorToggle);
        CHECK(monitor.slot == deck);
    }

    const ControlInfo& none = ControlInfoFor(MidiControl::None);
    CHECK(none.key.empty());
    CHECK(ControlInfoFor(MidiControl::Count).key.empty());
}
