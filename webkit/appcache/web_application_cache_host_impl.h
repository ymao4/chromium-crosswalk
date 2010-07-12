// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_APPCACHE_WEB_APPLICATION_CACHE_HOST_IMPL_H_
#define WEBKIT_APPCACHE_WEB_APPLICATION_CACHE_HOST_IMPL_H_

#include "googleurl/src/gurl.h"
#include "third_party/WebKit/WebKit/chromium/public/WebApplicationCacheHostClient.h"
#include "third_party/WebKit/WebKit/chromium/public/WebURLResponse.h"
#include "webkit/appcache/appcache_interfaces.h"

namespace WebKit {
class WebFrame;
}

namespace appcache {

class WebApplicationCacheHostImpl : public WebKit::WebApplicationCacheHost {
 public:
  // Returns the host having given id or NULL if there is no such host.
  static WebApplicationCacheHostImpl* FromId(int id);

  // Returns the host associated with the current document in frame.
  static WebApplicationCacheHostImpl* FromFrame(WebKit::WebFrame* frame);

  WebApplicationCacheHostImpl(WebKit::WebApplicationCacheHostClient* client,
                              AppCacheBackend* backend);
  virtual ~WebApplicationCacheHostImpl();

  int host_id() const { return host_id_; }
  AppCacheBackend* backend() const { return backend_; }
  WebKit::WebApplicationCacheHostClient* client() const { return client_; }

  void OnStatusChanged(appcache::Status);
  void OnEventRaised(appcache::EventID);
  void OnProgressEventRaised(const GURL& url, int num_total, int num_complete);
  virtual void OnLogMessage(LogLevel log_level, const std::string& message) {}
  virtual void OnContentBlocked(const GURL& manifest_url) {}
  virtual void OnCacheSelected(int64 selected_cache_id,
                               appcache::Status status);

  // WebApplicationCacheHost methods
  virtual void willStartMainResourceRequest(WebKit::WebURLRequest&);
  virtual void willStartSubResourceRequest(WebKit::WebURLRequest&);
  virtual void selectCacheWithoutManifest();
  virtual bool selectCacheWithManifest(const WebKit::WebURL& manifestURL);
  virtual void didReceiveResponseForMainResource(const WebKit::WebURLResponse&);
  virtual void didReceiveDataForMainResource(const char* data, int len);
  virtual void didFinishLoadingMainResource(bool success);
  virtual WebKit::WebApplicationCacheHost::Status status();
  virtual bool startUpdate();
  virtual bool swapCache();

 private:
  enum IsNewMasterEntry {
    MAYBE,
    YES,
    NO
  };

  WebKit::WebApplicationCacheHostClient* client_;
  AppCacheBackend* backend_;
  int host_id_;
  bool has_status_;
  appcache::Status status_;
  bool has_cached_status_;
  appcache::Status cached_status_;
  WebKit::WebURLResponse document_response_;
  GURL document_url_;
  bool is_scheme_supported_;
  bool is_get_method_;
  IsNewMasterEntry is_new_master_entry_;
};

}  // namespace

#endif  // WEBKIT_APPCACHE_WEB_APPLICATION_CACHE_HOST_IMPL_H_
