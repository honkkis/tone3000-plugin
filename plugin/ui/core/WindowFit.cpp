#include "WindowFit.h"

#include <algorithm>
#include <cmath>

#include "Design.h"

namespace t3k::ui::design {

namespace {
// A box with no size yet (before the first layout) fits at this rather
// than at zero.
constexpr double kMinScale = 0.05;

int maxWidth() { return juce::roundToInt(kWidth * kMaxScale); }
}  // namespace

Fit fit(int boxWidth, int boxHeight, int minDesignHeight) {
  const double byWidth = boxWidth / static_cast<double>(kWidth);
  const double byHeight = boxHeight / static_cast<double>(minDesignHeight);
  // Too short for the width: the height picks the scale and the sides go
  // black. Nothing to stretch.
  if (byHeight < byWidth) return {std::max(kMinScale, byHeight), 0};
  const double scale = std::max(kMinScale, byWidth);
  // Whole design rows only, so the root never overruns the box by a
  // fraction; what is left over (under one scaled px) stays black below.
  const int rows = static_cast<int>(std::floor(boxHeight / scale));
  return {scale, std::clamp(rows - minDesignHeight, 0, kMaxStretch)};
}

void FitConstrainer::setMinDesignHeight(int designHeight) { minDesignHeight_ = designHeight; }

int FitConstrainer::minHeightFor(int width) const {
  return juce::roundToInt(minDesignHeight_ * width / static_cast<double>(kWidth));
}

int FitConstrainer::maxHeightFor(int width) const {
  return juce::roundToInt((minDesignHeight_ + kMaxStretch) * width / static_cast<double>(kWidth));
}

void FitConstrainer::updateLimitsFor(int width) {
  const int w = std::clamp(width, kWidth, maxWidth());
  setSizeLimits(kWidth, minHeightFor(w), maxWidth(), maxHeightFor(w));
}

void FitConstrainer::checkBounds(juce::Rectangle<int>& bounds, const juce::Rectangle<int>& previousBounds,
                                 const juce::Rectangle<int>& limits, bool isStretchingTop, bool isStretchingLeft,
                                 bool isStretchingBottom, bool isStretchingRight) {
  // The height range follows the width this drag proposes; with the limits
  // pointed at it, the base class does the rest as it always has (the size,
  // which edges stay put, how far off screen a window may go).
  updateLimitsFor(bounds.getWidth());
  ComponentBoundsConstrainer::checkBounds(bounds, previousBounds, limits, isStretchingTop, isStretchingLeft,
                                          isStretchingBottom, isStretchingRight);
}

}  // namespace t3k::ui::design
