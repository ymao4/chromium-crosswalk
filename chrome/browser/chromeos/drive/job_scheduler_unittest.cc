// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/job_scheduler.h"

#include <set>

#include "base/bind.h"
#include "base/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/prefs/testing_pref_service.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/chromeos/drive/test_util.h"
#include "chrome/browser/drive/fake_drive_service.h"
#include "chrome/browser/google_apis/drive_api_parser.h"
#include "chrome/browser/google_apis/gdata_wapi_parser.h"
#include "chrome/browser/google_apis/test_util.h"
#include "chrome/common/pref_names.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace drive {

namespace {

// Dummy value passed for the |expected_file_size| parameter of DownloadFile().
const int64 kDummyDownloadFileSize = 0;

void CopyTitleFromGetResourceEntryCallback(
    std::vector<std::string>* title_list_out,
    google_apis::GDataErrorCode error_in,
    scoped_ptr<google_apis::ResourceEntry> resource_entry_in) {
  title_list_out->push_back(resource_entry_in->title());
}

class JobListLogger : public JobListObserver {
 public:
  enum EventType {
    ADDED,
    UPDATED,
    DONE,
  };

  struct EventLog {
    EventType type;
    JobInfo info;

    EventLog(EventType type, const JobInfo& info) : type(type), info(info) {
    }
  };

  // Checks whether the specified type of event has occurred.
  bool Has(EventType type, JobType job_type) {
    for (size_t i = 0; i < events.size(); ++i) {
      if (events[i].type == type && events[i].info.job_type == job_type)
        return true;
    }
    return false;
  }

  // Gets the progress event information of the specified type.
  void GetProgressInfo(JobType job_type, std::vector<int64>* progress) {
    for (size_t i = 0; i < events.size(); ++i) {
      if (events[i].type == UPDATED && events[i].info.job_type == job_type)
        progress->push_back(events[i].info.num_completed_bytes);
    }
  }

  // JobListObserver overrides.
  virtual void OnJobAdded(const JobInfo& info) OVERRIDE {
    events.push_back(EventLog(ADDED, info));
  }

  virtual void OnJobUpdated(const JobInfo& info) OVERRIDE {
    events.push_back(EventLog(UPDATED, info));
  }

  virtual void OnJobDone(const JobInfo& info, FileError error) OVERRIDE {
    events.push_back(EventLog(DONE, info));
  }

 private:
  std::vector<EventLog> events;
};

// Fake drive service extended for testing cancellation.
// When upload_new_file_cancelable is set, this Drive service starts
// returning a closure to cancel from InitiateUploadNewFile(). The task will
// finish only when the cancel closure is called.
class CancelTestableFakeDriveService : public FakeDriveService {
 public:
  CancelTestableFakeDriveService()
      : upload_new_file_cancelable_(false) {
  }

  void set_upload_new_file_cancelable(bool cancelable) {
    upload_new_file_cancelable_ = cancelable;
  }

  virtual google_apis::CancelCallback InitiateUploadNewFile(
      const std::string& content_type,
      int64 content_length,
      const std::string& parent_resource_id,
      const std::string& title,
      const google_apis::InitiateUploadCallback& callback) OVERRIDE {
    if (upload_new_file_cancelable_)
      return base::Bind(callback, google_apis::GDATA_CANCELLED, GURL());

    return FakeDriveService::InitiateUploadNewFile(content_type,
                                                   content_length,
                                                   parent_resource_id,
                                                   title,
                                                   callback);
  }

 private:
  bool upload_new_file_cancelable_;
};

}  // namespace

class JobSchedulerTest : public testing::Test {
 public:
  JobSchedulerTest()
      : pref_service_(new TestingPrefServiceSimple) {
    test_util::RegisterDrivePrefs(pref_service_->registry());
  }

  virtual void SetUp() OVERRIDE {
    fake_network_change_notifier_.reset(
        new test_util::FakeNetworkChangeNotifier);

    fake_drive_service_.reset(new CancelTestableFakeDriveService);
    fake_drive_service_->LoadResourceListForWapi(
        "gdata/root_feed.json");
    fake_drive_service_->LoadAccountMetadataForWapi(
        "gdata/account_metadata.json");
    fake_drive_service_->LoadAppListForDriveApi(
        "drive/applist.json");

    scheduler_.reset(new JobScheduler(pref_service_.get(),
                                      fake_drive_service_.get(),
                                      base::MessageLoopProxy::current().get()));
    scheduler_->SetDisableThrottling(true);
  }

 protected:
  // Sets up FakeNetworkChangeNotifier as if it's connected to a network with
  // the specified connection type.
  void ChangeConnectionType(net::NetworkChangeNotifier::ConnectionType type) {
    fake_network_change_notifier_->SetConnectionType(type);
  }

  // Sets up FakeNetworkChangeNotifier as if it's connected to wifi network.
  void ConnectToWifi() {
    ChangeConnectionType(net::NetworkChangeNotifier::CONNECTION_WIFI);
  }

  // Sets up FakeNetworkChangeNotifier as if it's connected to cellular network.
  void ConnectToCellular() {
    ChangeConnectionType(net::NetworkChangeNotifier::CONNECTION_2G);
  }

