// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "app/l10n_util.h"
#include "base/i18n/rtl.h"
#include "base/file_path.h"
#include "base/sys_info.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/browser/app_modal_dialog.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_init.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extensions_service.h"
#include "chrome/browser/js_modal_dialog.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/renderer_host/render_process_host.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/tabs/pinned_tab_codec.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/page_transition_types.h"
#include "chrome/test/in_process_browser_test.h"
#include "chrome/test/ui_test_utils.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "net/base/mock_host_resolver.h"

namespace {

const std::string BEFORE_UNLOAD_HTML =
    "<html><head><title>beforeunload</title></head><body>"
    "<script>window.onbeforeunload=function(e){return 'foo'}</script>"
    "</body></html>";

const std::wstring OPEN_NEW_BEFOREUNLOAD_PAGE =
    L"w=window.open(); w.onbeforeunload=function(e){return 'foo'};";

const FilePath::CharType* kTitle1File = FILE_PATH_LITERAL("title1.html");
const FilePath::CharType* kTitle2File = FILE_PATH_LITERAL("title2.html");

// Given a page title, returns the expected window caption string.
std::wstring WindowCaptionFromPageTitle(std::wstring page_title) {
#if defined(OS_MACOSX) || defined(OS_CHROMEOS)
  // On Mac or ChromeOS, we don't want to suffix the page title with
  // the application name.
  if (page_title.empty())
    return l10n_util::GetString(IDS_BROWSER_WINDOW_MAC_TAB_UNTITLED);
  return page_title;
#else
  if (page_title.empty())
    return l10n_util::GetString(IDS_PRODUCT_NAME);

  return l10n_util::GetStringF(IDS_BROWSER_WINDOW_TITLE_FORMAT, page_title);
#endif
}

// Returns the number of active RenderProcessHosts.
int CountRenderProcessHosts() {
  int result = 0;
  for (RenderProcessHost::iterator i(RenderProcessHost::AllHostsIterator());
       !i.IsAtEnd(); i.Advance())
    ++result;
  return result;
}

class MockTabStripModelObserver : public TabStripModelObserver {
 public:
  MockTabStripModelObserver() : closing_count_(0) {}

  virtual void TabClosingAt(TabContents* contents, int index) {
    closing_count_++;
  }

  int closing_count() const { return closing_count_; }

 private:
  int closing_count_;

  DISALLOW_COPY_AND_ASSIGN(MockTabStripModelObserver);
};

}  // namespace

class BrowserTest : public ExtensionBrowserTest {
 public:
  // Used by phantom tab tests. Creates two tabs, pins the first and makes it
  // a phantom tab (by closing it).
  void PhantomTabTest() {
    HTTPTestServer* server = StartHTTPServer();
    ASSERT_TRUE(server);
    host_resolver()->AddRule("www.example.com", "127.0.0.1");
    GURL url(server->TestServerPage("empty.html"));
    TabStripModel* model = browser()->tabstrip_model();

    ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII("app/")));

    Extension* extension_app = GetExtension();

    ui_test_utils::NavigateToURL(browser(), url);

    TabContents* app_contents = new TabContents(browser()->profile(), NULL,
                                                MSG_ROUTING_NONE, NULL);
    app_contents->SetExtensionApp(extension_app);

    model->AddTabContents(app_contents, 0, false, 0, false);
    model->SetTabPinned(0, true);
    ui_test_utils::NavigateToURL(browser(), url);

    // Close the first, which should make it a phantom.
    model->CloseTabContentsAt(0);

    // There should still be two tabs.
    ASSERT_EQ(2, browser()->tab_count());
    // The first tab should be a phantom.
    EXPECT_TRUE(model->IsPhantomTab(0));
    // And the tab contents of the first tab should have changed.
    EXPECT_TRUE(model->GetTabContentsAt(0) != app_contents);
  }

 protected:
  virtual void SetUpCommandLine(CommandLine* command_line) {
    ExtensionBrowserTest::SetUpCommandLine(command_line);

    // Needed for phantom tab tests.
    command_line->AppendSwitch(switches::kEnableExtensionApps);
  }

  // In RTL locales wrap the page title with RTL embedding characters so that it
  // matches the value returned by GetWindowTitle().
  std::wstring LocaleWindowCaptionFromPageTitle(
      const std::wstring& expected_title) {
    std::wstring page_title = WindowCaptionFromPageTitle(expected_title);
#if defined(OS_WIN)
    std::string locale = g_browser_process->GetApplicationLocale();
    if (base::i18n::GetTextDirectionForLocale(locale.c_str()) ==
        base::i18n::RIGHT_TO_LEFT) {
      base::i18n::WrapStringWithLTRFormatting(&page_title);
    }

    return page_title;
#else
    // Do we need to use the above code on POSIX as well?
    return page_title;
#endif
  }

  // Returns the app extension installed by PhantomTabTest.
  Extension* GetExtension() {
    const ExtensionList* extensions =
        browser()->profile()->GetExtensionsService()->extensions();
    for (size_t i = 0; i < extensions->size(); ++i) {
      if ((*extensions)[i]->name() == "App Test")
        return (*extensions)[i];
    }
    NOTREACHED();
    return NULL;
  }
};

