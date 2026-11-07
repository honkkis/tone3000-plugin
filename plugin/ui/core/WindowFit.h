// How a window box holds the design, shared by the plugin editor and the
// testbed host so both fit the root the same way.
//
// The width picks the scale: the root is always kWidth design px wide, so
// `scale = width / kWidth`. The height is free above the aspect-locked
// minimum (the design box at that scale): the surplus, in design px, is the
// column's stretch (PluginRoot::setStretch), and the window fills top to
// bottom at every aspect from the plugin's own 1024:578 up to an iPad's 4:3.
// A box too short for its width (a host that refused a resize, a screen
// held the other way) scales by height instead and letterboxes
// horizontally, so the UI is never squished or clipped.
//
// FitConstrainer is the matching resize policy for a window the user drags:
// the width within the scale range, and a height no less than the minimum
// for that width (JUCE's own constrainer only knows fixed limits or a fixed
// aspect; neither says "the minimum height follows the width").
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace t3k::ui::design {

struct Fit {
  double scale = 1.0;  // uniform; what the root's transform and Services::zoom carry
  int stretch = 0;     // design px added to the content column, 0 when letterboxing
};

// `minDesignHeight` is the design box at the current chrome (kHeight plus
// the strips). The scale never drops below a floor, so a box with no size
// yet is harmless.
Fit fit(int boxWidth, int boxHeight, int minDesignHeight);

class FitConstrainer : public juce::ComponentBoundsConstrainer {
public:
  // The design box the limits follow; re-set whenever the chrome changes.
  void setMinDesignHeight(int designHeight);
  int minDesignHeight() const { return minDesignHeight_; }

  // The height range at a width: the aspect-locked minimum up to kMaxStretch
  // more design px, both at that width's scale.
  int minHeightFor(int width) const;
  int maxHeightFor(int width) const;

  // Hosts that read limits rather than asking (the VST3 wrapper) see the
  // range for `width`; checkBounds enforces it for whatever width a drag
  // proposes.
  void updateLimitsFor(int width);

  void checkBounds(juce::Rectangle<int>& bounds, const juce::Rectangle<int>& previousBounds,
                   const juce::Rectangle<int>& limits, bool isStretchingTop, bool isStretchingLeft,
                   bool isStretchingBottom, bool isStretchingRight) override;

private:
  int minDesignHeight_ = 0;
};

}  // namespace t3k::ui::design
