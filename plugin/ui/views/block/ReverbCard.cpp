#include "ReverbCard.h"

#include "core/Fonts.h"
#include "core/KnobScale.h"
#include "core/Paint.h"
#include "core/Theme.h"

namespace t3k::ui {

namespace {
Knob::Options knobOptions(const char* label, float defaultValue) {
  Knob::Options o;
  o.label = label;
  o.scale = &scales::percent();
  o.defaultValue = defaultValue;
  return o;
}
}

ReverbCard::ReverbCard(Services& services, const ChainItem& item)
    : services_(services), id_(item.blockId),
      mix_(knobOptions("Mix", 0.25f)), room_(knobOptions("Room Size", 0.5f)) {
  addAndMakeVisible(power_);
  addAndMakeVisible(remove_);
  addAndMakeVisible(mix_);
  addAndMakeVisible(room_);
  power_.onClick = [this] {
    enabled_ = !enabled_;
    power_.setOn(enabled_);
    services_.chain.setBlockParam(id_, "enabled", enabled_);
  };
  remove_.onClick = [this] { services_.chain.removeBlock(id_); };
  mix_.onChange = [this](float v) { services_.chain.setBlockParam(id_, "mix", v); };
  room_.onChange = [this](float v) { services_.chain.setBlockParam(id_, "roomSize", v); };
  setSize(800, 320);
  setBlock(item);
}

void ReverbCard::setBlock(const ChainItem& item) {
  enabled_ = item.params.enabled;
  power_.setOn(enabled_);
  mix_.setValue(static_cast<float>(item.params.mix));
  room_.setValue(static_cast<float>(item.params.roomSize));
}

void ReverbCard::resized() {
  power_.setBounds(16, 7, theme::kIconBoxSize, theme::kIconBoxSize);
  remove_.setBounds(getWidth() - 16 - theme::kIconBoxSize, 7,
                    theme::kIconBoxSize, theme::kIconBoxSize);
  const int gap = 100;
  const int width = 120;
  const int center = getWidth() / 2;
  mix_.setBounds(center - gap - width, 115, width, Knob::heightFor(mix_.knobSize()));
  room_.setBounds(center + gap, 115, width, Knob::heightFor(room_.knobSize()));
}

void ReverbCard::paint(juce::Graphics& g) {
  paint::fill(g, getLocalBounds().toFloat(), 16.0f, theme::kSurface);
  g.setColour(theme::kWhite);
  g.setFont(Fonts::mono(16));
  g.drawText("REVERB", 64, 7, getWidth() - 128, 32, juce::Justification::centredLeft);
}

}  // namespace t3k::ui
