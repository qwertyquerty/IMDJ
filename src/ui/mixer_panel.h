#pragma once

#include "ui/ui_context.h"

namespace imdj {

void DrawMixerPanel(const UiContext& ui, float& transitionSeconds);
void DrawMasterPanel(const UiContext& ui);
void DrawPeakMonitorPanel(const UiContext& ui);

} // namespace imdj
