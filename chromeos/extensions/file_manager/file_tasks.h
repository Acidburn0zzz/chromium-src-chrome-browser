// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file provides utility functions for "file tasks".
//
// WHAT ARE FILE TASKS?
//
// File tasks are representatiosn of actions that can be performed over the
// currently selected files from Files.app. A task can be either of:
//
// 1) Chrome extension or app, registered via "file_handlers" or
// "file_browser_handlers" in manifest.json (ex. Text.app). This information
// comes from FileBrowserHandler::GetHandlers()
//
// See also:
// https://developer.chrome.com/extensions/manifest.html#file_handlers
// https://developer.chrome.com/extensions/fileBrowserHandler.html
//
// 2) Built-in handlers provided from Files.app. Files.app provides lots of
// file_browser_handlers, such as "play", "watch", "mount-archive".  These
// built-in handlers are often handled in special manners inside Files.app.
// This information also comes from FileBrowserHandler::GetHandlers().
//
// See also:
// chrome/browser/resources/file_manager/manifest.json
//
// 3) Drive app, which is a hosted app (i.e. just web site), that can work
// with Drive (ex. Pixlr Editor). This information comes from
// drive::DriveAppRegistry.
//
// See also:
// https://chrome.google.com/webstore/category/collection/drive_apps
//
// For example, if the user is now selecting a JPEG file, Files.app will
// receive file tasks represented as a JSON object via
// chrome.fileBrowserPrivate.getFileTasks() API, which look like:
//
// [
//   {
//     "driveApp": true,
//     "iconUrl": "<app_icon_url>",
//     "isDefault": false,
//     "taskId": "<drive_app_id>|drive|open-with",
//     "title": "Drive App Name (ex. Pixlr Editor)"
//   },
//   {
//     "driveApp": false,
//     "iconUrl": "chrome://extension-icon/hhaomjibdihmijegdhdafkllkbggdgoj/16/1",
//     "isDefault": true,
//     "taskId": "hhaomjibdihmijegdhdafkllkbggdgoj|file|gallery",
//     "title": "__MSG_OPEN_ACTION__"
//   }
// ]
//
// The first file task is a Drive app. The second file task is a built-in
// handler from Files.app.
//
// WHAT ARE TASK IDS?
//
// You may have noticed that "taskId" fields in the above example look
// awakard. Apparently "taskId" encodes three types of information delimited
// by "|". This is a weird format for something called as an ID.
//
// 1) Why are the three types information encoded in this way?
//
// It's just a historical reason. The reason is that a simple string can be
// easily stored in user's preferences. We should stop doing this, by storing
// this information in chrome.storage instead. crbug.com/267359.
//
// 2) OK, then what are the three types of information encoded here?
//
// The task ID encodes the folloing structure:
//
//     <app-id>|<task-type>|<task-action-id>
//
// <app-id> is either of Chrome Extension/App ID or Drive App ID. For some
// reason, Chrome Extension/App IDs and Drive App IDs look differently. As of
// writing, the fomer looks like "hhaomjibdihmijegdhdafkllkbggdgoj"
// (Files.app) and the latter looks like "419782477519" (Pixlr Editor).
//
// <task-type> is either of
// - "file" - File browser handler - app/extension declaring
//            "file_browser_handlers" in manifest.
// - "app" - File handler - app declaring "file_handlers" in manifest.json.
// - "drive" - Drive App
//
// <task-action-id> is an ID string used for identifying actions provided
// from a single Chrome Extension/App. In other words, a single
// Chrome/Extension can provide multiple file handlers hence each of them
// needs to have a unique action ID.  For Drive apps, <task-action-id> is
// always "open-with".
//
// HOW TASKS ARE EXECUTED?
//
// chrome.fileBrowserPrivate.viewFiles() is used to open a file in a browser,
// without any handler. Browser will take care of handling the file (ex. PDF).
//
// chrome.fileBrowserPrivate.executeTasks() is used to open a file with a
// handler (Chrome Extension/App or Drive App).
//
// Some built-in handlers such as "play" and "watch" are handled internally
// in Files.app. "mount-archive" is handled very differently. The task
// execution business should be simplified: crbug.com/267313
//
// See also:
// chrome/browser/resources/file_manager/js/file_tasks.js
//

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_MANAGER_FILE_TASKS_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_MANAGER_FILE_TASKS_H_

