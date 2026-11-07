// The fixed design space every native component is laid out in (port of
// useUiScale.ts constants). One unit here is one design px; the
// editor scales the whole root with a single AffineTransform, so components
// never see the window size.
#pragma once

#include <cmath>

#include <juce_core/system/juce_TargetPlatform.h>

namespace t3k::ui::design {

// Pixel snapping the way Blink does it: half away from zero, so a control
// centred at 10.5 lands on 11. juce::roundToInt rounds half to even (10.5
// gives 10), which is one pixel off for every odd centring gap.
inline int snap(float v) { return static_cast<int>(std::lround(v)); }

inline constexpr int kWidth = 1024;
// Core UI height (content only; Figma's 600 includes a 22px mock title bar).
// The minimum: a window taller than the width calls for stretches the
// content column between header and faceplate by the surplus (see
// WindowFit.h and PluginRoot::setStretch); titles keep their place under the
// header and the content below them centres in the room.
inline constexpr int kHeight = 578;

// Chrome strips that grow the window instead of squishing the core.
inline constexpr int kBannerHeight = 44;   // AppBanner.tsx
inline constexpr int kHintHeight = 36;     // HintBar.tsx
inline constexpr int kHeaderHeight = 64;   // PluginHeader.tsx
inline constexpr int kPlateHeight = 108;   // Faceplate.tsx
// The main screen's column between header and faceplate at kHeight (the
// chain gallery's and block detail's room before any stretch).
inline constexpr int kColumnHeight = kHeight - kHeaderHeight - kPlateHeight;

// Editor window scale range: the width picks the scale, 1x to 2x.
inline constexpr double kMaxScale = 2.0;
// How far the column may stretch beyond kHeight (design px, at any scale):
// one core height covers every landscape screen (an iPad's 4:3 asks for
// about 90) while a dragged-out window still reads as the plugin.
inline constexpr int kMaxStretch = kHeight;

// Platform flags, known at compile time. kCoarsePointer is a touch
// platform (no hover, ever); it fixes the hint copy and seeds
// Services::pointer, which on a desktop build follows the input that
// arrives instead (a Windows or Linux tablet). T3K_UI_COARSE_POINTER
// overrides it for a touch build on a desktop.
#if JUCE_IOS
inline constexpr bool kIos = true;
#else
inline constexpr bool kIos = false;
#endif

#if defined(T3K_UI_COARSE_POINTER)
inline constexpr bool kCoarsePointer = T3K_UI_COARSE_POINTER;
#elif JUCE_IOS || JUCE_ANDROID
inline constexpr bool kCoarsePointer = true;
#else
inline constexpr bool kCoarsePointer = false;
#endif

#if JUCE_MAC || JUCE_IOS
inline constexpr bool kAppleModifierGlyphs = true;
#else
inline constexpr bool kAppleModifierGlyphs = false;
#endif

}  // namespace t3k::ui::design