// Launch the app on a page with no title, check that the app title was set
// correctly.
IN_PROC_BROWSER_TEST_F(BrowserTest, NoTitle) {
  ui_test_utils::NavigateToURL(browser(),
      ui_test_utils::GetTestUrl(FilePath(FilePath::kCurrentDirectory),
                                FilePath(kTitle1File)));
  EXPECT_EQ(LocaleWindowCaptionFromPageTitle(L"title1.html"),
            UTF16ToWideHack(browser()->GetWindowTitleForCurrentTab()));
  string16 tab_title;
  ASSERT_TRUE(ui_test_utils::GetCurrentTabTitle(browser(), &tab_title));
  EXPECT_EQ(ASCIIToUTF16("title1.html"), tab_title);
}

// Launch the app, navigate to a page with a title, check that the app title
// was set correctly.
IN_PROC_BROWSER_TEST_F(BrowserTest, Title) {
  ui_test_utils::NavigateToURL(browser(),
      ui_test_utils::GetTestUrl(FilePath(FilePath::kCurrentDirectory),
                                FilePath(kTitle2File)));
  const std::wstring test_title(L"Title Of Awesomeness");
  EXPECT_EQ(LocaleWindowCaptionFromPageTitle(test_title),
            UTF16ToWideHack(browser()->GetWindowTitleForCurrentTab()));
  string16 tab_title;
  ASSERT_TRUE(ui_test_utils::GetCurrentTabTitle(browser(), &tab_title));
  EXPECT_EQ(WideToUTF16(test_title), tab_title);
}

#if defined(OS_MACOSX)
// Test is crashing on Mac, see http://crbug.com/29424.
#define MAYBE_JavascriptAlertActivatesTab DISABLED_JavascriptAlertActivatesTab
#else
#define MAYBE_JavascriptAlertActivatesTab JavascriptAlertActivatesTab
#endif

IN_PROC_BROWSER_TEST_F(BrowserTest, MAYBE_JavascriptAlertActivatesTab) {
  GURL url(ui_test_utils::GetTestUrl(FilePath(FilePath::kCurrentDirectory),
                                     FilePath(kTitle1File)));
  ui_test_utils::NavigateToURL(browser(), url);
  browser()->AddTabWithURL(url, GURL(), PageTransition::TYPED,
                           0, Browser::ADD_SELECTED, NULL, std::string());
  EXPECT_EQ(2, browser()->tab_count());
  EXPECT_EQ(0, browser()->selected_index());
  TabContents* second_tab = browser()->GetTabContentsAt(1);
  ASSERT_TRUE(second_tab);
  second_tab->render_view_host()->ExecuteJavascriptInWebFrame(L"",
      L"alert('Activate!');");
  AppModalDialog* alert = ui_test_utils::WaitForAppModalDialog();
  alert->CloseModalDialog();
  EXPECT_EQ(2, browser()->tab_count());
  EXPECT_EQ(1, browser()->selected_index());
}

// Create 34 tabs and verify that a lot of processes have been created. The
// exact number of processes depends on the amount of memory. Previously we
// had a hard limit of 31 processes and this test is mainly directed at
// verifying that we don't crash when we pass this limit.
IN_PROC_BROWSER_TEST_F(BrowserTest, ThirtyFourTabs) {
  GURL url(ui_test_utils::GetTestUrl(FilePath(FilePath::kCurrentDirectory),
                                     FilePath(kTitle2File)));

  // There is one initial tab.
  for (int ix = 0; ix != 33; ++ix) {
    browser()->AddTabWithURL(url, GURL(), PageTransition::TYPED,
                             0, Browser::ADD_SELECTED, NULL, std::string());
  }
  EXPECT_EQ(34, browser()->tab_count());

  // See browser\renderer_host\render_process_host.cc for the algorithm to
  // decide how many processes to create.
  if (base::SysInfo::AmountOfPhysicalMemoryMB() >= 2048) {
    EXPECT_GE(CountRenderProcessHosts(), 24);
  } else {
    EXPECT_LE(CountRenderProcessHosts(), 23);
  }
}