#include <set>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/callback_forward.h"
#include "chrome/browser/extensions/api/file_handlers/app_file_handler_util.h"
#include "url/gurl.h"

class PrefService;
class Profile;

namespace drive {
class DriveAppRegistry;
}

namespace fileapi {
class FileSystemURL;
}

namespace file_manager {
namespace file_tasks {

// Task types as explained in the comment above. Search for <task-type>.
enum TaskType {
  TASK_TYPE_FILE_BROWSER_HANDLER,
  TASK_TYPE_FILE_HANDLER,
  TASK_TYPE_DRIVE_APP,
  TASK_TYPE_UNKNOWN,  // Used only for handling errors.
};

// Describes a task.
// See the comment above for <app-id>, <task-type>, and <action-id>.
struct TaskDescriptor {
  TaskDescriptor(const std::string& in_app_id,
                 TaskType in_task_type,
                 const std::string& in_action_id)
      : app_id(in_app_id),
        task_type(in_task_type),
        action_id(in_action_id) {
  }
  TaskDescriptor() {
  }

  std::string app_id;
  TaskType task_type;
  std::string action_id;
};

// Describes a task with extra information such as icon URL.
class FullTaskDescriptor {
 public:
  FullTaskDescriptor(const TaskDescriptor& task_descriptor,
                     const std::string& task_title,
                     const GURL& icon_url,
                     bool is_default);
  const TaskDescriptor& task_descriptor() const { return task_descriptor_; }

  // The title of the task.
  const std::string& task_title() { return task_title_; }
  // The icon URL for the task (ex. app icon)
  const GURL& icon_url() const { return icon_url_; }

  // True if this task is set as default.
  bool is_default() const { return is_default_; }
  void set_is_default(bool is_default) { is_default_ = is_default; }

  // Returns a DictionaryValue representation, which looks like:
  //
  // {
  //   "driveApp": true,
  //   "iconUrl": "<app_icon_url>",
  //   "isDefault": false,
  //   "taskId": "<drive_app_id>|drive|open-with",
  //   "title": "Drive App Name (ex. Pixlr Editor)"
  // },
  //
  // "iconUrl" is omitted if icon_url_ is empty.
  //
  // This representation will be used to send task info to the JavaScript.
  scoped_ptr<base::DictionaryValue> AsDictionaryValue() const;