  // Sets up FakeNetworkChangeNotifier as if it's connected to wimax network.
  void ConnectToWimax() {
    ChangeConnectionType(net::NetworkChangeNotifier::CONNECTION_4G);
  }

  // Sets up FakeNetworkChangeNotifier as if it's disconnected.
  void ConnectToNone() {
    ChangeConnectionType(net::NetworkChangeNotifier::CONNECTION_NONE);
  }

  static int GetMetadataQueueMaxJobCount() {
    return JobScheduler::kMaxJobCount[JobScheduler::METADATA_QUEUE];
  }

  content::TestBrowserThreadBundle thread_bundle_;
  scoped_ptr<TestingPrefServiceSimple> pref_service_;
  scoped_ptr<test_util::FakeNetworkChangeNotifier>
      fake_network_change_notifier_;
  scoped_ptr<CancelTestableFakeDriveService> fake_drive_service_;
  scoped_ptr<JobScheduler> scheduler_;
};

TEST_F(JobSchedulerTest, GetAboutResource) {
  ConnectToWifi();

  google_apis::GDataErrorCode error = google_apis::GDATA_OTHER_ERROR;
  scoped_ptr<google_apis::AboutResource> about_resource;
  scheduler_->GetAboutResource(
      google_apis::test_util::CreateCopyResultCallback(
          &error, &about_resource));
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(google_apis::HTTP_SUCCESS, error);
  ASSERT_TRUE(about_resource);
}

TEST_F(JobSchedulerTest, GetAppList) {
  ConnectToWifi();

  google_apis::GDataErrorCode error = google_apis::GDATA_OTHER_ERROR;
  scoped_ptr<google_apis::AppList> app_list;

  scheduler_->GetAppList(
      google_apis::test_util::CreateCopyResultCallback(&error, &app_list));
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(google_apis::HTTP_SUCCESS, error);
  ASSERT_TRUE(app_list);
}

TEST_F(JobSchedulerTest, GetAllResourceList) {
  ConnectToWifi();

  google_apis::GDataErrorCode error = google_apis::GDATA_OTHER_ERROR;
  scoped_ptr<google_apis::ResourceList> resource_list;

  scheduler_->GetAllResourceList(
      google_apis::test_util::CreateCopyResultCallback(
          &error, &resource_list));
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(google_apis::HTTP_SUCCESS, error);
  ASSERT_TRUE(resource_list);
}

TEST_F(JobSchedulerTest, GetResourceListInDirectory) {
  ConnectToWifi();

  google_apis::GDataErrorCode error = google_apis::GDATA_OTHER_ERROR;
  scoped_ptr<google_apis::ResourceList> resource_list;

  scheduler_->GetResourceListInDirectory(
      fake_drive_service_->GetRootResourceId(),
      google_apis::test_util::CreateCopyResultCallback(
          &error, &resource_list));
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(google_apis::HTTP_SUCCESS, error);
  ASSERT_TRUE(resource_list);
}

TEST_F(JobSchedulerTest, Search) {
  ConnectToWifi();

  google_apis::GDataErrorCode error = google_apis::GDATA_OTHER_ERROR;
  scoped_ptr<google_apis::ResourceList> resource_list;

  scheduler_->Search(
      "File",  // search query
      google_apis::test_util::CreateCopyResultCallback(
          &error, &resource_list));
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(google_apis::HTTP_SUCCESS, error);
  ASSERT_TRUE(resource_list);
}

TEST_F(JobSchedulerTest, GetChangeList) {
  ConnectToWifi();

  google_apis::GDataErrorCode error = google_apis::GDATA_OTHER_ERROR;

  // Create a new directory.
  // The loaded (initial) changestamp is 654321. Thus, by this operation,
  // it should become 654322.
  {
    scoped_ptr<google_apis::ResourceEntry> resource_entry;
    fake_drive_service_->AddNewDirectory(
        fake_drive_service_->GetRootResourceId(),
        "new directory",
        google_apis::test_util::CreateCopyResultCallback(
            &error, &resource_entry));
    base::RunLoop().RunUntilIdle();
    ASSERT_EQ(google_apis::HTTP_CREATED, error);
  }

  error = google_apis::GDATA_OTHER_ERROR;
  scoped_ptr<google_apis::ResourceList> resource_list;
  scheduler_->GetChangeList(
      654321 + 1,  // start_changestamp
      google_apis::test_util::CreateCopyResultCallback(
          &error, &resource_list));
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(google_apis::HTTP_SUCCESS, error);
  ASSERT_TRUE(resource_list);
}

TEST_F(JobSchedulerTest, GetRemainingChangeList) {
  ConnectToWifi();
  fake_drive_service_->set_default_max_results(2);

  google_apis::GDataErrorCode error = google_apis::GDATA_OTHER_ERROR;
  scoped_ptr<google_apis::ResourceList> resource_list;

  scheduler_->GetAllResourceList(
      google_apis::test_util::CreateCopyResultCallback(
          &error, &resource_list));
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(google_apis::HTTP_SUCCESS, error);
  ASSERT_TRUE(resource_list);

  const google_apis::Link* next_link =
      resource_list->GetLinkByType(google_apis::Link::LINK_NEXT);
  ASSERT_TRUE(next_link);
  // Keep the next url before releasing the |resource_list|.
  GURL next_url(next_link->href());

  error = google_apis::GDATA_OTHER_ERROR;
  resource_list.reset();

  scheduler_->GetRemainingChangeList(
      next_url,
      google_apis::test_util::CreateCopyResultCallback(
          &error, &resource_list));
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(google_apis::HTTP_SUCCESS, error);
  ASSERT_TRUE(resource_list);
}

