#include "ui/transport_panel.h"

#include <algorithm>
#include <cmath>
#include <format>

#include "imgui.h"
#include "ui/ui_common.h"
#include "core/strings.h"

namespace imdj {

namespace {

constexpr float FLAG_SIZE = 9.0f;

struct StripView {
    ImDrawList* drawList = nullptr;
    ImVec2 min;
    ImVec2 max;
    double windowStart = 0.0;
    double windowSeconds = 1.0;

    float width() const { return max.x - min.x; }
    float height() const { return max.y - min.y; }
    double windowEnd() const { return windowStart + windowSeconds; }
    bool contains(double seconds) const { return seconds >= windowStart && seconds <= windowEnd(); }

    float xFor(double seconds) const
    {
        return min.x + static_cast<float>((seconds - windowStart) / windowSeconds) * width();
    }

    void line(double seconds, ImU32 color, float thickness = 2.0f) const
    {
        if (!contains(seconds)) {
            return;
        }

        float x = xFor(seconds);
        drawList->AddLine(ImVec2(x, min.y), ImVec2(x, max.y), color, thickness);
    }

    void flag(double seconds, ImU32 color, bool atTop, float direction) const
    {
        if (!contains(seconds)) {
            return;
        }

        line(seconds, color);

        float x = xFor(seconds);
        float y = atTop ? min.y : max.y;
        float tip = atTop ? y + FLAG_SIZE : y - FLAG_SIZE;
        drawList->AddTriangleFilled(ImVec2(x, y), ImVec2(x + FLAG_SIZE * direction, y), ImVec2(x, tip), color);
    }
};

ImU32 WaveformColorForBands(float lowEnergy, float midEnergy, float highEnergy)
{
    const ThemeColors& colors = Colors();
    const float total = lowEnergy + midEnergy + highEnergy;
    if (total <= 0.0001f) {
        return ToU32(colors.waveformSilent);
    }

    const float low = lowEnergy / total;
    const float mid = midEnergy / total;
    const float high = highEnergy / total;
    const ImVec4& a = colors.waveformLow;
    const ImVec4& b = colors.waveformMid;
    const ImVec4& c = colors.waveformHigh;
    return ToU32(ImVec4(
        a.x * low + b.x * mid + c.x * high,
        a.y * low + b.y * mid + c.y * high,
        a.z * low + b.z * mid + c.z * high,
        a.w * low + b.w * mid + c.w * high
    ));
}

void DrawWaveform(const StripView& view, const Deck& deck)
{
    const WaveformPeaks& peaks = deck.peaks;
    if (peaks.empty()) {
        return;
    }

    const double duration = deck.durationSeconds();
    const int columns = std::max(1, static_cast<int>(view.width()));
    const double secondsPerColumn = view.windowSeconds / columns;
    const bool exact = secondsPerColumn < 1.0 / WaveformPeaks::BUCKETS_PER_SECOND && deck.buffer.frameCount() > 0;
    const float midY = (view.min.y + view.max.y) * 0.5f;
    const float halfHeight = view.height() * 0.5f - 4.0f;

    for (int i = 0; i < columns; ++i) {
        double start = view.windowStart + static_cast<double>(i) * secondsPerColumn;
        double end = start + secondsPerColumn;
        if (end < 0.0 || start > duration) {
            continue;
        }

        size_t first = peaks.bucketForSeconds(std::max(0.0, start));
        size_t last = peaks.bucketForSeconds(std::min(duration, end));
        if (first >= peaks.size()) {
            continue;
        }

        if (last <= first) {
            last = first + 1;
        }

        last = std::min(last, peaks.size());

        float minimum = 0.0f;
        float maximum = 0.0f;
        float low = 0.0f;
        float mid = 0.0f;
        float high = 0.0f;
        for (size_t bucket = first; bucket < last; ++bucket) {
            const WaveformBucket& value = peaks.at(bucket);
            minimum = std::min(minimum, value.peakLow / 127.0f);
            maximum = std::max(maximum, value.peakHigh / 127.0f);
            low = std::max(low, value.energyLow / 255.0f);
            mid = std::max(mid, value.energyMid / 255.0f);
            high = std::max(high, value.energyHigh / 255.0f);
        }

        if (exact) {
            const uint64_t frameCount = deck.buffer.frameCount();
            const uint64_t firstFrame = static_cast<uint64_t>(std::max(0.0, start) * SAMPLE_RATE);
            const uint64_t lastFrame =
                std::min(frameCount, static_cast<uint64_t>(std::max(0.0, end) * SAMPLE_RATE) + 1);
            minimum = 0.0f;
            maximum = 0.0f;
            for (uint64_t frame = firstFrame; frame < lastFrame; ++frame) {
                const float* samples = deck.buffer.frame(frame);
                const float mono = (samples[0] + samples[1]) * 0.5f;
                minimum = std::min(minimum, mono);
                maximum = std::max(maximum, mono);
            }
        }

        float x = view.min.x + static_cast<float>(i);
        ImU32 color = WaveformColorForBands(low, mid, high);
        view.drawList->AddLine(ImVec2(x, midY - maximum * halfHeight), ImVec2(x, midY - minimum * halfHeight), color);
    }
}

void DrawBeatGridLines(const StripView& view, const BeatGrid& grid, double duration)
{
    if (!grid.valid()) {
        return;
    }

    const double anchor = grid.anchorFrame / SAMPLE_RATE;
    const double beatSeconds = grid.beatSeconds();
    long beat = static_cast<long>(std::floor((view.windowStart - anchor) / beatSeconds)) - 1;

    for (int guard = 0; guard < 4096; ++beat, ++guard) {
        double seconds = anchor + beat * beatSeconds;
        if (seconds > view.windowEnd()) {
            break;
        }

        if (seconds < view.windowStart || seconds > duration) {
            continue;
        }

        float x = view.xFor(seconds);
        if (beat % BEATS_PER_BAR == 0) {
            view.drawList->AddLine(
                ImVec2(x, view.min.y), ImVec2(x, view.min.y + view.height() * 0.5f), ToU32(Colors().downbeat), 2.0f
            );
        }
        else {
            view.drawList->AddLine(ImVec2(x, view.min.y), ImVec2(x, view.min.y + 8.0f), ToU32(Colors().beat));
        }
    }
}

void DrawLoopRegion(const StripView& view, const Deck& deck)
{
    double loopStart = static_cast<double>(deck.loopStart.load()) / SAMPLE_RATE;
    double loopEnd = static_cast<double>(deck.loopEnd.load()) / SAMPLE_RATE;

    if (deck.loopEnabled.load()) {
        float x0 = std::clamp(view.xFor(loopStart), view.min.x, view.max.x);
        float x1 = std::clamp(view.xFor(loopEnd), view.min.x, view.max.x);
        if (x1 > x0) {
            view.drawList->AddRectFilled(ImVec2(x0, view.min.y), ImVec2(x1, view.max.y), ToU32(Colors().loopRegion));
        }

        return;
    }

    if (deck.loopInSet.load()) {
        view.line(loopStart, ToU32(Colors().loopEdge));
    }

    if (deck.loopOutSet.load()) {
        view.line(loopEnd, ToU32(Colors().loopEdge));
    }
}

void DrawMarkers(const StripView& view, const Deck& deck)
{
    for (const Marker& marker : deck.markers) {
        double seconds = static_cast<double>(marker.frame) / SAMPLE_RATE;
        if (!view.contains(seconds)) {
            continue;
        }

        view.flag(seconds, ToU32(Colors().marker), false, 1.0f);
        if (marker.name.empty()) {
            continue;
        }

        float x = view.xFor(seconds);
        ImVec2 textPos(x + 2.0f, view.max.y - FLAG_SIZE - ImGui::GetFontSize());
        view.drawList->AddText(textPos, ToU32(Colors().marker), marker.name.c_str());

        ImVec2 textSize = ImGui::CalcTextSize(marker.name.c_str());
        ImVec2 hoverMin(x - FLAG_SIZE, view.max.y - FLAG_SIZE);
        ImVec2 hoverMax(x + textSize.x + 2.0f, view.max.y);
        if (ImGui::IsMouseHoveringRect(hoverMin, hoverMax)) {
            ImGui::SetTooltip("%s", marker.name.c_str());
        }
    }
}

void DrawCues(const StripView& view, const Deck& deck)
{
    view.line(0.0, ToU32(Colors().trackStart));
    view.line(deck.durationSeconds(), ToU32(Colors().trackEnd));
    view.flag(deck.startPoint().count(), ToU32(Colors().startPoint), true, 1.0f);
    if (deck.hotCueSet.load()) {
        view.flag(deck.hotCue().count(), ToU32(Colors().hotCue), true, -1.0f);
    }

    DrawMarkers(view, deck);
}

void HandleStripInput(const DeckView& deckView, const StripView& view, double beatSeconds)
{
    ImGui::InvisibleButton("##strip", ImVec2(view.width(), view.height()));

    static double dragWindowStart[MAX_DECK_COUNT] = {};
    static double dragWindowSeconds[MAX_DECK_COUNT] = {};
    const int index = deckView.index;
    if (ImGui::IsItemActivated()) {
        dragWindowStart[index] = view.windowStart;
        dragWindowSeconds[index] = view.windowSeconds;
    }

    if (ImGui::IsItemActive() && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        float fraction = std::clamp((ImGui::GetIO().MousePos.x - view.min.x) / view.width(), 0.0f, 1.0f);
        deckView.deck.setScratchTarget(Seconds{dragWindowStart[index] + fraction * dragWindowSeconds[index]});
    }

    if (ImGui::IsItemDeactivated()) {
        deckView.deck.endScratch();
    }

    float wheel = ImGui::GetIO().MouseWheel;
    if (!ImGui::IsItemHovered() || wheel == 0.0f) {
        return;
    }

    const bool ctrl = ImGui::GetIO().KeyCtrl;
    const bool shift = ImGui::GetIO().KeyShift;
    double step;
    if (ctrl && shift) {
        step = 0.004;
    }
    else if (ctrl) {
        step = 0.001;
    }
    else if (shift) {
        step = beatSeconds * BEATS_PER_BAR;
    }
    else {
        step = beatSeconds;
    }

    deckView.deck.seek(Seconds{deckView.deck.positionSeconds().count() - static_cast<double>(wheel) * step});
}

void DrawDeckStrip(const DeckView& deckView, int barsVisible, const ImVec2& size)
{
    const Deck& deck = deckView.deck;
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 topLeft = ImGui::GetCursorScreenPos();
    ImVec2 bottomRight(topLeft.x + size.x, topLeft.y + size.y);

    drawList->AddRectFilled(topLeft, bottomRight, ToU32(Colors().panelBg));
    drawList->AddRect(topLeft, bottomRight, ToU32(Colors().panelBorder));
    drawList->PushClipRect(topLeft, bottomRight, true);

    if (!deck.hasTrack() || size.x <= 1.0f) {
        drawList->AddText(
            ImVec2(topLeft.x + 8, topLeft.y + size.y * 0.5f - 8), ToU32(Colors().textDim), Tr("transport.no_track")
        );
        ImGui::InvisibleButton("##strip", size);
        drawList->PopClipRect();

        return;
    }

    const BeatGrid grid = deck.grid();
    const double beatSeconds = grid.valid() ? grid.beatSeconds() : 0.5;
    const double position = deck.positionSeconds().count();

    StripView view;
    view.drawList = drawList;
    view.min = topLeft;
    view.max = bottomRight;
    view.windowSeconds = std::max(0.25, barsVisible * BEATS_PER_BAR * beatSeconds);
    view.windowStart = position - view.windowSeconds * 0.5;

    DrawLoopRegion(view, deck);
    DrawWaveform(view, deck);
    DrawBeatGridLines(view, grid, deck.durationSeconds());
    DrawCues(view, deck);

    float playheadX = topLeft.x + size.x * 0.5f;
    drawList->AddLine(ImVec2(playheadX, topLeft.y), ImVec2(playheadX, bottomRight.y), ToU32(Colors().playhead), 2.0f);

    HandleStripInput(deckView, view, beatSeconds);
    drawList->PopClipRect();
}

void DrawBeatPulse(double phase)
{
    ImVec2 center = ImGui::GetCursorScreenPos();
    center.x += 10.0f;
    center.y += 10.0f;

    float brightness = static_cast<float>(1.0 - phase);
    ImU32 color = ToU32(Mix(Colors().beatPulseDim, Colors().beatPulseBright, brightness));
    ImGui::GetWindowDrawList()->AddCircleFilled(center, 4.0f + brightness * 5.0f, color);
    ImGui::Dummy(ImVec2(24, 20));
}

void DrawDeckControls(const DeckView& view)
{
    Deck& deck = view.deck;

    if (ImGui::Button(deck.playing.load() ? TrLabel("transport.pause") : TrLabel("transport.play"), ImVec2(70, 0))) {
        deck.togglePlay();
    }

    ImGui::SameLine();
    ImGui::BeginDisabled(!deck.hasTrack());
    if (ImGui::Button(TrLabel("transport.stop"), ImVec2(60, 0))) {
        deck.stop();
    }

    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::Button(TrLabel("transport.cue"), ImVec2(60, 0));
    if (ImGui::IsItemActivated()) {
        deck.cuePress();
    }

    if (ImGui::IsItemDeactivated()) {
        deck.cueRelease();
    }

    ImGui::SameLine();
    if (ImGui::Button(TrLabel("transport.align_play"), ImVec2(100, 0))) {
        view.engine.alignBeatToOtherDeck(view.index);
        deck.play();
    }

    ImGui::SameLine();
    if (ImGui::Button(TrLabel("transport.set_start"), ImVec2(100, 0))) {
        deck.setStartHere();
    }

    ImGui::SameLine();
    if (ImGui::Button(TrLabel("transport.clear_start"), ImVec2(100, 0))) {
        deck.clearStart();
    }

    ImGui::SameLine();
    AccentText(view.label());
    ImGui::SameLine();
    ImGui::Text(
        "  %s / %s", FormatTime(deck.positionSeconds().count()).c_str(), FormatTime(deck.durationSeconds()).c_str()
    );

    ImGui::SameLine(0.0f, 16.0f);
    ImGui::Text("%s", TrFormat("transport.beat", deck.currentBeat().count()).c_str());
    ImGui::SameLine(0.0f, 12.0f);
    DrawBeatPulse(deck.beatPhase());
}

void DrawDancerColumn(
    const DeckView& view, float columnWidth, float rowHeight, const std::vector<DancerInfo>& dancerLibrary
)
{
    DeckUiState& ui = view.ui;
    ImVec2 topLeft = ImGui::GetCursorScreenPos();
    ImVec2 bottomRight(topLeft.x + columnWidth, topLeft.y + rowHeight);
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    drawList->AddRectFilled(topLeft, bottomRight, ToU32(Colors().panelBg));
    drawList->AddRect(topLeft, bottomRight, ToU32(Colors().panelBorder));

    const ImTextureID texture = ui.dancer.textureForBeat(view.deck.currentBeat().count());
    if (texture != ImTextureID_Invalid) {
        float aspect = static_cast<float>(ui.dancer.height()) / static_cast<float>(std::max(1, ui.dancer.width()));
        float width = columnWidth;
        float height = width * aspect;
        if (height > rowHeight) {
            height = rowHeight;
            width = height / std::max(0.01f, aspect);
        }

        ImVec2 imageMin(topLeft.x + (columnWidth - width) * 0.5f, topLeft.y + (rowHeight - height) * 0.5f);
        ImVec2 imageMax(imageMin.x + width, imageMin.y + height);
        drawList->AddImage(texture, imageMin, imageMax);
    }

    ImGui::InvisibleButton("##dancerImg", ImVec2(columnWidth, rowHeight));
    if (!ImGui::BeginPopupContextItem("##dancerMenu")) {
        return;
    }

    if (ImGui::Selectable(TrLabel("transport.dancer_none"), ui.selectedDancerIndex == -1)) {
        ui.selectedDancerIndex = -1;
        ui.dancer.unload();
    }

    for (int i = 0; i < static_cast<int>(dancerLibrary.size()); ++i) {
        bool selected = ui.selectedDancerIndex == i;
        if (ImGui::Selectable(DancerLabel(dancerLibrary[i]).c_str(), selected)) {
            ui.selectedDancerIndex = i;
            if (Status loaded = ui.dancer.load(dancerLibrary[i]); !loaded) {
                view.notifications.error(loaded.error());
            }
        }

        if (selected) {
            ImGui::SetItemDefaultFocus();
        }
    }

    ImGui::EndPopup();
}

} // namespace

void DrawTransportPanel(const UiContext& context, int& barsVisible)
{
    AudioEngine& engine = context.engine;
    const std::vector<DancerInfo>& dancerLibrary = context.dancers;

    AccentText(Tr("transport.title"));
    ImGui::SameLine();
    ImGui::SetNextItemWidth(220);
    ImGui::SliderInt(TrLabel("transport.zoom"), &barsVisible, 1, 64);
    ImGui::Separator();

    constexpr float DANCER_COLUMN_WIDTH = 100.0f;
    const float rowHeight = ImGui::GetFrameHeightWithSpacing();
    const ImVec2 spacing = ImGui::GetStyle().ItemSpacing;
    const ImVec2 available = ImGui::GetContentRegionAvail();
    const int deckCount = engine.deckCount();
    const int columns = deckCount > 2 ? 2 : 1;
    const int rows = deckCount / columns;
    const float cellWidth = (available.x - spacing.x * static_cast<float>(columns - 1)) / static_cast<float>(columns);
    const float cellHeight = (available.y - spacing.y * static_cast<float>(rows - 1)) / static_cast<float>(rows);
    const float stripHeight = std::max(40.0f, cellHeight - rowHeight);

    for (int i = 0; i < deckCount; ++i) {
        const DeckView view = context.deckView(i);
        if (i % columns != 0) {
            ImGui::SameLine();
        }

        ImGui::BeginChild(
            std::format("transport{}", i).c_str(),
            ImVec2(cellWidth, cellHeight),
            false,
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse
        );
        DrawDeckControls(view);
        DrawDancerColumn(view, DANCER_COLUMN_WIDTH, stripHeight, dancerLibrary);
        ImGui::SameLine();
        DrawDeckStrip(view, barsVisible, ImVec2(ImGui::GetContentRegionAvail().x, stripHeight));
        ImGui::EndChild();
    }
}

} // namespace imdj
