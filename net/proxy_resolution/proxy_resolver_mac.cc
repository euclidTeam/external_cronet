// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/proxy_resolution/proxy_resolver_mac.h"

#include <CoreFoundation/CoreFoundation.h>

#include <memory>

#include "base/check.h"
#include "base/lazy_instance.h"
#include "base/mac/foundation_util.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_checker.h"
#include "build/build_config.h"
#include "net/base/net_errors.h"
#include "net/base/proxy_server.h"
#include "net/base/proxy_string_util.h"
#include "net/proxy_resolution/proxy_info.h"
#include "net/proxy_resolution/proxy_list.h"
#include "net/proxy_resolution/proxy_resolver.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_IOS)
#include <CFNetwork/CFProxySupport.h>
#else
#include <CoreServices/CoreServices.h>
#endif

namespace net {

class NetworkAnonymizationKey;

namespace {

// A lock shared by all ProxyResolverMac instances. It is used to synchronize
// the events of multiple CFNetworkExecuteProxyAutoConfigurationURL run loop
// sources. These events are:
// 1. Adding the source to the run loop.
// 2. Handling the source result.
// 3. Removing the source from the run loop.
static base::LazyInstance<base::Lock>::Leaky g_cfnetwork_pac_runloop_lock =
    LAZY_INSTANCE_INITIALIZER;

// Forward declaration of the callback function used by the
// SynchronizedRunLoopObserver class.
void RunLoopObserverCallBackFunc(CFRunLoopObserverRef observer,
                                 CFRunLoopActivity activity,
                                 void* info);

// Utility function to map a CFProxyType to a ProxyServer::Scheme.
// If the type is unknown, returns ProxyServer::SCHEME_INVALID.
ProxyServer::Scheme GetProxyServerScheme(CFStringRef proxy_type) {
  if (CFEqual(proxy_type, kCFProxyTypeNone))
    return ProxyServer::SCHEME_DIRECT;
  if (CFEqual(proxy_type, kCFProxyTypeHTTP))
    return ProxyServer::SCHEME_HTTP;
  if (CFEqual(proxy_type, kCFProxyTypeHTTPS)) {
    // The "HTTPS" on the Mac side here means "proxy applies to https://" URLs;
    // the proxy itself is still expected to be an HTTP proxy.
    return ProxyServer::SCHEME_HTTP;
  }
  if (CFEqual(proxy_type, kCFProxyTypeSOCKS)) {
    // We can't tell whether this was v4 or v5. We will assume it is
    // v5 since that is the only version OS X supports.
    return ProxyServer::SCHEME_SOCKS5;
  }
  return ProxyServer::SCHEME_INVALID;
}

// Callback for CFNetworkExecuteProxyAutoConfigurationURL. |client| is a pointer
// to a CFTypeRef.  This stashes either |error| or |proxies| in that location.
void ResultCallback(void* client, CFArrayRef proxies, CFErrorRef error) {
  DCHECK((proxies != nullptr) == (error == nullptr));

  CFTypeRef* result_ptr = reinterpret_cast<CFTypeRef*>(client);
  DCHECK(result_ptr != nullptr);
  DCHECK(*result_ptr == nullptr);

  if (error != nullptr) {
    *result_ptr = CFRetain(error);
  } else {
    *result_ptr = CFRetain(proxies);
  }
  CFRunLoopStop(CFRunLoopGetCurrent());
}

#pragma mark - SynchronizedRunLoopObserver
// A run loop observer that guarantees that no two run loop sources protected
// by the same lock will be fired concurrently in different threads.
// The observer does not prevent the parallel execution of the sources but only
// synchronizes the run loop events associated with the sources. In the context
// of proxy resolver, the observer is used to synchronize the execution of the
// callbacks function that handles the result of
// CFNetworkExecuteProxyAutoConfigurationURL execution.
class SynchronizedRunLoopObserver final {
 public:
  // Creates the instance of an observer that will synchronize the sources
  // using a given |lock|.
  SynchronizedRunLoopObserver(base::Lock& lock);

