// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/scoped_ptr.h"
#include "chrome/browser/cocoa/cocoa_test_helper.h"
#include "chrome/browser/cocoa/fullscreen_window.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

@interface PerformCloseUIItem : NSObject<NSValidatedUserInterfaceItem>
@end

@implementation PerformCloseUIItem
- (SEL)action {
  return @selector(performClose:);
}

- (NSInteger)tag {
  return 0;
}
@end

class FullscreenWindowTest : public CocoaTest {
};

TEST_F(FullscreenWindowTest, Basics) {
  scoped_nsobject<FullscreenWindow> window;
  window.reset([[FullscreenWindow alloc] init]);

  EXPECT_EQ([NSScreen mainScreen], [window screen]);
  EXPECT_TRUE([window canBecomeKeyWindow]);
  EXPECT_TRUE([window canBecomeMainWindow]);
  EXPECT_EQ(NSBorderlessWindowMask, [window styleMask]);
  EXPECT_TRUE(NSEqualRects([[NSScreen mainScreen] frame], [window frame]));
  EXPECT_FALSE([window isReleasedWhenClosed]);
}

TEST_F(FullscreenWindowTest, CanPerformClose) {
  scoped_nsobject<FullscreenWindow> window;
  window.reset([[FullscreenWindow alloc] init]);

  scoped_nsobject<PerformCloseUIItem> item;
  item.reset([[PerformCloseUIItem alloc] init]);

  EXPECT_TRUE([window validateUserInterfaceItem:item.get()]);
}
