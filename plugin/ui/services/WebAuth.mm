#include "WebAuth.h"

#if JUCE_IOS

#import <AuthenticationServices/AuthenticationServices.h>
#import <UIKit/UIKit.h>

#include <juce_events/juce_events.h>

// ObjC objects live in a C++ Impl below; written for ARC so their lifetime
// follows it. NativeUi.cmake adds -fobjc-arc for this file alone (JUCE's own
// ObjC++ is MRC); this is the guard against that flag getting lost.
#if !__has_feature(objc_arc)
#error "WebAuth.mm is written for ARC (NativeUi.cmake adds -fobjc-arc for it)"
#endif

// The window the sheet is presented over. JUCE's standalone owns one
// UIWindow in the app's one UIWindowScene (juce_Windowing_ios.mm,
// JuceAppSceneDelegate), so the key window of a connected scene is it.
@interface T3kWebAuthAnchor : NSObject <ASWebAuthenticationPresentationContextProviding>
@end

@implementation T3kWebAuthAnchor
- (ASPresentationAnchor)presentationAnchorForWebAuthenticationSession:(ASWebAuthenticationSession*)session {
  UIWindow* fallback = nil;
  for (UIScene* scene in UIApplication.sharedApplication.connectedScenes) {
    if (![scene isKindOfClass:[UIWindowScene class]]) continue;
    for (UIWindow* window in static_cast<UIWindowScene*>(scene).windows) {
      if (window.isKeyWindow) return window;
      if (fallback == nil) fallback = window;
    }
  }
  return fallback;
}
@end

namespace t3k::ui::webauth {

struct Sheet::Impl {
  T3kWebAuthAnchor* anchor = [[T3kWebAuthAnchor alloc] init];
  ASWebAuthenticationSession* session = nil;
  OnEnd onEnd;
  // Bumped on every start and cancel, so the completion a cancelled or
  // superseded session raises is recognised and dropped.
  int generation = 0;
};

Sheet::Sheet() : impl_(std::make_unique<Impl>()) {}

Sheet::~Sheet() {
  cancel();
  masterReference.clear();
}

bool Sheet::active() const { return impl_->session != nil; }

bool Sheet::start(const juce::String& url, OnEnd onEnd) {
  cancel();
  NSString* text = [NSString stringWithUTF8String:url.toRawUTF8()];
  NSURL* target = text != nil ? [NSURL URLWithString:text] : nil;
  if (target == nil) return false;

  const int generation = ++impl_->generation;
  juce::WeakReference<Sheet> weak(this);
  auto completion = ^(NSURL* callbackUrl, NSError* error) {
    // No callback scheme is registered (the redirect goes to the loopback),
    // so a completion always means the sheet closed without one: Cancel on
    // the sheet or on the consent alert, or the system could not present
    // it. It lands on the main thread, which is the message thread, but
    // goes through callAsync so the session never changes phase from inside
    // whatever UIKit was doing when it fired.
    const bool canceled = error != nil && [error.domain isEqualToString:ASWebAuthenticationSessionErrorDomain] &&
                          error.code == ASWebAuthenticationSessionErrorCodeCanceledLogin;
    juce::String reason;
    if (error != nil && !canceled) reason = juce::String::fromUTF8([error.localizedDescription UTF8String]);
    else if (error == nil) reason = "unexpected callback " + juce::String::fromUTF8([callbackUrl.absoluteString UTF8String]);
    juce::MessageManager::callAsync([weak, generation, canceled, reason] {
      if (weak == nullptr || weak->impl_->generation != generation) return;
      auto fin = std::move(weak->impl_->onEnd);
      weak->impl_->onEnd = nullptr;
      weak->impl_->session = nil;
      if (fin) fin(canceled ? End::canceled : End::failed, reason);
    });
  };

  // The nullable-scheme initialiser is deprecated since iOS 17.4 in favour
  // of one that requires a callback matcher. With the loopback taking the
  // redirect there is nothing to match, so this one is the right fit, and
  // it is the only one available down to the iOS 16 floor.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
  ASWebAuthenticationSession* session = [[ASWebAuthenticationSession alloc] initWithURL:target
                                                                       callbackURLScheme:nil
                                                                       completionHandler:completion];
#pragma clang diagnostic pop
  session.presentationContextProvider = impl_->anchor;
  // Not ephemeral: the sheet shares Safari's cookies, so someone already
  // signed in to tone3000.com there is through in one tap. The price is the
  // system's one-time consent alert before the sheet, which is standard.
  session.prefersEphemeralWebBrowserSession = NO;
  if (![session start]) return false;
  impl_->session = session;
  impl_->onEnd = std::move(onEnd);
  return true;
}

void Sheet::cancel() {
  ASWebAuthenticationSession* session = impl_->session;
  impl_->session = nil;
  impl_->onEnd = nullptr;
  ++impl_->generation;  // the completion this cancel raises is dropped
  if (session != nil) [session cancel];
}

}  // namespace t3k::ui::webauth

#endif  // JUCE_IOS