  SynchronizedRunLoopObserver(const SynchronizedRunLoopObserver&) = delete;
  SynchronizedRunLoopObserver& operator=(const SynchronizedRunLoopObserver&) =
      delete;

  // Destructor.
  ~SynchronizedRunLoopObserver();
  // Adds the observer to the current run loop for a given run loop mode.
  // This method should always be paired with |RemoveFromCurrentRunLoop|.
  void AddToCurrentRunLoop(const CFStringRef mode);
  // Removes the observer from the current run loop for a given run loop mode.
  // This method should always be paired with |AddToCurrentRunLoop|.
  void RemoveFromCurrentRunLoop(const CFStringRef mode);
  // Callback function that is called when an observable run loop event occurs.
  void RunLoopObserverCallBack(CFRunLoopObserverRef observer,
                               CFRunLoopActivity activity);

 private:
  // Lock to use to synchronize the run loop sources.
  base::Lock& lock_;
  // Indicates whether the current observer holds the lock. It is used to
  // avoid double locking and releasing.
  bool lock_acquired_ = false;
  // The underlying CFRunLoopObserverRef structure wrapped by this instance.
  base::ScopedCFTypeRef<CFRunLoopObserverRef> observer_;
  // Validates that all methods of this class are executed on the same thread.
  base::ThreadChecker thread_checker_;
};

SynchronizedRunLoopObserver::SynchronizedRunLoopObserver(base::Lock& lock)
    : lock_(lock) {
  CFRunLoopObserverContext observer_context = {0, this, nullptr, nullptr,
                                               nullptr};
  observer_.reset(CFRunLoopObserverCreate(
      kCFAllocatorDefault,
      kCFRunLoopBeforeSources | kCFRunLoopBeforeWaiting | kCFRunLoopExit, true,
      0, RunLoopObserverCallBackFunc, &observer_context));
}

SynchronizedRunLoopObserver::~SynchronizedRunLoopObserver() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!lock_acquired_);
}

void SynchronizedRunLoopObserver::AddToCurrentRunLoop(const CFStringRef mode) {
  DCHECK(thread_checker_.CalledOnValidThread());
  CFRunLoopAddObserver(CFRunLoopGetCurrent(), observer_.get(), mode);
}

void SynchronizedRunLoopObserver::RemoveFromCurrentRunLoop(
    const CFStringRef mode) {
  DCHECK(thread_checker_.CalledOnValidThread());
  CFRunLoopRemoveObserver(CFRunLoopGetCurrent(), observer_.get(), mode);
}

void SynchronizedRunLoopObserver::RunLoopObserverCallBack(
    CFRunLoopObserverRef observer,
    CFRunLoopActivity activity) NO_THREAD_SAFETY_ANALYSIS {
  DCHECK(thread_checker_.CalledOnValidThread());
  // Acquire the lock when a source has been signaled and going to be fired.
  // In the context of the proxy resolver that happens when the proxy for a
  // given URL has been resolved and the callback function that handles the
  // result is going to be fired.
  // Release the lock when all source events have been handled.
  //
  // NO_THREAD_SAFETY_ANALYSIS: Runtime dependent locking.
  switch (activity) {
    case kCFRunLoopBeforeSources:
      if (!lock_acquired_) {
        lock_.Acquire();
        lock_acquired_ = true;
      }
      break;
    case kCFRunLoopBeforeWaiting:
    case kCFRunLoopExit:
      if (lock_acquired_) {
        lock_acquired_ = false;
        lock_.Release();
      }
      break;
  }
}

void RunLoopObserverCallBackFunc(CFRunLoopObserverRef observer,
                                 CFRunLoopActivity activity,
                                 void* info) {
  // Forward the call to the instance of SynchronizedRunLoopObserver
  // that is associated with the current CF run loop observer.
  SynchronizedRunLoopObserver* observerInstance =
      (SynchronizedRunLoopObserver*)info;
  observerInstance->RunLoopObserverCallBack(observer, activity);
}