TEST_F(JobSchedulerTest, GetRemainingFileList) {
  ConnectToWifi();
  fake_drive_service_->set_default_max_results(2);

  google_apis::GDataErrorCode error = google_apis::GDATA_OTHER_ERROR;
  scoped_ptr<google_apis::ResourceList> resource_list;

  scheduler_->GetResourceListInDirectory(
      fake_drive_service_->GetRootResourceId(),
      google_apis::test_util::CreateCopyResultCallback(
          &error, &resource_list));
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(google_apis::HTTP_SUCCESS, error);
  ASSERT_TRUE(resource_list);

  const google_apis::Link* next_link =
      resource_list->GetLinkByType(google_apis::Link::LINK_NEXT);
  ASSERT_TRUE(next_link);
  // Keep the next url before releasing the |resource_list|.
  GURL next_url(next_link->href());

  error = google_apis::GDATA_OTHER_ERROR;
  resource_list.reset();

  scheduler_->GetRemainingFileList(
      next_url,
      google_apis::test_util::CreateCopyResultCallback(
          &error, &resource_list));
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(google_apis::HTTP_SUCCESS, error);
  ASSERT_TRUE(resource_list);
}

TEST_F(JobSchedulerTest, GetShareUrl) {
  ConnectToWifi();

  google_apis::GDataErrorCode error = google_apis::GDATA_OTHER_ERROR;
  GURL share_url;

  scheduler_->GetShareUrl(
      "file:2_file_resource_id",  // resource ID
      GURL("chrome-extension://test-id/"), // embed origin
      ClientContext(USER_INITIATED),
      google_apis::test_util::CreateCopyResultCallback(&error, &share_url));
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(google_apis::HTTP_SUCCESS, error);
  ASSERT_FALSE(share_url.is_empty());
}

TEST_F(JobSchedulerTest, DeleteResource) {
  ConnectToWifi();

  google_apis::GDataErrorCode error = google_apis::GDATA_OTHER_ERROR;

  scheduler_->DeleteResource(
      "file:2_file_resource_id",
      google_apis::test_util::CreateCopyResultCallback(&error));
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(google_apis::HTTP_SUCCESS, error);
}

TEST_F(JobSchedulerTest, CopyResource) {
  ConnectToWifi();

  google_apis::GDataErrorCode error = google_apis::GDATA_OTHER_ERROR;
  scoped_ptr<google_apis::ResourceEntry> entry;

  scheduler_->CopyResource(
      "file:2_file_resource_id",  // resource ID
      "folder:1_folder_resource_id",  // parent resource ID
      "New Document",  // new title
      base::Time(),
      google_apis::test_util::CreateCopyResultCallback(&error, &entry));
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(google_apis::HTTP_SUCCESS, error);
  ASSERT_TRUE(entry);
}

TEST_F(JobSchedulerTest, CopyHostedDocument) {
  ConnectToWifi();

  google_apis::GDataErrorCode error = google_apis::GDATA_OTHER_ERROR;
  scoped_ptr<google_apis::ResourceEntry> entry;

  scheduler_->CopyHostedDocument(
      "document:5_document_resource_id",  // resource ID
      "New Document",  // new title
      google_apis::test_util::CreateCopyResultCallback(&error, &entry));
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(google_apis::HTTP_SUCCESS, error);
  ASSERT_TRUE(entry);
}

TEST_F(JobSchedulerTest, MoveResource) {
  ConnectToWifi();

  google_apis::GDataErrorCode error = google_apis::GDATA_OTHER_ERROR;
  scoped_ptr<google_apis::ResourceEntry> entry;

  scheduler_->MoveResource(
      "file:2_file_resource_id",  // resource ID
      "folder:1_folder_resource_id",  // parent resource ID
      "New Document",  // new title
      base::Time(),
      google_apis::test_util::CreateCopyResultCallback(&error, &entry));
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(google_apis::HTTP_SUCCESS, error);
  ASSERT_TRUE(entry);
}

TEST_F(JobSchedulerTest, RenameResource) {
  ConnectToWifi();

  google_apis::GDataErrorCode error = google_apis::GDATA_OTHER_ERROR;

  scheduler_->RenameResource(
      "file:2_file_resource_id",
      "New Title",
      google_apis::test_util::CreateCopyResultCallback(&error));
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(google_apis::HTTP_SUCCESS, error);
}

TEST_F(JobSchedulerTest, AddResourceToDirectory) {
  ConnectToWifi();

  google_apis::GDataErrorCode error = google_apis::GDATA_OTHER_ERROR;

  scheduler_->AddResourceToDirectory(
      "folder:1_folder_resource_id",
      "file:2_file_resource_id",
      google_apis::test_util::CreateCopyResultCallback(&error));
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(google_apis::HTTP_SUCCESS, error);
}