// Test for crbug.com/22004.  Reloading a page with a before unload handler and
// then canceling the dialog should not leave the throbber spinning.
IN_PROC_BROWSER_TEST_F(BrowserTest, ReloadThenCancelBeforeUnload) {
  GURL url("data:text/html," + BEFORE_UNLOAD_HTML);
  ui_test_utils::NavigateToURL(browser(), url);

  // Navigate to another page, but click cancel in the dialog.  Make sure that
  // the throbber stops spinning.
  browser()->Reload();
  AppModalDialog* alert = ui_test_utils::WaitForAppModalDialog();
  alert->CloseModalDialog();
  EXPECT_FALSE(browser()->GetSelectedTabContents()->is_loading());

  // Clear the beforeunload handler so the test can easily exit.
  browser()->GetSelectedTabContents()->render_view_host()->
      ExecuteJavascriptInWebFrame(L"", L"onbeforeunload=null;");
}

// Crashy on mac.  http://crbug.com/40150
#if defined(OS_MACOSX)
#define MAYBE_SingleBeforeUnloadAfterWindowClose \
        DISABLED_SingleBeforeUnloadAfterWindowClose
#else
#define MAYBE_SingleBeforeUnloadAfterWindowClose \
        SingleBeforeUnloadAfterWindowClose
#endif

// Test for crbug.com/11647.  A page closed with window.close() should not have
// two beforeunload dialogs shown.
IN_PROC_BROWSER_TEST_F(BrowserTest, MAYBE_SingleBeforeUnloadAfterWindowClose) {
  browser()->GetSelectedTabContents()->render_view_host()->
      ExecuteJavascriptInWebFrame(L"", OPEN_NEW_BEFOREUNLOAD_PAGE);

  // Close the new window with JavaScript, which should show a single
  // beforeunload dialog.  Then show another alert, to make it easy to verify
  // that a second beforeunload dialog isn't shown.
  browser()->GetTabContentsAt(0)->render_view_host()->
      ExecuteJavascriptInWebFrame(L"", L"w.close(); alert('bar');");
  AppModalDialog* alert = ui_test_utils::WaitForAppModalDialog();
  alert->AcceptWindow();

  alert = ui_test_utils::WaitForAppModalDialog();
  EXPECT_FALSE(static_cast<JavaScriptAppModalDialog*>(alert)->
                   is_before_unload_dialog());
  alert->AcceptWindow();
}

// Test that get_process_idle_time() returns reasonable values when compared
// with time deltas measured locally.
IN_PROC_BROWSER_TEST_F(BrowserTest, RenderIdleTime) {
  base::TimeTicks start = base::TimeTicks::Now();
  ui_test_utils::NavigateToURL(browser(),
      ui_test_utils::GetTestUrl(FilePath(FilePath::kCurrentDirectory),
                                FilePath(kTitle1File)));
  RenderProcessHost::iterator it(RenderProcessHost::AllHostsIterator());
  for (; !it.IsAtEnd(); it.Advance()) {
    base::TimeDelta renderer_td =
        it.GetCurrentValue()->get_child_process_idle_time();
    base::TimeDelta browser_td = base::TimeTicks::Now() - start;
    EXPECT_TRUE(browser_td >= renderer_td);
  }
}

