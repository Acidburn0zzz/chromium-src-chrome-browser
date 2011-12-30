// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_DOWNLOADS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_DOWNLOADS_UI_H_
#pragma once

#include "chrome/browser/ui/webui/chrome_web_ui.h"

class RefCountedMemory;

class DownloadsUI : public ChromeWebUI {
 public:
  explicit DownloadsUI(content::WebContents* contents);

  static RefCountedMemory* GetFaviconResourceBytes();

 private:
  DISALLOW_COPY_AND_ASSIGN(DownloadsUI);
};

#endif  // CHROME_BROWSER_UI_WEBUI_DOWNLOADS_UI_H_