TEST_F(JobSchedulerTest, RemoveResourceFromDirectory) {
  ConnectToWifi();

  google_apis::GDataErrorCode error = google_apis::GDATA_OTHER_ERROR;

  scheduler_->RemoveResourceFromDirectory(
      "folder:1_folder_resource_id",
      "file:subdirectory_file_1_id",  // resource ID
      google_apis::test_util::CreateCopyResultCallback(&error));
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(google_apis::HTTP_SUCCESS, error);
}

TEST_F(JobSchedulerTest, AddNewDirectory) {
  ConnectToWifi();

  google_apis::GDataErrorCode error = google_apis::GDATA_OTHER_ERROR;
  scoped_ptr<google_apis::ResourceEntry> entry;

  scheduler_->AddNewDirectory(
      fake_drive_service_->GetRootResourceId(),  // Root directory.
      "New Directory",
      google_apis::test_util::CreateCopyResultCallback(&error, &entry));
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(google_apis::HTTP_CREATED, error);
  ASSERT_TRUE(entry);
}

TEST_F(JobSchedulerTest, PriorityHandling) {
  const base::FilePath kDummyFilePath(FILE_PATH_LITERAL("dummy"));

  // Saturate the metadata job queue with uninteresting jobs to prevent
  // following jobs from starting.
  google_apis::GDataErrorCode error_dontcare = google_apis::GDATA_OTHER_ERROR;
  scoped_ptr<google_apis::ResourceEntry> entry_dontcare;
  for (int i = 0; i < GetMetadataQueueMaxJobCount(); ++i) {
    scheduler_->CreateFile(
        fake_drive_service_->GetRootResourceId(),
        kDummyFilePath,
        base::StringPrintf("uninteresting file %d", i),
        "text/plain",
        ClientContext(USER_INITIATED),
        google_apis::test_util::CreateCopyResultCallback(&error_dontcare,
                                                         &entry_dontcare));
  }

  // Start jobs with different priorities.
  std::string title_1("new file 1");
  std::string title_2("new file 2");
  std::string title_3("new file 3");
  std::string title_4("new file 4");
  std::vector<std::string> titles;

  scheduler_->CreateFile(
      fake_drive_service_->GetRootResourceId(),
      kDummyFilePath,
      title_1,
      "text/plain",
      ClientContext(USER_INITIATED),
      base::Bind(&CopyTitleFromGetResourceEntryCallback, &titles));
  scheduler_->CreateFile(
      fake_drive_service_->GetRootResourceId(),
      kDummyFilePath,
      title_2,
      "text/plain",
      ClientContext(BACKGROUND),
      base::Bind(&CopyTitleFromGetResourceEntryCallback, &titles));
  scheduler_->CreateFile(
      fake_drive_service_->GetRootResourceId(),
      kDummyFilePath,
      title_3,
      "text/plain",
      ClientContext(BACKGROUND),
      base::Bind(&CopyTitleFromGetResourceEntryCallback, &titles));
  scheduler_->CreateFile(
      fake_drive_service_->GetRootResourceId(),
      kDummyFilePath,
      title_4,
      "text/plain",
      ClientContext(USER_INITIATED),
      base::Bind(&CopyTitleFromGetResourceEntryCallback, &titles));

  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(4ul, titles.size());
  EXPECT_EQ(title_1, titles[0]);
  EXPECT_EQ(title_4, titles[1]);
  EXPECT_EQ(title_2, titles[2]);
  EXPECT_EQ(title_3, titles[3]);
}

TEST_F(JobSchedulerTest, NoConnectionUserInitiated) {
  ConnectToNone();

  google_apis::GDataErrorCode error = google_apis::GDATA_OTHER_ERROR;
  scoped_ptr<google_apis::ResourceEntry> entry;
  scheduler_->CreateFile(
      fake_drive_service_->GetRootResourceId(),
      base::FilePath(FILE_PATH_LITERAL("dummy")),
      "title",
      "text/plain",
      ClientContext(USER_INITIATED),
      google_apis::test_util::CreateCopyResultCallback(&error, &entry));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(google_apis::GDATA_NO_CONNECTION, error);
}

TEST_F(JobSchedulerTest, NoConnectionBackground) {
  ConnectToNone();

  std::string resource_id("file:2_file_resource_id");

  google_apis::GDataErrorCode error = google_apis::GDATA_OTHER_ERROR;
  scoped_ptr<google_apis::ResourceEntry> entry;
  scheduler_->CreateFile(
      fake_drive_service_->GetRootResourceId(),
      base::FilePath(FILE_PATH_LITERAL("dummy")),
      "title",
      "text/plain",
      ClientContext(BACKGROUND),
      google_apis::test_util::CreateCopyResultCallback(&error, &entry));
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(entry);

  // Reconnect to the net.
  ConnectToWifi();

  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(google_apis::HTTP_SUCCESS, error);
  ASSERT_TRUE(entry);
  EXPECT_EQ("title", entry->title());
}

