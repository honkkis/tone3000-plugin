#pragma once

#include "GalleryTile.h"
#include "model/ChainState.h"
#include "widgets/ChromeIconButton.h"

namespace t3k::ui {

class ReverbTile : public GalleryTile {
public:
  ReverbTile(Services& services, const ChainItem& item, int size);
  void setBlock(const ChainItem& item);
  std::function<void(const std::string&)> onOpen;
  void paint(juce::Graphics& g) override;
  void resized() override;
  bool isInterestedInFileDrag(const juce::StringArray&) override { return false; }

private:
  void open() override { if (onOpen) onOpen(blockId()); }
  std::vector<ContextMenu::Item> menuItems() override;
  bool enabled_ = true;
  ChromeIconButton power_{Icon::Power, ChromeIconButton::Tone::power, help::Key::blockPower};
  ChromeIconButton remove_{Icon::Trash2, ChromeIconButton::Tone::plain, help::Key::removeBlock};
};

}  // namespace t3k::ui
