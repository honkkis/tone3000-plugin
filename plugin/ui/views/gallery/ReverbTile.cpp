#include "ReverbTile.h"

#include "core/Fonts.h"
#include "core/Paint.h"
#include "core/Theme.h"
#include "GalleryGeometry.h"

namespace t3k::ui {

ReverbTile::ReverbTile(Services& services, const ChainItem& item, int size)
    : GalleryTile(services, item.blockId, size) {
  setTitle("Reverb");
  addAndMakeVisible(power_);
  addAndMakeVisible(remove_);
  power_.onClick = [this] {
    enabled_ = !enabled_;
    power_.setOn(enabled_);
    this->services().chain.setBlockParam(blockId(), "enabled", enabled_);
    repaint();
  };
  remove_.onClick = [this] { this->services().chain.removeBlock(blockId()); };
  setBlock(item);
  resized();
}

void ReverbTile::setBlock(const ChainItem& item) {
  enabled_ = item.params.enabled;
  power_.setOn(enabled_);
  repaint();
}

std::vector<ContextMenu::Item> ReverbTile::menuItems() {
  return {{"Copy", Icon::Copy, help::Key::copyBlock,
           [this] { services().chain.copyBlock(blockId()); }}};
}

void ReverbTile::resized() {
  power_.setBounds(4, 4, theme::kIconBoxSize, theme::kIconBoxSize);
  remove_.setBounds(getWidth() - theme::kIconBoxSize - 4, 4,
                    theme::kIconBoxSize, theme::kIconBoxSize);
}

void ReverbTile::paint(juce::Graphics& g) {
  paint::fill(g, getLocalBounds().toFloat(), gallery::kTileCorner, theme::kSurfaceRaised);
  g.setColour(enabled_ ? theme::kWhite : theme::kGray);
  g.setFont(Fonts::mono(14));
  g.drawText("REVERB", getLocalBounds().reduced(3), juce::Justification::centred);
}

}  // namespace t3k::ui
