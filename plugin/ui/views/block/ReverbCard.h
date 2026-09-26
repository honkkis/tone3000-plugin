#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "model/ChainState.h"
#include "services/Services.h"
#include "widgets/ChromeIconButton.h"
#include "widgets/Knob.h"

namespace t3k::ui {

class ReverbCard : public juce::Component {
public:
  explicit ReverbCard(Services& services, const ChainItem& item);
  void setBlock(const ChainItem& item);
  void resized() override;
  void paint(juce::Graphics& g) override;

private:
  Services& services_;
  std::string id_;
  bool enabled_ = true;
  ChromeIconButton power_{Icon::Power, ChromeIconButton::Tone::power, help::Key::blockPower};
  ChromeIconButton remove_{Icon::Trash2, ChromeIconButton::Tone::plain, help::Key::removeBlock};
  Knob mix_, room_;
};

}  // namespace t3k::ui