// Test IDC_CREATE_SHORTCUTS command is enabled for url scheme file, ftp, http
// and https and disabled for chrome://, about:// etc.
// TODO(pinkerton): Disable app-mode in the model until we implement it
// on the Mac. http://crbug.com/13148
#if !defined(OS_MACOSX)
IN_PROC_BROWSER_TEST_F(BrowserTest, CommandCreateAppShortcut) {
  static const wchar_t kDocRoot[] = L"chrome/test/data";
  static const FilePath::CharType* kEmptyFile = FILE_PATH_LITERAL("empty.html");

  CommandUpdater* command_updater = browser()->command_updater();

  // Urls that are okay to have shortcuts.
  GURL file_url(ui_test_utils::GetTestUrl(FilePath(FilePath::kCurrentDirectory),
                                          FilePath(kEmptyFile)));
  ASSERT_TRUE(file_url.SchemeIs(chrome::kFileScheme));
  ui_test_utils::NavigateToURL(browser(), file_url);
  EXPECT_TRUE(command_updater->IsCommandEnabled(IDC_CREATE_SHORTCUTS));

  scoped_refptr<FTPTestServer> ftp_server(
        FTPTestServer::CreateServer(kDocRoot));
  ASSERT_TRUE(NULL != ftp_server.get());
  GURL ftp_url(ftp_server->TestServerPage(""));
  ASSERT_TRUE(ftp_url.SchemeIs(chrome::kFtpScheme));
  ui_test_utils::NavigateToURL(browser(), ftp_url);
  EXPECT_TRUE(command_updater->IsCommandEnabled(IDC_CREATE_SHORTCUTS));

  scoped_refptr<HTTPTestServer> http_server(
        HTTPTestServer::CreateServer(kDocRoot, NULL));
  ASSERT_TRUE(NULL != http_server.get());
  GURL http_url(http_server->TestServerPage(""));
  ASSERT_TRUE(http_url.SchemeIs(chrome::kHttpScheme));
  ui_test_utils::NavigateToURL(browser(), http_url);
  EXPECT_TRUE(command_updater->IsCommandEnabled(IDC_CREATE_SHORTCUTS));

  scoped_refptr<HTTPSTestServer> https_server(
        HTTPSTestServer::CreateGoodServer(kDocRoot));
  ASSERT_TRUE(NULL != https_server.get());
  GURL https_url(https_server->TestServerPage("/"));
  ASSERT_TRUE(https_url.SchemeIs(chrome::kHttpsScheme));
  ui_test_utils::NavigateToURL(browser(), https_url);
  EXPECT_TRUE(command_updater->IsCommandEnabled(IDC_CREATE_SHORTCUTS));

  // Urls that should not have shortcuts.
  GURL new_tab_url(chrome::kChromeUINewTabURL);
  ui_test_utils::NavigateToURL(browser(), new_tab_url);
  EXPECT_FALSE(command_updater->IsCommandEnabled(IDC_CREATE_SHORTCUTS));

  GURL history_url(chrome::kChromeUIHistoryURL);
  ui_test_utils::NavigateToURL(browser(), history_url);
  EXPECT_FALSE(command_updater->IsCommandEnabled(IDC_CREATE_SHORTCUTS));

  GURL downloads_url(chrome::kChromeUIDownloadsURL);
  ui_test_utils::NavigateToURL(browser(), downloads_url);
  EXPECT_FALSE(command_updater->IsCommandEnabled(IDC_CREATE_SHORTCUTS));

  GURL blank_url(chrome::kAboutBlankURL);
  ui_test_utils::NavigateToURL(browser(), blank_url);
  EXPECT_FALSE(command_updater->IsCommandEnabled(IDC_CREATE_SHORTCUTS));
}
#endif  // !defined(OS_MACOSX)

// Test RenderView correctly send back favicon url for web page that redirects
// to an anchor in javascript body.onload handler.
IN_PROC_BROWSER_TEST_F(BrowserTest, FaviconOfOnloadRedirectToAnchorPage) {
  static const wchar_t kDocRoot[] = L"chrome/test/data";
  scoped_refptr<HTTPTestServer> server(
        HTTPTestServer::CreateServer(kDocRoot, NULL));
  ASSERT_TRUE(NULL != server.get());
  GURL url(server->TestServerPage("files/onload_redirect_to_anchor.html"));
  GURL expected_favicon_url(server->TestServerPage("files/test.png"));

  ui_test_utils::NavigateToURL(browser(), url);

  NavigationEntry* entry = browser()->GetSelectedTabContents()->
      controller().GetActiveEntry();
  EXPECT_EQ(expected_favicon_url.spec(), entry->favicon().url().spec());
}

// Test that an icon can be changed from JS.
IN_PROC_BROWSER_TEST_F(BrowserTest, FaviconChange) {
  static const FilePath::CharType* kFile =
      FILE_PATH_LITERAL("onload_change_favicon.html");
  GURL file_url(ui_test_utils::GetTestUrl(FilePath(FilePath::kCurrentDirectory),
                                          FilePath(kFile)));
  ASSERT_TRUE(file_url.SchemeIs(chrome::kFileScheme));
  ui_test_utils::NavigateToURL(browser(), file_url);

  NavigationEntry* entry = browser()->GetSelectedTabContents()->
      controller().GetActiveEntry();
  static const FilePath::CharType* kIcon =
      FILE_PATH_LITERAL("test1.png");
  GURL expected_favicon_url(
      ui_test_utils::GetTestUrl(FilePath(FilePath::kCurrentDirectory),
                                         FilePath(kIcon)));
  EXPECT_EQ(expected_favicon_url.spec(), entry->favicon().url().spec());
}