TEST_F(JobSchedulerTest, DownloadFileCellularDisabled) {
  ConnectToCellular();

  // Disable fetching over cellular network.
  pref_service_->SetBoolean(prefs::kDisableDriveOverCellular, true);

  // Try to get a file in the background
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  const base::FilePath kOutputFilePath =
      temp_dir.path().AppendASCII("whatever.txt");
  google_apis::GDataErrorCode download_error = google_apis::GDATA_OTHER_ERROR;
  base::FilePath output_file_path;
  scheduler_->DownloadFile(
      base::FilePath::FromUTF8Unsafe("drive/whatever.txt"),  // virtual path
      kDummyDownloadFileSize,
      kOutputFilePath,
      "file:2_file_resource_id",
      ClientContext(BACKGROUND),
      google_apis::test_util::CreateCopyResultCallback(
          &download_error, &output_file_path),
      google_apis::GetContentCallback());
  // Metadata should still work
  google_apis::GDataErrorCode metadata_error = google_apis::GDATA_OTHER_ERROR;
  scoped_ptr<google_apis::AboutResource> about_resource;

  // Try to get the metadata
  scheduler_->GetAboutResource(
      google_apis::test_util::CreateCopyResultCallback(
          &metadata_error, &about_resource));
  base::RunLoop().RunUntilIdle();

  // Check the metadata
  ASSERT_EQ(google_apis::HTTP_SUCCESS, metadata_error);
  ASSERT_TRUE(about_resource);

  // Check the download
  EXPECT_EQ(google_apis::GDATA_OTHER_ERROR, download_error);

  // Switch to a Wifi connection
  ConnectToWifi();

  base::RunLoop().RunUntilIdle();

  // Check the download again
  EXPECT_EQ(google_apis::HTTP_SUCCESS, download_error);
  std::string content;
  EXPECT_EQ(output_file_path, kOutputFilePath);
  ASSERT_TRUE(base::ReadFileToString(output_file_path, &content));
  EXPECT_EQ("This is some test content.", content);
}

TEST_F(JobSchedulerTest, DownloadFileWimaxDisabled) {
  ConnectToWimax();

  // Disable fetching over cellular network.
  pref_service_->SetBoolean(prefs::kDisableDriveOverCellular, true);

  // Try to get a file in the background
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  const base::FilePath kOutputFilePath =
      temp_dir.path().AppendASCII("whatever.txt");
  google_apis::GDataErrorCode download_error = google_apis::GDATA_OTHER_ERROR;
  base::FilePath output_file_path;
  scheduler_->DownloadFile(
      base::FilePath::FromUTF8Unsafe("drive/whatever.txt"),  // virtual path
      kDummyDownloadFileSize,
      kOutputFilePath,
      "file:2_file_resource_id",
      ClientContext(BACKGROUND),
      google_apis::test_util::CreateCopyResultCallback(
          &download_error, &output_file_path),
      google_apis::GetContentCallback());
  // Metadata should still work
  google_apis::GDataErrorCode metadata_error = google_apis::GDATA_OTHER_ERROR;
  scoped_ptr<google_apis::AboutResource> about_resource;

  // Try to get the metadata
  scheduler_->GetAboutResource(
      google_apis::test_util::CreateCopyResultCallback(
          &metadata_error, &about_resource));
  base::RunLoop().RunUntilIdle();

  // Check the metadata
  ASSERT_EQ(google_apis::HTTP_SUCCESS, metadata_error);
  ASSERT_TRUE(about_resource);

  // Check the download
  EXPECT_EQ(google_apis::GDATA_OTHER_ERROR, download_error);

  // Switch to a Wifi connection
  ConnectToWifi();

  base::RunLoop().RunUntilIdle();

  // Check the download again
  EXPECT_EQ(google_apis::HTTP_SUCCESS, download_error);
  std::string content;
  EXPECT_EQ(output_file_path, kOutputFilePath);
  ASSERT_TRUE(base::ReadFileToString(output_file_path, &content));
  EXPECT_EQ("This is some test content.", content);
}

TEST_F(JobSchedulerTest, DownloadFileCellularEnabled) {
  ConnectToCellular();

  // Enable fetching over cellular network.
  pref_service_->SetBoolean(prefs::kDisableDriveOverCellular, false);

  // Try to get a file in the background
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  const base::FilePath kOutputFilePath =
      temp_dir.path().AppendASCII("whatever.txt");
  google_apis::GDataErrorCode download_error = google_apis::GDATA_OTHER_ERROR;
  base::FilePath output_file_path;
  scheduler_->DownloadFile(
      base::FilePath::FromUTF8Unsafe("drive/whatever.txt"),  // virtual path
      kDummyDownloadFileSize,
      kOutputFilePath,
      "file:2_file_resource_id",
      ClientContext(BACKGROUND),
      google_apis::test_util::CreateCopyResultCallback(
          &download_error, &output_file_path),
      google_apis::GetContentCallback());
  // Metadata should still work
  google_apis::GDataErrorCode metadata_error = google_apis::GDATA_OTHER_ERROR;
  scoped_ptr<google_apis::AboutResource> about_resource;

  // Try to get the metadata
  scheduler_->GetAboutResource(
      google_apis::test_util::CreateCopyResultCallback(
          &metadata_error, &about_resource));
  base::RunLoop().RunUntilIdle();

  // Check the metadata
  ASSERT_EQ(google_apis::HTTP_SUCCESS, metadata_error);
  ASSERT_TRUE(about_resource);

  // Check the download
  EXPECT_EQ(google_apis::HTTP_SUCCESS, download_error);
  std::string content;
  EXPECT_EQ(output_file_path, kOutputFilePath);
  ASSERT_TRUE(base::ReadFileToString(output_file_path, &content));
  EXPECT_EQ("This is some test content.", content);
}

