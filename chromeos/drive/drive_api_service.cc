// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/drive_api_service.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/message_loop_proxy.h"
#include "chrome/browser/google_apis/drive_api_operations.h"
#include "chrome/browser/google_apis/gdata_operations.h"
#include "chrome/browser/google_apis/gdata_util.h"
#include "chrome/browser/google_apis/operation_runner.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/net/url_util.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace drive {

namespace {

// OAuth2 scopes for Drive API.
const char kDriveScope[] = "https://www.googleapis.com/auth/drive";
const char kDriveAppsReadonlyScope[] =
    "https://www.googleapis.com/auth/drive.apps.readonly";

}  // namespace

DriveAPIService::DriveAPIService()
    : profile_(NULL),
      runner_(NULL) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

DriveAPIService::~DriveAPIService() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (runner_.get()) {
    runner_->operation_registry()->RemoveObserver(this);
    runner_->auth_service()->RemoveObserver(this);
  }
}

void DriveAPIService::Initialize(Profile* profile) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  profile_ = profile;

  std::vector<std::string> scopes;
  scopes.push_back(kDriveScope);
  scopes.push_back(kDriveAppsReadonlyScope);
  runner_.reset(new gdata::OperationRunner(profile, scopes));
  runner_->Initialize();

  runner_->auth_service()->AddObserver(this);
  runner_->operation_registry()->AddObserver(this);
}

void DriveAPIService::AddObserver(DriveServiceObserver* observer) {
  observers_.AddObserver(observer);
}

void DriveAPIService::RemoveObserver(DriveServiceObserver* observer) {
  observers_.RemoveObserver(observer);
}

bool DriveAPIService::CanStartOperation() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  return HasRefreshToken();
}

void DriveAPIService::CancelAll() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  runner_->CancelAll();
}

bool DriveAPIService::CancelForFilePath(const FilePath& file_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return operation_registry()->CancelForFilePath(file_path);
}

gdata::OperationProgressStatusList
DriveAPIService::GetProgressStatusList() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return operation_registry()->GetProgressStatusList();
}

void DriveAPIService::Authenticate(const gdata::AuthStatusCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  runner_->Authenticate(callback);
}

void DriveAPIService::GetDocuments(const GURL& url,
                                   int64 start_changestamp,
                                   const std::string& search_query,
                                   const std::string& directory_resource_id,
                                   const gdata::GetDataCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (search_query.empty())
    GetChangelist(url, start_changestamp, callback);
  else
    GetFilelist(url, search_query, callback);

  return;
  // TODO(kochi): Implement !directory_resource_id.empty() case.
  NOTREACHED();
}

void DriveAPIService::GetFilelist(const GURL& url,
                                  const std::string& search_query,
                                  const gdata::GetDataCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  runner_->StartOperationWithRetry(
      new gdata::GetFilelistOperation(operation_registry(),
                               url,
                               search_query,
                               callback));
}

void DriveAPIService::GetChangelist(const GURL& url,
                                    int64 start_changestamp,
                                    const gdata::GetDataCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  runner_->StartOperationWithRetry(
      new gdata::GetChangelistOperation(operation_registry(),
                                 url,
                                 start_changestamp,
                                 callback));
}

void DriveAPIService::GetDocumentEntry(const std::string& resource_id,
                                       const gdata::GetDataCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  runner_->StartOperationWithRetry(new gdata::GetFileOperation(
      operation_registry(),
      resource_id,
      callback));
}

void DriveAPIService::GetAccountMetadata(
    const gdata::GetDataCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  runner_->StartOperationWithRetry(
      new gdata::GetAboutOperation(operation_registry(), callback));
}

void DriveAPIService::GetApplicationInfo(
    const gdata::GetDataCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  runner_->StartOperationWithRetry(
      new gdata::GetApplistOperation(operation_registry(), callback));
}

void DriveAPIService::DownloadDocument(
    const FilePath& virtual_path,
    const FilePath& local_cache_path,
    const GURL& document_url,
    DocumentExportFormat format,
    const gdata::DownloadActionCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // TODO(kochi): Implement this.
  NOTREACHED();
}

void DriveAPIService::DownloadFile(
      const FilePath& virtual_path,
      const FilePath& local_cache_path,
      const GURL& document_url,
      const gdata::DownloadActionCallback& download_action_callback,
      const gdata::GetContentCallback& get_content_callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // TODO(kochi): Implement this.
  NOTREACHED();
}

void DriveAPIService::DeleteDocument(
    const GURL& document_url,
    const gdata::EntryActionCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // TODO(kochi): Implement this.
  NOTREACHED();
}

void DriveAPIService::CreateDirectory(
    const GURL& parent_content_url,
    const FilePath::StringType& directory_name,
    const gdata::GetDataCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // TODO(kochi): Implement this.
  NOTREACHED();
}

void DriveAPIService::CopyDocument(const std::string& resource_id,
                                   const FilePath::StringType& new_name,
                                   const gdata::GetDataCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // TODO(kochi): Implement this.
  NOTREACHED();
}

void DriveAPIService::RenameResource(
    const GURL& resource_url,
    const FilePath::StringType& new_name,
    const gdata::EntryActionCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // TODO(kochi): Implement this.
  NOTREACHED();
}

void DriveAPIService::AddResourceToDirectory(
    const GURL& parent_content_url,
    const GURL& resource_url,
    const gdata::EntryActionCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // TODO(kochi): Implement this.
  NOTREACHED();
}

void DriveAPIService::RemoveResourceFromDirectory(
    const GURL& parent_content_url,
    const GURL& resource_url,
    const std::string& resource_id,
    const gdata::EntryActionCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // TODO(kochi): Implement this.
  NOTREACHED();
}

void DriveAPIService::InitiateUpload(
    const gdata::InitiateUploadParams& params,
    const gdata::InitiateUploadCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // TODO(kochi): Implement this.
  NOTREACHED();
}

void DriveAPIService::ResumeUpload(
    const gdata::ResumeUploadParams& params,
    const gdata::ResumeUploadCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // TODO(kochi): Implement this.
  NOTREACHED();
}

void DriveAPIService::AuthorizeApp(const GURL& resource_url,
                                   const std::string& app_ids,
                                   const gdata::GetDataCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // TODO(kochi): Implement this.
  NOTREACHED();
}

bool DriveAPIService::HasAccessToken() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  return runner_->auth_service()->HasAccessToken();
}

bool DriveAPIService::HasRefreshToken() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  return runner_->auth_service()->HasRefreshToken();
}

gdata::OperationRegistry* DriveAPIService::operation_registry() const {
  return runner_->operation_registry();
}

void DriveAPIService::OnOAuth2RefreshTokenChanged() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (CanStartOperation()) {
    FOR_EACH_OBSERVER(
        DriveServiceObserver, observers_, OnReadyToPerformOperations());
  }
}

void DriveAPIService::OnProgressUpdate(
    const gdata::OperationProgressStatusList& list) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  FOR_EACH_OBSERVER(
      DriveServiceObserver, observers_, OnProgressUpdate(list));
}

void DriveAPIService::OnAuthenticationFailed(gdata::GDataErrorCode error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  FOR_EACH_OBSERVER(
      DriveServiceObserver, observers_, OnAuthenticationFailed(error));
}

}  // namespace drive