// TODO(sky): get these to run on a Mac.
#if !defined(OS_MACOSX)
IN_PROC_BROWSER_TEST_F(BrowserTest, PhantomTab) {
  PhantomTabTest();
}

IN_PROC_BROWSER_TEST_F(BrowserTest, RevivePhantomTab) {
  PhantomTabTest();

  if (HasFatalFailure())
    return;

  TabStripModel* model = browser()->tabstrip_model();

  // Revive the phantom tab by selecting it.
  browser()->SelectTabContentsAt(0, true);

  // There should still be two tabs.
  ASSERT_EQ(2, browser()->tab_count());
  // The first tab should no longer be a phantom.
  EXPECT_FALSE(model->IsPhantomTab(0));
}

// Makes sure TabClosing is sent when uninstalling an extension that is an app
// tab.
IN_PROC_BROWSER_TEST_F(BrowserTest, TabClosingWhenRemovingExtension) {
  HTTPTestServer* server = StartHTTPServer();
  ASSERT_TRUE(server);
  host_resolver()->AddRule("www.example.com", "127.0.0.1");
  GURL url(server->TestServerPage("empty.html"));
  TabStripModel* model = browser()->tabstrip_model();

  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII("app/")));

  Extension* extension_app = GetExtension();

  ui_test_utils::NavigateToURL(browser(), url);

  TabContents* app_contents = new TabContents(browser()->profile(), NULL,
                                              MSG_ROUTING_NONE, NULL);
  app_contents->SetExtensionApp(extension_app);

  model->AddTabContents(app_contents, 0, false, 0, false);
  model->SetTabPinned(0, true);
  ui_test_utils::NavigateToURL(browser(), url);

  MockTabStripModelObserver observer;
  model->AddObserver(&observer);

  // Uninstall the extension and make sure TabClosing is sent.
  ExtensionsService* service = browser()->profile()->GetExtensionsService();
  service->UninstallExtension(GetExtension()->id(), false);
  EXPECT_EQ(1, observer.closing_count());

  model->RemoveObserver(&observer);

  // There should only be one tab now.
  ASSERT_EQ(1, browser()->tab_count());
}

IN_PROC_BROWSER_TEST_F(BrowserTest, AppTabRemovedWhenExtensionUninstalled) {
  PhantomTabTest();

  Extension* extension = GetExtension();
  UninstallExtension(extension->id());

  // The uninstall should have removed the tab.
  ASSERT_EQ(1, browser()->tab_count());
}
#endif  // !defined(OS_MACOSX)

// Tests that the CLD (Compact Language Detection) works properly.
// Flaky, http://crbug.com/42095.
IN_PROC_BROWSER_TEST_F(BrowserTest, FLAKY_PageLanguageDetection) {
  static const wchar_t kDocRoot[] = L"chrome/test/data";
  scoped_refptr<HTTPTestServer> server(
        HTTPTestServer::CreateServer(kDocRoot, NULL));
  ASSERT_TRUE(NULL != server.get());

  TabContents* current_tab = browser()->GetSelectedTabContents();

  // Navigate to a page in English.
  ui_test_utils::WindowedNotificationObserverWithDetails<TabContents,
                                                         std::string>
      en_language_detected_signal(NotificationType::TAB_LANGUAGE_DETERMINED,
                                  current_tab);
  ui_test_utils::NavigateToURL(
      browser(), GURL(server->TestServerPage("files/english_page.html")));
  EXPECT_TRUE(current_tab->language_state().original_language().empty());
  en_language_detected_signal.Wait();
  std::string lang;
  EXPECT_TRUE(en_language_detected_signal.GetDetailsFor(current_tab, &lang));
  EXPECT_EQ("en", lang);
  EXPECT_EQ("en", current_tab->language_state().original_language());

  // Now navigate to a page in French.
  ui_test_utils::WindowedNotificationObserverWithDetails<TabContents,
                                                         std::string>
      fr_language_detected_signal(NotificationType::TAB_LANGUAGE_DETERMINED,
                                  current_tab);
  ui_test_utils::NavigateToURL(
      browser(), GURL(server->TestServerPage("files/french_page.html")));
  EXPECT_TRUE(current_tab->language_state().original_language().empty());
  fr_language_detected_signal.Wait();
  lang.clear();
  EXPECT_TRUE(fr_language_detected_signal.GetDetailsFor(current_tab, &lang));
  EXPECT_EQ("fr", lang);
  EXPECT_EQ("fr", current_tab->language_state().original_language());
}