TEST_F(JobSchedulerTest, DownloadFileWimaxEnabled) {
  ConnectToWimax();

  // Enable fetching over cellular network.
  pref_service_->SetBoolean(prefs::kDisableDriveOverCellular, false);

  // Try to get a file in the background
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  const base::FilePath kOutputFilePath =
      temp_dir.path().AppendASCII("whatever.txt");
  google_apis::GDataErrorCode download_error = google_apis::GDATA_OTHER_ERROR;
  base::FilePath output_file_path;
  scheduler_->DownloadFile(
      base::FilePath::FromUTF8Unsafe("drive/whatever.txt"),  // virtual path
      kDummyDownloadFileSize,
      kOutputFilePath,
      "file:2_file_resource_id",
      ClientContext(BACKGROUND),
      google_apis::test_util::CreateCopyResultCallback(
          &download_error, &output_file_path),
      google_apis::GetContentCallback());
  // Metadata should still work
  google_apis::GDataErrorCode metadata_error = google_apis::GDATA_OTHER_ERROR;
  scoped_ptr<google_apis::AboutResource> about_resource;

  // Try to get the metadata
  scheduler_->GetAboutResource(
      google_apis::test_util::CreateCopyResultCallback(
          &metadata_error, &about_resource));
  base::RunLoop().RunUntilIdle();

  // Check the metadata
  ASSERT_EQ(google_apis::HTTP_SUCCESS, metadata_error);
  ASSERT_TRUE(about_resource);

  // Check the download
  EXPECT_EQ(google_apis::HTTP_SUCCESS, download_error);
  std::string content;
  EXPECT_EQ(output_file_path, kOutputFilePath);
  ASSERT_TRUE(base::ReadFileToString(output_file_path, &content));
  EXPECT_EQ("This is some test content.", content);
}

