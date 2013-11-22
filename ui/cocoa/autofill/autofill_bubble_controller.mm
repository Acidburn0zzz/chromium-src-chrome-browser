// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/autofill/autofill_bubble_controller.h"

#import "chrome/browser/ui/cocoa/autofill/autofill_dialog_constants.h"
#import "chrome/browser/ui/cocoa/info_bubble_view.h"
#import "chrome/browser/ui/cocoa/info_bubble_window.h"
#include "skia/ext/skia_utils_mac.h"

namespace {

// Border inset for error label.
const CGFloat kLabelInset = 3.0;

}  // namespace


@implementation AutofillBubbleController

- (id)initWithParentWindow:(NSWindow*)parentWindow
                   message:(NSString*)message {
  base::scoped_nsobject<InfoBubbleWindow> window(
      [[InfoBubbleWindow alloc] initWithContentRect:NSMakeRect(0, 0, 200, 100)
                                          styleMask:NSBorderlessWindowMask
                                            backing:NSBackingStoreBuffered
                                              defer:NO]);
  [window setAllowedAnimations:info_bubble::kAnimateNone];
  if ((self = [super initWithWindow:window
                       parentWindow:parentWindow
                         anchoredAt:NSZeroPoint])) {
    [self setShouldOpenAsKeyWindow:NO];
    [[self bubble] setArrowLocation:info_bubble::kTopCenter];
    [[self bubble] setAlignment:info_bubble::kAlignArrowToAnchor];

    label_.reset([[NSTextField alloc] init]);
    [label_ setEditable:NO];
    [label_ setBordered:NO];
    [label_ setDrawsBackground:NO];
    [label_ setStringValue:message];
    NSSize labelSize = [[label_ cell] cellSizeForBounds:
        NSMakeRect(
            0, 0,
            2 * autofill::kFieldWidth + autofill::kHorizontalFieldPadding,
            CGFLOAT_MAX)];
    [label_ setFrameSize:labelSize];
    [label_ setFrameOrigin:NSMakePoint(kLabelInset, kLabelInset)];

    [[self bubble] addSubview:label_];

    NSRect windowFrame = [[self window] frame];
    windowFrame.size = NSMakeSize(
        NSMaxX([label_ frame]),
        NSHeight([label_ frame]) + info_bubble::kBubbleArrowHeight);
    windowFrame = NSInsetRect(windowFrame, -kLabelInset, -kLabelInset);
    [[self window] setFrame:windowFrame display:NO];
  }
  return self;
}

@end
