// In-app sign-in on iOS (ASWebAuthenticationSession); a no-op elsewhere.
//
// App Store review rejects an app that hands the user to Safari to sign in
// (Guideline 4, Design): sign-in has to happen inside the app, and Apple's
// API for exactly this is ASWebAuthenticationSession, a Safari sheet
// presented over the app with the page's address and certificate on show,
// Safari's cookies shared (an existing tone3000.com login carries over, after
// the system's one-time "wants to use tone3000.com to sign in" consent) and
// a Cancel button. The desktop plugin keeps the system browser, where a
// hand-off is the norm and no review applies.
//
// The redirect still lands on the LoopbackServer. The sheet is a browser:
// it follows the server's redirect to http://localhost:<port>/ like any
// other page, the listener takes the query, and the session dismisses the
// sheet from here (cancel()). So the server sees the same redirect_uri as
// desktop and nothing new needs registering for the publishable key; only
// launchInDefaultBrowser is replaced. The sheet's own completion handler
// therefore only ever reports the sheet closing without a redirect: the user
// cancelled, or it could not be presented at all.
//
// Same shim pattern as IosAudioRoute / AudioPermissions: one header, one
// ObjC++ implementation compiled on iOS only (NativeUi.cmake), header-only
// no-ops everywhere else so the desktop plugin and the testbed link
// unchanged.
#pragma once

#include <juce_core/juce_core.h>

#include <functional>
#include <memory>

namespace t3k::ui::webauth {

// Where sign-in is presented inside the app rather than handed to the
// system browser.
#if JUCE_IOS
inline constexpr bool kInApp = true;
#else
inline constexpr bool kInApp = false;
#endif

// How a sheet ended without the loopback taking the redirect.
enum class End {
  canceled,  // the user dismissed it (Cancel, or the consent alert's Cancel)
  failed     // the system refused to present it; `reason` is its words
};
using OnEnd = std::function<void(End end, const juce::String& reason)>;

#if JUCE_IOS

class Sheet {
public:
  Sheet();
  ~Sheet();

  // Present `url` over the app. Message thread only. `onEnd` fires once, on
  // the message thread, if the sheet closes on its own (see End); not after
  // cancel(). False when the session would not start at all (onEnd is not
  // called then either).
  bool start(const juce::String& url, OnEnd onEnd);
  // Dismiss a sheet still up: the redirect landed on the loopback, or the
  // flow was abandoned. onEnd does not fire for this.
  void cancel();
  bool active() const;

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;

  JUCE_DECLARE_WEAK_REFERENCEABLE(Sheet)
};

#else

class Sheet {
public:
  bool start(const juce::String&, OnEnd) { return false; }
  void cancel() {}
  bool active() const { return false; }
};

#endif

}  // namespace t3k::ui::webauth