#pragma mark - ProxyResolverMac
class ProxyResolverMac : public ProxyResolver {
 public:
  explicit ProxyResolverMac(const scoped_refptr<PacFileData>& script_data);
  ~ProxyResolverMac() override;

  // ProxyResolver methods:
  int GetProxyForURL(const GURL& url,
                     const NetworkAnonymizationKey& network_anonymization_key,
                     ProxyInfo* results,
                     CompletionOnceCallback callback,
                     std::unique_ptr<Request>* request,
                     const NetLogWithSource& net_log) override;

 private:
  const scoped_refptr<PacFileData> script_data_;
};

ProxyResolverMac::ProxyResolverMac(
    const scoped_refptr<PacFileData>& script_data)
    : script_data_(script_data) {}

ProxyResolverMac::~ProxyResolverMac() = default;

// Gets the proxy information for a query URL from a PAC. Implementation
// inspired by http://developer.apple.com/samplecode/CFProxySupportTool/
int ProxyResolverMac::GetProxyForURL(
    const GURL& query_url,
    const NetworkAnonymizationKey& network_anonymization_key,
    ProxyInfo* results,
    CompletionOnceCallback /*callback*/,
    std::unique_ptr<Request>* /*request*/,
    const NetLogWithSource& net_log) {
  // OS X's system resolver does not support WebSocket URLs in proxy.pac, as of
  // version 10.13.5. See https://crbug.com/862121.
  GURL mutable_query_url = query_url;
  if (query_url.SchemeIsWSOrWSS()) {
    GURL::Replacements replacements;
    replacements.SetSchemeStr(query_url.SchemeIsCryptographic() ? "https"
                                                                : "http");
    mutable_query_url = query_url.ReplaceComponents(replacements);
  }

  base::ScopedCFTypeRef<CFStringRef> query_ref(
      base::SysUTF8ToCFStringRef(mutable_query_url.spec()));
  base::ScopedCFTypeRef<CFURLRef> query_url_ref(
      CFURLCreateWithString(kCFAllocatorDefault, query_ref.get(), nullptr));
  if (!query_url_ref.get())
    return ERR_FAILED;
  base::ScopedCFTypeRef<CFStringRef> pac_ref(base::SysUTF8ToCFStringRef(
      script_data_->type() == PacFileData::TYPE_AUTO_DETECT
          ? std::string()
          : script_data_->url().spec()));
  base::ScopedCFTypeRef<CFURLRef> pac_url_ref(
      CFURLCreateWithString(kCFAllocatorDefault, pac_ref.get(), nullptr));
  if (!pac_url_ref.get())
    return ERR_FAILED;

  // Work around <rdar://problem/5530166>. This dummy call to
  // CFNetworkCopyProxiesForURL initializes some state within CFNetwork that is
  // required by CFNetworkExecuteProxyAutoConfigurationURL.

  base::ScopedCFTypeRef<CFDictionaryRef> empty_dictionary(
      CFDictionaryCreate(nullptr, nullptr, nullptr, 0, nullptr, nullptr));
  CFArrayRef dummy_result =
      CFNetworkCopyProxiesForURL(query_url_ref.get(), empty_dictionary);
  if (dummy_result)
    CFRelease(dummy_result);

  // We cheat here. We need to act as if we were synchronous, so we pump the
  // runloop ourselves. Our caller moved us to a new thread anyway, so this is
  // OK to do. (BTW, CFNetworkExecuteProxyAutoConfigurationURL returns a
  // runloop source we need to release despite its name.)

  CFTypeRef result = nullptr;
  CFStreamClientContext context = {0, &result, nullptr, nullptr, nullptr};
  base::ScopedCFTypeRef<CFRunLoopSourceRef> runloop_source(
      CFNetworkExecuteProxyAutoConfigurationURL(
          pac_url_ref.get(), query_url_ref.get(), ResultCallback, &context));
  if (!runloop_source)
    return ERR_FAILED;

  const CFStringRef private_runloop_mode =
      CFSTR("org.chromium.ProxyResolverMac");

  // Add the run loop observer to synchronize events of
  // CFNetworkExecuteProxyAutoConfigurationURL sources. See the definition of
  // |g_cfnetwork_pac_runloop_lock|.
  SynchronizedRunLoopObserver observer(g_cfnetwork_pac_runloop_lock.Get());
  observer.AddToCurrentRunLoop(private_runloop_mode);

  // Make sure that no CFNetworkExecuteProxyAutoConfigurationURL sources
  // are added to the run loop concurrently.
  {
    base::AutoLock lock(g_cfnetwork_pac_runloop_lock.Get());
    CFRunLoopAddSource(CFRunLoopGetCurrent(), runloop_source.get(),
                       private_runloop_mode);
  }

  CFRunLoopRunInMode(private_runloop_mode, DBL_MAX, false);

  // Make sure that no CFNetworkExecuteProxyAutoConfigurationURL sources
  // are removed from the run loop concurrently.
  {
    base::AutoLock lock(g_cfnetwork_pac_runloop_lock.Get());
    CFRunLoopRemoveSource(CFRunLoopGetCurrent(), runloop_source.get(),
                          private_runloop_mode);
  }
  observer.RemoveFromCurrentRunLoop(private_runloop_mode);

  DCHECK(result != nullptr);

  if (CFGetTypeID(result) == CFErrorGetTypeID()) {
    // TODO(avi): do something better than this
    CFRelease(result);
    return ERR_FAILED;
  }
  base::ScopedCFTypeRef<CFArrayRef> proxy_array_ref(
      base::mac::CFCastStrict<CFArrayRef>(result));
  DCHECK(proxy_array_ref != nullptr);

  ProxyList proxy_list;

  CFIndex proxy_array_count = CFArrayGetCount(proxy_array_ref.get());
  for (CFIndex i = 0; i < proxy_array_count; ++i) {
    CFDictionaryRef proxy_dictionary = base::mac::CFCastStrict<CFDictionaryRef>(
        CFArrayGetValueAtIndex(proxy_array_ref.get(), i));
    DCHECK(proxy_dictionary != nullptr);

    // The dictionary may have the following keys:
    // - kCFProxyTypeKey : The type of the proxy
    // - kCFProxyHostNameKey
    // - kCFProxyPortNumberKey : The meat we're after.
    // - kCFProxyUsernameKey
    // - kCFProxyPasswordKey : Despite the existence of these keys in the
    //                         documentation, they're never populated. Even if a
    //                         username/password were to be set in the network
    //                         proxy system preferences, we'd need to fetch it
    //                         from the Keychain ourselves. CFProxy is such a
    //                         tease.
    // - kCFProxyAutoConfigurationURLKey : If the PAC file specifies another
    //                                     PAC file, I'm going home.

    CFStringRef proxy_type = base::mac::GetValueFromDictionary<CFStringRef>(
        proxy_dictionary, kCFProxyTypeKey);
    ProxyServer proxy_server = ProxyDictionaryToProxyServer(
        GetProxyServerScheme(proxy_type), proxy_dictionary, kCFProxyHostNameKey,
        kCFProxyPortNumberKey);
    if (!proxy_server.is_valid())
      continue;

    proxy_list.AddProxyServer(proxy_server);
  }

  if (!proxy_list.IsEmpty())
    results->UseProxyList(proxy_list);
  // Else do nothing (results is already guaranteed to be in the default state).

  return OK;
}

}  // namespace

ProxyResolverFactoryMac::ProxyResolverFactoryMac()
    : ProxyResolverFactory(false /*expects_pac_bytes*/) {
}

int ProxyResolverFactoryMac::CreateProxyResolver(
    const scoped_refptr<PacFileData>& pac_script,
    std::unique_ptr<ProxyResolver>* resolver,
    CompletionOnceCallback callback,
    std::unique_ptr<Request>* request) {
  *resolver = std::make_unique<ProxyResolverMac>(pac_script);
  return OK;
}

}  // namespace net