TEST_F(JobSchedulerTest, JobInfo) {
  JobListLogger logger;
  scheduler_->AddObserver(&logger);

  // Disable background upload/download.
  ConnectToWimax();
  pref_service_->SetBoolean(prefs::kDisableDriveOverCellular, true);

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  google_apis::GDataErrorCode error = google_apis::GDATA_OTHER_ERROR;
  scoped_ptr<google_apis::ResourceEntry> entry;
  scoped_ptr<google_apis::AboutResource> about_resource;
  base::FilePath path;

  std::set<JobType> expected_types;

  // Add many jobs.
  expected_types.insert(TYPE_ADD_NEW_DIRECTORY);
  scheduler_->AddNewDirectory(
      fake_drive_service_->GetRootResourceId(),
      "New Directory",
      google_apis::test_util::CreateCopyResultCallback(&error, &entry));
  expected_types.insert(TYPE_GET_ABOUT_RESOURCE);
  scheduler_->GetAboutResource(
      google_apis::test_util::CreateCopyResultCallback(
          &error, &about_resource));
  expected_types.insert(TYPE_RENAME_RESOURCE);
  scheduler_->RenameResource(
      "file:2_file_resource_id",
      "New Title",
      google_apis::test_util::CreateCopyResultCallback(&error));
  expected_types.insert(TYPE_DOWNLOAD_FILE);
  scheduler_->DownloadFile(
      base::FilePath::FromUTF8Unsafe("drive/whatever.txt"),  // virtual path
      kDummyDownloadFileSize,
      temp_dir.path().AppendASCII("whatever.txt"),
      "file:2_file_resource_id",
      ClientContext(BACKGROUND),
      google_apis::test_util::CreateCopyResultCallback(&error, &path),
      google_apis::GetContentCallback());

  // The number of jobs queued so far.
  EXPECT_EQ(4U, scheduler_->GetJobInfoList().size());
  EXPECT_TRUE(logger.Has(JobListLogger::ADDED, TYPE_ADD_NEW_DIRECTORY));
  EXPECT_TRUE(logger.Has(JobListLogger::ADDED, TYPE_GET_ABOUT_RESOURCE));
  EXPECT_TRUE(logger.Has(JobListLogger::ADDED, TYPE_RENAME_RESOURCE));
  EXPECT_TRUE(logger.Has(JobListLogger::ADDED, TYPE_DOWNLOAD_FILE));
  EXPECT_FALSE(logger.Has(JobListLogger::DONE, TYPE_ADD_NEW_DIRECTORY));
  EXPECT_FALSE(logger.Has(JobListLogger::DONE, TYPE_GET_ABOUT_RESOURCE));
  EXPECT_FALSE(logger.Has(JobListLogger::DONE, TYPE_RENAME_RESOURCE));
  EXPECT_FALSE(logger.Has(JobListLogger::DONE, TYPE_DOWNLOAD_FILE));

  // Add more jobs.
  expected_types.insert(TYPE_ADD_RESOURCE_TO_DIRECTORY);
  scheduler_->AddResourceToDirectory(
      "folder:1_folder_resource_id",
      "file:2_file_resource_id",
      google_apis::test_util::CreateCopyResultCallback(&error));
  expected_types.insert(TYPE_COPY_HOSTED_DOCUMENT);
  scheduler_->CopyHostedDocument(
      "document:5_document_resource_id",
      "New Document",
      google_apis::test_util::CreateCopyResultCallback(&error, &entry));

  // 6 jobs in total were queued.
  std::vector<JobInfo> jobs = scheduler_->GetJobInfoList();
  EXPECT_EQ(6U, jobs.size());
  std::set<JobType> actual_types;
  std::set<JobID> job_ids;
  for (size_t i = 0; i < jobs.size(); ++i) {
    actual_types.insert(jobs[i].job_type);
    job_ids.insert(jobs[i].job_id);
  }
  EXPECT_EQ(expected_types, actual_types);
  EXPECT_EQ(6U, job_ids.size()) << "All job IDs must be unique";
  EXPECT_TRUE(logger.Has(JobListLogger::ADDED, TYPE_ADD_RESOURCE_TO_DIRECTORY));
  EXPECT_TRUE(logger.Has(JobListLogger::ADDED, TYPE_COPY_HOSTED_DOCUMENT));
  EXPECT_FALSE(logger.Has(JobListLogger::DONE, TYPE_ADD_RESOURCE_TO_DIRECTORY));
  EXPECT_FALSE(logger.Has(JobListLogger::DONE, TYPE_COPY_HOSTED_DOCUMENT));

  // Run the jobs.
  base::RunLoop().RunUntilIdle();

  // All jobs except the BACKGROUND job should have started running (UPDATED)
  // and then finished (DONE).
  jobs = scheduler_->GetJobInfoList();
  ASSERT_EQ(1U, jobs.size());
  EXPECT_EQ(TYPE_DOWNLOAD_FILE, jobs[0].job_type);

  EXPECT_TRUE(logger.Has(JobListLogger::UPDATED, TYPE_ADD_NEW_DIRECTORY));
  EXPECT_TRUE(logger.Has(JobListLogger::UPDATED, TYPE_GET_ABOUT_RESOURCE));
  EXPECT_TRUE(logger.Has(JobListLogger::UPDATED, TYPE_RENAME_RESOURCE));
  EXPECT_TRUE(logger.Has(JobListLogger::UPDATED,
                         TYPE_ADD_RESOURCE_TO_DIRECTORY));
  EXPECT_TRUE(logger.Has(JobListLogger::UPDATED, TYPE_COPY_HOSTED_DOCUMENT));
  EXPECT_FALSE(logger.Has(JobListLogger::UPDATED, TYPE_DOWNLOAD_FILE));

  EXPECT_TRUE(logger.Has(JobListLogger::DONE, TYPE_ADD_NEW_DIRECTORY));
  EXPECT_TRUE(logger.Has(JobListLogger::DONE, TYPE_GET_ABOUT_RESOURCE));
  EXPECT_TRUE(logger.Has(JobListLogger::DONE, TYPE_RENAME_RESOURCE));
  EXPECT_TRUE(logger.Has(JobListLogger::DONE, TYPE_ADD_RESOURCE_TO_DIRECTORY));
  EXPECT_TRUE(logger.Has(JobListLogger::DONE, TYPE_COPY_HOSTED_DOCUMENT));
  EXPECT_FALSE(logger.Has(JobListLogger::DONE, TYPE_DOWNLOAD_FILE));

  // Run the background downloading job as well.
  ConnectToWifi();
  base::RunLoop().RunUntilIdle();

  // All jobs should have finished.
  EXPECT_EQ(0U, scheduler_->GetJobInfoList().size());
  EXPECT_TRUE(logger.Has(JobListLogger::UPDATED, TYPE_DOWNLOAD_FILE));
  EXPECT_TRUE(logger.Has(JobListLogger::DONE, TYPE_DOWNLOAD_FILE));
}

TEST_F(JobSchedulerTest, JobInfoProgress) {
  JobListLogger logger;
  scheduler_->AddObserver(&logger);

  ConnectToWifi();

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  google_apis::GDataErrorCode error = google_apis::GDATA_OTHER_ERROR;
  base::FilePath path;

  // Download job.
  scheduler_->DownloadFile(
      base::FilePath::FromUTF8Unsafe("drive/whatever.txt"),  // virtual path
      kDummyDownloadFileSize,
      temp_dir.path().AppendASCII("whatever.txt"),
      "file:2_file_resource_id",
      ClientContext(BACKGROUND),
      google_apis::test_util::CreateCopyResultCallback(&error, &path),
      google_apis::GetContentCallback());
  base::RunLoop().RunUntilIdle();

  std::vector<int64> download_progress;
  logger.GetProgressInfo(TYPE_DOWNLOAD_FILE, &download_progress);
  ASSERT_TRUE(!download_progress.empty());
  EXPECT_TRUE(base::STLIsSorted(download_progress));
  EXPECT_GE(download_progress.front(), 0);
  EXPECT_LE(download_progress.back(), 26);

  // Upload job.
  path = temp_dir.path().AppendASCII("new_file.txt");
  ASSERT_TRUE(google_apis::test_util::WriteStringToFile(path, "Hello"));
  google_apis::GDataErrorCode upload_error =
      google_apis::GDATA_OTHER_ERROR;
  scoped_ptr<google_apis::ResourceEntry> entry;

  scheduler_->UploadNewFile(
      fake_drive_service_->GetRootResourceId(),
      base::FilePath::FromUTF8Unsafe("drive/new_file.txt"),
      path,
      "dummy title",
      "plain/plain",
      ClientContext(BACKGROUND),
      google_apis::test_util::CreateCopyResultCallback(&upload_error, &entry));
  base::RunLoop().RunUntilIdle();

  std::vector<int64> upload_progress;
  logger.GetProgressInfo(TYPE_UPLOAD_NEW_FILE, &upload_progress);
  ASSERT_TRUE(!upload_progress.empty());
  EXPECT_TRUE(base::STLIsSorted(upload_progress));
  EXPECT_GE(upload_progress.front(), 0);
  EXPECT_LE(upload_progress.back(), 13);
}

