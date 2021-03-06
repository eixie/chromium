// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/service_worker_context.h"

#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "content/browser/browser_thread_impl.h"
#include "content/browser/service_worker/embedded_worker_registry.h"
#include "content/browser/service_worker/embedded_worker_test_helper.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/browser/service_worker/service_worker_storage.h"
#include "content/common/service_worker/service_worker_messages.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

void SaveResponseCallback(bool* called,
                          int64* store_result,
                          ServiceWorkerStatusCode status,
                          int64 result) {
  *called = true;
  *store_result = result;
}

ServiceWorkerContextCore::RegistrationCallback MakeRegisteredCallback(
    bool* called,
    int64* store_result) {
  return base::Bind(&SaveResponseCallback, called, store_result);
}

void CallCompletedCallback(bool* called, ServiceWorkerStatusCode) {
  *called = true;
}

ServiceWorkerContextCore::UnregistrationCallback MakeUnregisteredCallback(
    bool* called) {
  return base::Bind(&CallCompletedCallback, called);
}

void ExpectRegisteredWorkers(
    bool expect_pending,
    bool expect_active,
    ServiceWorkerStatusCode status,
    const scoped_refptr<ServiceWorkerRegistration>& registration) {
  ASSERT_EQ(SERVICE_WORKER_OK, status);

  if (expect_pending)
    EXPECT_TRUE(registration->pending_version());
  else
    EXPECT_FALSE(registration->pending_version());

  if (expect_active)
    EXPECT_TRUE(registration->active_version());
  else
    EXPECT_FALSE(registration->active_version());
}

}  // namespace

class ServiceWorkerContextTest : public testing::Test {
 public:
  ServiceWorkerContextTest()
      : browser_thread_bundle_(TestBrowserThreadBundle::IO_MAINLOOP),
        render_process_id_(99) {}

  virtual void SetUp() OVERRIDE {
    context_.reset(new ServiceWorkerContextCore(base::FilePath(), NULL));
    helper_.reset(new EmbeddedWorkerTestHelper(
        context_.get(), render_process_id_));
  }

  virtual void TearDown() OVERRIDE {
    helper_.reset();
    context_.reset();
  }

 protected:
  TestBrowserThreadBundle browser_thread_bundle_;
  scoped_ptr<ServiceWorkerContextCore> context_;
  scoped_ptr<EmbeddedWorkerTestHelper> helper_;
  const int render_process_id_;
};

// Make sure basic registration is working.
TEST_F(ServiceWorkerContextTest, Register) {
  int64 registration_id = -1L;
  bool called = false;
  context_->RegisterServiceWorker(
      GURL("http://www.example.com/*"),
      GURL("http://www.example.com/service_worker.js"),
      render_process_id_,
      MakeRegisteredCallback(&called, &registration_id));

  ASSERT_FALSE(called);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(called);

  EXPECT_EQ(2UL, helper_->ipc_sink()->message_count());
  EXPECT_TRUE(helper_->inner_ipc_sink()->GetUniqueMessageMatching(
      ServiceWorkerMsg_InstallEvent::ID));
  EXPECT_NE(-1L, registration_id);

  context_->storage()->FindRegistrationForId(
      registration_id,
      base::Bind(&ExpectRegisteredWorkers,
                 false /* expect_pending */,
                 true /* expect_active */));
  base::RunLoop().RunUntilIdle();
}

class RejectInstallTestHelper : public EmbeddedWorkerTestHelper {
 public:
  RejectInstallTestHelper(ServiceWorkerContextCore* context,
                          int mock_render_process_id)
      : EmbeddedWorkerTestHelper(context, mock_render_process_id) {}

  virtual void OnInstallEvent(int embedded_worker_id,
                              int request_id,
                              int active_version_id) OVERRIDE {
    SimulateSendMessageToBrowser(
        embedded_worker_id,
        request_id,
        ServiceWorkerHostMsg_InstallEventFinished(
            blink::WebServiceWorkerEventResultRejected));
  }
};