 private:
  TaskDescriptor task_descriptor_;
  std::string task_title_;
  GURL icon_url_;
  bool is_default_;
};

// Update the default file handler for the given sets of suffixes and MIME
// types.
void UpdateDefaultTask(PrefService* pref_service,
                       const std::string& task_id,
                       const std::set<std::string>& suffixes,
                       const std::set<std::string>& mime_types);

// Returns the task ID of the default task for the given |mime_type|/|suffix|
// combination. If it finds a MIME type match, then it prefers that over a
// suffix match. If it a default can't be found, then it returns the empty
// string.
std::string GetDefaultTaskIdFromPrefs(const PrefService& pref_service,
                                      const std::string& mime_type,
                                      const std::string& suffix);

// Generates task id for the task specified by |app_id|, |task_type| and
// |action_id|.
//
// |app_id| is either of Chrome Extension/App ID or Drive App ID.
// |action_id| is a free-form string ID for the action.
std::string MakeTaskID(const std::string& app_id,
                       TaskType task_type,
                       const std::string& action_id);

// Returns a task id for the Drive app with |app_id|.
// TODO(gspencer): For now, the action id is always "open-with", but we
// could add any actions that the drive app supports.
std::string MakeDriveAppTaskId(const std::string& app_id);

// Converts |task_descriptor| to a task ID.
std::string TaskDescriptorToId(const TaskDescriptor& task_descriptor);

// Parses the task ID and extracts app ID, task type, and action ID into
// |task|. On failure, returns false, and the contents of |task| are
// undefined.
//
// See also the comment at the beginning of the file for details for how
// "task_id" looks like.
bool ParseTaskID(const std::string& task_id, TaskDescriptor* task);

// The callback is used for ExecuteFileTask(). Will be called with true if
// the file task execution is successful, or false if unsuccessful.
typedef base::Callback<void(bool success)> FileTaskFinishedCallback;

// Executes file handler task for each element of |file_urls|.
// Returns |false| if the execution cannot be initiated. Otherwise returns
// |true| and then eventually calls |done| when all the files have been handled.
// |done| can be a null callback.
//
// Parameters:
// profile    - The profile used for making this function call.
// app_id     - The ID of the app requesting the file task execution.
// source_url - The source URL which originates this function call.
// tab_id     - The ID of the tab which originates this function call.
//              This can be 0 if no tab is associated.
// task       - See the comment at TaskDescriptor struct.
// file_urls  - URLs of the target files.
// done       - The callback which will be called on completion.
//              The callback won't be called if the function returns
//              false.
bool ExecuteFileTask(Profile* profile,
                     const GURL& source_url,
                     const std::string& app_id,
                     int32 tab_id,
                     const TaskDescriptor& task,
                     const std::vector<fileapi::FileSystemURL>& file_urls,
                     const FileTaskFinishedCallback& done);

typedef extensions::app_file_handler_util::PathAndMimeTypeSet
    PathAndMimeTypeSet;

// Holds fields to build a task result.
struct TaskInfo;

// Map from a task id to TaskInfo.
typedef std::map<std::string, TaskInfo> TaskInfoMap;

// Looks up available apps for each file in |path_mime_set| in the
// |registry|, and returns the intersection of all available apps as a
// map from task id to TaskInfo.
void GetAvailableDriveTasks(const drive::DriveAppRegistry& registry,
                            const PathAndMimeTypeSet& path_mime_set,
                            TaskInfoMap* task_info_map);

// Creates a list of each task in |task_info_map| and stores the result into
// |result_list|. If a default task is set in the result list,
// |default_already_set| is set to true.
void CreateDriveTasks(const TaskInfoMap& task_info_map,
                      std::vector<FullTaskDescriptor>* result_list);

// Finds the drive app tasks that can be used with the given files, and
// append them to the |result_list|.
//
// "taskId" field in |result_list| will look like
// "<drive-app-id>|drive|open-with" (See also file_tasks.h).
// "driveApp" field in |result_list| will be set to "true".
void FindDriveAppTasks(Profile* profile,
                       const PathAndMimeTypeSet& path_mime_set,
                       std::vector<FullTaskDescriptor>* result_list);

// Finds the file handler tasks (apps declaring "file_handlers" in
// manifest.json) that can be used with the given files, appending them to
// the |result_list|.
void FindFileHandlerTasks(Profile* profile,
                          const PathAndMimeTypeSet& path_mime_set,
                          std::vector<FullTaskDescriptor>* result_list);

// Finds the file browser handler tasks (app/extensions declaring
// "file_browser_handlers" in manifest.json) that can be used with the
// given files, appending them to the |result_list|.
void FindFileBrowserHandlerTasks(
    Profile* profile,
    const std::vector<GURL>& file_urls,
    const std::vector<base::FilePath>& file_paths,
    std::vector<FullTaskDescriptor>* result_list);

// Finds all types (drive, file handlers, file browser handlers) of
// tasks. See the comment at FindDriveAppTasks() about |result_list|.
void FindAllTypesOfTasks(
    Profile* profile,
    const PathAndMimeTypeSet& path_mime_set,
    const std::vector<GURL>& file_urls,
    const std::vector<base::FilePath>& file_paths,
    std::vector<FullTaskDescriptor>* result_list);

// Chooses the default task in |tasks| and sets it as default, if the default
// task is found (i.e. the default task may not exist in |tasks|). No tasks
// should be set as default before calling this function.
void ChooseAndSetDefaultTask(const PrefService& pref_service,
                             const PathAndMimeTypeSet& path_mime_set,
                             std::vector<FullTaskDescriptor>* tasks);

}  // namespace file_tasks
}  // namespace file_manager

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_MANAGER_FILE_TASKS_H_