TEST_F(JobSchedulerTest, CancelPendingJob) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath upload_path = temp_dir.path().AppendASCII("new_file.txt");
  ASSERT_TRUE(google_apis::test_util::WriteStringToFile(upload_path, "Hello"));

  // To create a pending job for testing, set the mode to cellular connection
  // and issue BACKGROUND jobs.
  ConnectToCellular();
  pref_service_->SetBoolean(prefs::kDisableDriveOverCellular, true);

  // Start the first job and record its job ID.
  google_apis::GDataErrorCode error1 = google_apis::GDATA_OTHER_ERROR;
  scoped_ptr<google_apis::ResourceEntry> entry;
  scheduler_->UploadNewFile(
      fake_drive_service_->GetRootResourceId(),
      base::FilePath::FromUTF8Unsafe("dummy/path"),
      upload_path,
      "dummy title 1",
      "text/plain",
      ClientContext(BACKGROUND),
      google_apis::test_util::CreateCopyResultCallback(&error1, &entry));

  const std::vector<JobInfo>& jobs = scheduler_->GetJobInfoList();
  ASSERT_EQ(1u, jobs.size());
  ASSERT_EQ(STATE_NONE, jobs[0].state);  // Not started yet.
  JobID first_job_id = jobs[0].job_id;

  // Start the second job.
  google_apis::GDataErrorCode error2 = google_apis::GDATA_OTHER_ERROR;
  scheduler_->UploadNewFile(
      fake_drive_service_->GetRootResourceId(),
      base::FilePath::FromUTF8Unsafe("dummy/path"),
      upload_path,
      "dummy title 2",
      "text/plain",
      ClientContext(BACKGROUND),
      google_apis::test_util::CreateCopyResultCallback(&error2, &entry));

  // Cancel the first one.
  scheduler_->CancelJob(first_job_id);

  // Only the first job should be cancelled.
  ConnectToWifi();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(google_apis::GDATA_CANCELLED, error1);
  EXPECT_EQ(google_apis::HTTP_SUCCESS, error2);
  EXPECT_TRUE(scheduler_->GetJobInfoList().empty());
}

TEST_F(JobSchedulerTest, CancelRunningJob) {
  ConnectToWifi();

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath upload_path = temp_dir.path().AppendASCII("new_file.txt");
  ASSERT_TRUE(google_apis::test_util::WriteStringToFile(upload_path, "Hello"));

  // Run as a cancelable task.
  fake_drive_service_->set_upload_new_file_cancelable(true);
  google_apis::GDataErrorCode error1 = google_apis::GDATA_OTHER_ERROR;
  scoped_ptr<google_apis::ResourceEntry> entry;
  scheduler_->UploadNewFile(
      fake_drive_service_->GetRootResourceId(),
      base::FilePath::FromUTF8Unsafe("dummy/path"),
      upload_path,
      "dummy title 1",
      "text/plain",
      ClientContext(USER_INITIATED),
      google_apis::test_util::CreateCopyResultCallback(&error1, &entry));

  const std::vector<JobInfo>& jobs = scheduler_->GetJobInfoList();
  ASSERT_EQ(1u, jobs.size());
  ASSERT_EQ(STATE_RUNNING, jobs[0].state);  // It's running.
  JobID first_job_id = jobs[0].job_id;

  // Start the second job normally.
  fake_drive_service_->set_upload_new_file_cancelable(false);
  google_apis::GDataErrorCode error2 = google_apis::GDATA_OTHER_ERROR;
  scheduler_->UploadNewFile(
      fake_drive_service_->GetRootResourceId(),
      base::FilePath::FromUTF8Unsafe("dummy/path"),
      upload_path,
      "dummy title 2",
      "text/plain",
      ClientContext(USER_INITIATED),
      google_apis::test_util::CreateCopyResultCallback(&error2, &entry));

  // Cancel the first one.
  scheduler_->CancelJob(first_job_id);

  // Only the first job should be cancelled.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(google_apis::GDATA_CANCELLED, error1);
  EXPECT_EQ(google_apis::HTTP_SUCCESS, error2);
  EXPECT_TRUE(scheduler_->GetJobInfoList().empty());
}

}  // namespace drive