// Chromeos defaults to restoring the last session, so this test isn't
// applicable.
#if !defined(OS_CHROMEOS)
#if defined(OS_MACOSX)
// Crashy, http://crbug.com/38522
#define RestorePinnedTabs DISABLED_RestorePinnedTabs
#endif
// Makes sure pinned tabs are restored correctly on start.
IN_PROC_BROWSER_TEST_F(BrowserTest, RestorePinnedTabs) {
  HTTPTestServer* server = StartHTTPServer();
  ASSERT_TRUE(server);

  // Add an pinned app tab.
  host_resolver()->AddRule("www.example.com", "127.0.0.1");
  GURL url(server->TestServerPage("empty.html"));
  TabStripModel* model = browser()->tabstrip_model();
  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII("app/")));
  Extension* extension_app = GetExtension();
  ui_test_utils::NavigateToURL(browser(), url);
  TabContents* app_contents = new TabContents(browser()->profile(), NULL,
                                              MSG_ROUTING_NONE, NULL);
  app_contents->SetExtensionApp(extension_app);
  model->AddTabContents(app_contents, 0, false, 0, false);
  model->SetTabPinned(0, true);
  ui_test_utils::NavigateToURL(browser(), url);

  // Add a non pinned tab.
  browser()->NewTab();

  // Add a pinned non-app tab.
  browser()->NewTab();
  ui_test_utils::NavigateToURL(browser(), GURL("about:blank"));
  model->SetTabPinned(2, true);

  // Write out the pinned tabs.
  PinnedTabCodec::WritePinnedTabs(browser()->profile());

  // Simulate launching again.
  CommandLine dummy(CommandLine::ARGUMENTS_ONLY);
  BrowserInit::LaunchWithProfile launch(std::wstring(), dummy);
  launch.profile_ = browser()->profile();
  launch.ProcessStartupURLs(std::vector<GURL>());

  // The launch should have created a new browser.
  ASSERT_EQ(2u, BrowserList::GetBrowserCount(browser()->profile()));

  // Find the new browser.
  Browser* new_browser = NULL;
  for (BrowserList::const_iterator i = BrowserList::begin();
       i != BrowserList::end() && !new_browser; ++i) {
    if (*i != browser())
      new_browser = *i;
  }
  ASSERT_TRUE(new_browser);
  ASSERT_TRUE(new_browser != browser());

  // We should get back an additional tab for the app.
  ASSERT_EQ(2, new_browser->tab_count());

  // Make sure the state matches.
  TabStripModel* new_model = new_browser->tabstrip_model();
  EXPECT_TRUE(new_model->IsAppTab(0));
  EXPECT_FALSE(new_model->IsAppTab(1));

  EXPECT_TRUE(new_model->IsTabPinned(0));
  EXPECT_TRUE(new_model->IsTabPinned(1));

  EXPECT_TRUE(new_model->GetTabContentsAt(0)->extension_app() ==
              extension_app);
}
#endif  // !defined(OS_CHROMEOS)

class BrowserAppRefocusTest : public ExtensionBrowserTest {
 public:
  BrowserAppRefocusTest(): server_(NULL),
                           extension_app_(NULL),
                           profile_(NULL) {}

 protected:
  virtual void SetUpCommandLine(CommandLine* command_line) {
    ExtensionBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kEnableExtensionApps);
  }

  // Common setup for all tests.  Can't use SetUpInProcessBrowserTestFixture
  // because starting the http server crashes if called from that function.
  // The IO thread is not set up at that point.
  virtual void SetUpExtensionApp() {
    server_ = StartHTTPServer();
    ASSERT_TRUE(server_);
    host_resolver()->AddRule("www.example.com", "127.0.0.1");
    url_ = GURL(server_->TestServerPage("empty.html"));

    profile_ = browser()->profile();
    ASSERT_TRUE(profile_);

    ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII("app/")));

    // Save a pointer to the loaded extension in |extension_app_|.
    const ExtensionList* extensions =
        profile_->GetExtensionsService()->extensions();

    for (size_t i = 0; i < extensions->size(); ++i) {
      if ((*extensions)[i]->name() == "App Test")
        extension_app_ =(*extensions)[i];
    }
    ASSERT_TRUE(extension_app_) << "App Test extension not loaded.";
  }

  HTTPTestServer* server_;
  Extension* extension_app_;
  Profile* profile_;
  GURL url_;
};

#if defined(OS_WINDOWS)

