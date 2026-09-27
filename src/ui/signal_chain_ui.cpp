#include "ui/signal_chain_ui.h"

#include <algorithm>

#include "core/strings.h"
#include "imgui.h"
#include "ui/modal.h"
#include "ui/ui_common.h"

namespace imdj {

namespace {

void DrawFlowArrow()
{
    const float size = ImGui::GetFontSize() * 0.3f;
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const float x = pos.x + ImGui::GetFrameHeight() * 0.5f;
    ImGui::GetWindowDrawList()->AddTriangleFilled(
        ImVec2(x - size, pos.y),
        ImVec2(x + size, pos.y),
        ImVec2(x, pos.y + size * 1.5f),
        ImGui::GetColorU32(ImGuiCol_TextDisabled)
    );
    ImGui::Dummy(ImVec2(0.0f, size * 1.5f));
}

void DrawEndpoint(const char* label)
{
    ImGui::AlignTextToFramePadding();
    ImGui::TextDisabled("%s", label);
}

void DrawPlugins(VstChain& plugins)
{
    ImGui::Indent(ImGui::GetFrameHeight() * 2.0f);
    if (plugins.size() == 0) {
        ImGui::TextDisabled("%s", Tr("common.none"));
    }

    for (size_t i = 0; i < plugins.size(); ++i) {
        VstPluginInstance& plugin = plugins.at(i);
        ImGui::TextDisabled(
            "%s%s", plugin.descriptor().label.c_str(), plugin.bypassed() ? Tr("chain.bypassed_suffix") : ""
        );
    }

    ImGui::Unindent(ImGui::GetFrameHeight() * 2.0f);
}

void DrawStage(Deck& deck, DeckChain& chain, size_t position)
{
    DeckProcessor& processor = chain.at(position);
    ImGui::PushID(&processor);

    BoundCheckbox("##enabled", processor.enabled(), [&](bool on) { processor.setEnabled(on); });
    ImGui::SameLine();

    const float buttons = ImGui::CalcTextSize(Tr("common.up")).x + ImGui::CalcTextSize(Tr("common.down")).x +
                          ImGui::GetStyle().FramePadding.x * 4.0f + ImGui::GetStyle().ItemSpacing.x;
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, processor.enabled() ? 1.0f : 0.5f);
    ImGui::TextUnformatted(processor.name());
    ImGui::PopStyleVar();

    ImGui::SameLine();
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + std::max(0.0f, ImGui::GetContentRegionAvail().x - buttons));
    ImGui::BeginDisabled(position == 0);
    if (ImGui::SmallButton(TrLabel("common.up"))) {
        chain.moveUp(position);
    }

    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(position + 1 == chain.size());
    if (ImGui::SmallButton(TrLabel("common.down"))) {
        chain.moveDown(position);
    }

    ImGui::EndDisabled();
    if (processor.hostsPlugins()) {
        DrawPlugins(deck.vstChain);
    }

    ImGui::PopID();
}

void DrawDeckChain(Deck& deck)
{
    DeckChain& chain = deck.dsp.chain;
    DrawEndpoint(Tr("chain.input"));
    for (size_t position = 0; position < chain.size(); ++position) {
        DrawFlowArrow();
        DrawStage(deck, chain, position);
    }

    DrawFlowArrow();
    DrawEndpoint(Tr("chain.output"));

    ImGui::Spacing();
    if (ImGui::Button(TrLabel("chain.reset_order"))) {
        chain.resetOrder();
    }
}

} // namespace

void DrawSignalChainPopup(const UiContext& context)
{
    Modal modal(TrLabel("popup.signal_chain"), ImVec2(420, 0));
    if (!modal) {
        return;
    }

    if (ImGui::BeginTabBar("##deckChains")) {
        for (int i = 0; i < context.deckCount(); ++i) {
            if (ImGui::BeginTabItem(DeckLabel(i))) {
                DrawDeckChain(context.deck(i));
                ImGui::EndTabItem();
            }
        }

        ImGui::EndTabBar();
    }

    modal.closeButton();
}

} // namespace imdj
