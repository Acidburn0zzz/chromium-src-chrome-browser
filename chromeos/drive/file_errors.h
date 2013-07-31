// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DRIVE_FILE_ERRORS_H_
#define CHROME_BROWSER_CHROMEOS_DRIVE_FILE_ERRORS_H_

#include "base/callback_forward.h"
#include "base/platform_file.h"
#include "chrome/browser/google_apis/gdata_errorcode.h"

namespace drive {

enum FileError {
  FILE_ERROR_OK = 0,
  FILE_ERROR_FAILED = -1,
  FILE_ERROR_IN_USE = -2,
  FILE_ERROR_EXISTS = -3,
  FILE_ERROR_NOT_FOUND = -4,
  FILE_ERROR_ACCESS_DENIED = -5,
  FILE_ERROR_TOO_MANY_OPENED = -6,
  FILE_ERROR_NO_MEMORY = -7,
  FILE_ERROR_NO_SPACE = -8,
  FILE_ERROR_NOT_A_DIRECTORY = -9,
  FILE_ERROR_INVALID_OPERATION = -10,
  FILE_ERROR_SECURITY = -11,
  FILE_ERROR_ABORT = -12,
  FILE_ERROR_NOT_A_FILE = -13,
  FILE_ERROR_NOT_EMPTY = -14,
  FILE_ERROR_INVALID_URL = -15,
  FILE_ERROR_NO_CONNECTION = -16,
};

// Used as callbacks for file operations.
typedef base::Callback<void(FileError error)> FileOperationCallback;

// Returns a string representation of FileError.
std::string FileErrorToString(FileError error);

// Returns a PlatformFileError that corresponds to the FileError provided.
base::PlatformFileError FileErrorToPlatformError(FileError error);

// Converts GData error code into Drive file error code.
FileError GDataToFileError(google_apis::GDataErrorCode status);

}  // namespace drive

#endif  // CHROME_BROWSER_CHROMEOS_DRIVE_FILE_ERRORS_H_