#define MAYBE_OpenTab OpenTab
#define MAYBE_OpenPanel OpenPanel
#define MAYBE_OpenWindow OpenWindow
#define MAYBE_WindowBeforeTab WindowBeforeTab
#define MAYBE_PanelBeforeTab PanelBeforeTab
#define MAYBE_TabInFocusedWindow TabInFocusedWindow

#else

// Crashes on mac involving app panels: http://crbug.com/42865

// ChromeOS doesn't open extension based app windows correctly yet:
// http://crbug.com/43061
#define MAYBE_OpenTab DISABLED_OpenTab
#define MAYBE_OpenPanel DISABLED_OpenPanel
#define MAYBE_OpenWindow DISABLED_OpenWindow
#define MAYBE_WindowBeforeTab DISABLED_WindowBeforeTab
#define MAYBE_PanelBeforeTab DISABLED_PanelBeforeTab
#define MAYBE_TabInFocusedWindow DISABLED_TabInFocusedWindow

#endif

// Test that launching an app refocuses a tab already hosting the app.
IN_PROC_BROWSER_TEST_F(BrowserAppRefocusTest, MAYBE_OpenTab) {
  SetUpExtensionApp();

  ui_test_utils::NavigateToURL(browser(), url_);
  ASSERT_EQ(1, browser()->tab_count());

  // Open a tab with the app.
  Browser::OpenApplicationTab(profile_, extension_app_);
  ASSERT_TRUE(ui_test_utils::WaitForNavigationInCurrentTab(browser()));
  ASSERT_EQ(2, browser()->tab_count());
  int app_tab_index = browser()->selected_index();
  ASSERT_EQ(0, app_tab_index) << "App tab should be the left most tab.";

  // Open the same app.  The existing tab should stay focused.
  Browser::OpenApplication(profile_, extension_app_->id());
  ASSERT_TRUE(ui_test_utils::WaitForNavigationInCurrentTab(browser()));
  ASSERT_EQ(2, browser()->tab_count());
  ASSERT_EQ(app_tab_index, browser()->selected_index());

  // Focus the other tab, and reopen the app. The existing tab should
  // be refocused.
  browser()->SelectTabContentsAt(1, false);
  Browser::OpenApplication(profile_, extension_app_->id());
  ASSERT_EQ(2, browser()->tab_count());
  ASSERT_EQ(app_tab_index, browser()->selected_index());
}

// Test that launching an app refocuses a panel running the app.
IN_PROC_BROWSER_TEST_F(BrowserAppRefocusTest, MAYBE_OpenPanel) {
  SetUpExtensionApp();

  ui_test_utils::NavigateToURL(browser(), url_);
  ASSERT_EQ(1, browser()->tab_count());

  // Open the app in a panel.
  Browser::OpenApplicationWindow(profile_, extension_app_,
                                 Extension::LAUNCH_PANEL, GURL());
  Browser* app_panel = BrowserList::GetLastActive();
  ASSERT_TRUE(app_panel);
  ASSERT_NE(app_panel, browser()) << "New browser should have opened.";
  ASSERT_EQ(app_panel, BrowserList::GetLastActive());

  // Focus the initial browser.
  browser()->window()->Show();
  ASSERT_EQ(browser(), BrowserList::GetLastActive());

  // Open the app.
  Browser::OpenApplication(profile_, extension_app_->id());

  // Focus should move to the panel.
  ASSERT_EQ(app_panel, BrowserList::GetLastActive());

  // No new tab should have been created in the initial browser.
  ASSERT_EQ(1, browser()->tab_count());
}

// Test that launching an app refocuses a window running the app.
IN_PROC_BROWSER_TEST_F(BrowserAppRefocusTest, MAYBE_OpenWindow) {
  SetUpExtensionApp();

  ui_test_utils::NavigateToURL(browser(), url_);
  ASSERT_EQ(1, browser()->tab_count());

  // Open a window with the app.
  Browser::OpenApplicationWindow(profile_, extension_app_,
                                 Extension::LAUNCH_WINDOW, GURL());
  Browser* app_window = BrowserList::GetLastActive();
  ASSERT_TRUE(app_window);
  ASSERT_NE(app_window, browser()) << "New browser should have opened.";

  // Focus the initial browser.
  browser()->window()->Show();
  ASSERT_EQ(browser(), BrowserList::GetLastActive());

  // Open the app.
  Browser::OpenApplication(profile_, extension_app_->id());

  // Focus should move to the window.
  ASSERT_EQ(app_window, BrowserList::GetLastActive());

  // No new tab should have been created in the initial browser.
  ASSERT_EQ(1, browser()->tab_count());
}