// Test registration when the service worker rejects the install event. The
// registration callback should indicate success, but there should be no pending
// or active worker in the registration.
TEST_F(ServiceWorkerContextTest, Register_RejectInstall) {
  helper_.reset(
      new RejectInstallTestHelper(context_.get(), render_process_id_));
  int64 registration_id = -1L;
  bool called = false;
  context_->RegisterServiceWorker(
      GURL("http://www.example.com/*"),
      GURL("http://www.example.com/service_worker.js"),
      render_process_id_,
      MakeRegisteredCallback(&called, &registration_id));

  ASSERT_FALSE(called);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(called);

  EXPECT_EQ(2UL, helper_->ipc_sink()->message_count());
  EXPECT_TRUE(helper_->inner_ipc_sink()->GetUniqueMessageMatching(
      ServiceWorkerMsg_InstallEvent::ID));
  EXPECT_NE(-1L, registration_id);

  context_->storage()->FindRegistrationForId(
      registration_id,
      base::Bind(&ExpectRegisteredWorkers,
                 false /* expect_pending */,
                 false /* expect_active */));
  base::RunLoop().RunUntilIdle();
}

// Test registration when there is an existing registration with no pending or
// active worker.
TEST_F(ServiceWorkerContextTest, Register_DuplicateScriptNoActiveWorker) {
  helper_.reset(
      new RejectInstallTestHelper(context_.get(), render_process_id_));
  int64 old_registration_id = -1L;
  bool called = false;
  context_->RegisterServiceWorker(
      GURL("http://www.example.com/*"),
      GURL("http://www.example.com/service_worker.js"),
      render_process_id_,
      MakeRegisteredCallback(&called, &old_registration_id));

  ASSERT_FALSE(called);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(called);

  EXPECT_EQ(2UL, helper_->ipc_sink()->message_count());

  int64 new_registration_id = -1L;
  called = false;
  context_->RegisterServiceWorker(
      GURL("http://www.example.com/*"),
      GURL("http://www.example.com/service_worker.js"),
      render_process_id_,
      MakeRegisteredCallback(&called, &new_registration_id));

  ASSERT_FALSE(called);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(called);

  EXPECT_EQ(old_registration_id, new_registration_id);
  // Our current implementation does the full registration flow on re-register,
  // so the worker receives another start message and install message.
  EXPECT_EQ(4UL, helper_->ipc_sink()->message_count());
}

// Make sure registrations are cleaned up when they are unregistered.
TEST_F(ServiceWorkerContextTest, Unregister) {
  GURL pattern("http://www.example.com/*");

  bool called = false;
  int64 registration_id = -1L;
  context_->RegisterServiceWorker(
      pattern,
      GURL("http://www.example.com/service_worker.js"),
      render_process_id_,
      MakeRegisteredCallback(&called, &registration_id));

  ASSERT_FALSE(called);
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(called);

  called = false;
  context_->UnregisterServiceWorker(
      pattern, render_process_id_, MakeUnregisteredCallback(&called));

  ASSERT_FALSE(called);
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(called);
}

// Make sure that when a new registration replaces an existing
// registration, that the old one is cleaned up.
TEST_F(ServiceWorkerContextTest, RegisterNewScript) {
  GURL pattern("http://www.example.com/*");

  bool called = false;
  int64 old_registration_id = -1L;
  context_->RegisterServiceWorker(
      pattern,
      GURL("http://www.example.com/service_worker.js"),
      render_process_id_,
      MakeRegisteredCallback(&called, &old_registration_id));

  ASSERT_FALSE(called);
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(called);

  called = false;
  int64 new_registration_id = -1L;
  context_->RegisterServiceWorker(
      pattern,
      GURL("http://www.example.com/service_worker_new.js"),
      render_process_id_,
      MakeRegisteredCallback(&called, &new_registration_id));

  ASSERT_FALSE(called);
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(called);

  ASSERT_NE(old_registration_id, new_registration_id);
}

// Make sure that when registering a duplicate pattern+script_url
// combination, that the same registration is used.
TEST_F(ServiceWorkerContextTest, RegisterDuplicateScript) {
  GURL pattern("http://www.example.com/*");
  GURL script_url("http://www.example.com/service_worker.js");

  bool called = false;
  int64 old_registration_id = -1L;
  context_->RegisterServiceWorker(
      pattern,
      script_url,
      render_process_id_,
      MakeRegisteredCallback(&called, &old_registration_id));

  ASSERT_FALSE(called);
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(called);

  called = false;
  int64 new_registration_id = -1L;
  context_->RegisterServiceWorker(
      pattern,
      script_url,
      render_process_id_,
      MakeRegisteredCallback(&called, &new_registration_id));

  ASSERT_FALSE(called);
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(called);

  ASSERT_EQ(old_registration_id, new_registration_id);
}

}  // namespace content