// Test that if an app is opened while running in a window and a tab,
// the window is focused.
IN_PROC_BROWSER_TEST_F(BrowserAppRefocusTest, MAYBE_WindowBeforeTab) {
  SetUpExtensionApp();

  ui_test_utils::NavigateToURL(browser(), url_);
  ASSERT_EQ(1, browser()->tab_count());

  // Open a tab with the app.
  Browser::OpenApplicationTab(profile_, extension_app_);
  ASSERT_TRUE(ui_test_utils::WaitForNavigationInCurrentTab(browser()));
  ASSERT_EQ(2, browser()->tab_count());
  int app_tab_index = browser()->selected_index();
  ASSERT_EQ(0, app_tab_index) << "App tab should be the left most tab.";

  // Open a window with the app.
  Browser::OpenApplicationWindow(profile_, extension_app_,
                                 Extension::LAUNCH_WINDOW, GURL());
  Browser* app_window = BrowserList::GetLastActive();
  ASSERT_TRUE(app_window);
  ASSERT_NE(app_window, browser()) << "New browser should have opened.";

  // Focus the initial browser.
  browser()->window()->Show();

  // Open the app.  Focus should move to the window.
  Browser::OpenApplication(profile_, extension_app_->id());
  ASSERT_EQ(app_window, BrowserList::GetLastActive());
}

// Test that if an app is opened while running in a panel and a tab,
// the panel is focused.
IN_PROC_BROWSER_TEST_F(BrowserAppRefocusTest, MAYBE_PanelBeforeTab) {
  SetUpExtensionApp();

  ui_test_utils::NavigateToURL(browser(), url_);
  ASSERT_EQ(1, browser()->tab_count());

  // Open a tab with the app.
  Browser::OpenApplicationTab(profile_, extension_app_);
  ASSERT_TRUE(ui_test_utils::WaitForNavigationInCurrentTab(browser()));
  ASSERT_EQ(2, browser()->tab_count());
  int app_tab_index = browser()->selected_index();
  ASSERT_EQ(0, app_tab_index) << "App tab should be the left most tab.";

  // Open a panel with the app.
  Browser::OpenApplicationWindow(profile_, extension_app_,
                                 Extension::LAUNCH_PANEL, GURL());
  Browser* app_panel = BrowserList::GetLastActive();
  ASSERT_TRUE(app_panel);
  ASSERT_NE(app_panel, browser()) << "New browser should have opened.";

  // Focus the initial browser.
  browser()->window()->Show();

  // Open the app.  Focus should move to the panel.
  Browser::OpenApplication(profile_, extension_app_->id());
  ASSERT_EQ(app_panel, BrowserList::GetLastActive());
}

// Test that if multiple tabs host an app, and that app is opened,
// the tab in the current window gets focus.
IN_PROC_BROWSER_TEST_F(BrowserAppRefocusTest, MAYBE_TabInFocusedWindow) {
  SetUpExtensionApp();

  ui_test_utils::NavigateToURL(browser(), url_);
  ASSERT_EQ(1, browser()->tab_count());

  Browser::OpenApplicationTab(profile_, extension_app_);
  ASSERT_TRUE(ui_test_utils::WaitForNavigationInCurrentTab(browser()));
  ASSERT_EQ(2, browser()->tab_count());
  int app_tab_index = browser()->selected_index();
  ASSERT_EQ(0, app_tab_index) << "App tab should be the left most tab.";

  // Open a new browser window, add an app tab.
  Browser* extra_browser = CreateBrowser(profile_);
  ASSERT_EQ(extra_browser, BrowserList::GetLastActive());

  Browser::OpenApplicationTab(profile_, extension_app_);
  ASSERT_TRUE(ui_test_utils::WaitForNavigationInCurrentTab(extra_browser));
  ASSERT_EQ(2, extra_browser->tab_count());
  app_tab_index = extra_browser->selected_index();
  ASSERT_EQ(0, app_tab_index) << "App tab should be the left most tab";

  // Open the app.  Focus should move to the panel.
  Browser::OpenApplication(profile_, extension_app_->id());
  ASSERT_EQ(extra_browser, BrowserList::GetLastActive());
  ASSERT_EQ(2, extra_browser->tab_count());

  browser()->window()->Show();
  Browser::OpenApplication(profile_, extension_app_->id());
  ASSERT_EQ(browser(), BrowserList::GetLastActive());
  ASSERT_EQ(2, browser()->tab_count());
}
