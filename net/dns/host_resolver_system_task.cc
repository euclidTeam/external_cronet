// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/host_resolver_system_task.h"

#include <memory>

#include "base/dcheck_is_on.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/metrics/field_trial_params.h"
#include "base/no_destructor.h"
#include "base/sequence_checker.h"
#include "base/sequence_checker_impl.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/trace_event/trace_event.h"
#include "base/types/pass_key.h"
#include "dns_reloader.h"
#include "net/base/net_errors.h"
#include "net/base/network_interfaces.h"
#include "net/base/sys_addrinfo.h"
#include "net/base/trace_constants.h"
#include "net/dns/address_info.h"
#include "net/dns/dns_names_util.h"

#if BUILDFLAG(IS_WIN)
#include "net/base/winsock_init.h"
#endif

namespace net {

namespace {

const base::FeatureParam<base::TaskPriority>::Option prio_modes[] = {
    {base::TaskPriority::USER_VISIBLE, "default"},
    {base::TaskPriority::USER_BLOCKING, "user_blocking"}};
BASE_FEATURE(kSystemResolverPriorityExperiment,
             "SystemResolverPriorityExperiment",
             base::FEATURE_DISABLED_BY_DEFAULT);
const base::FeatureParam<base::TaskPriority> priority_mode{
    &kSystemResolverPriorityExperiment, "mode",
    base::TaskPriority::USER_VISIBLE, &prio_modes};

base::TaskTraits GetSystemDnsResolutionTaskTraits() {
  return {base::MayBlock(), priority_mode.Get(),
          base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN};
}

// Returns nullptr in the common case, or a task runner if the default has
// been overridden.
scoped_refptr<base::TaskRunner>& GetSystemDnsResolutionTaskRunnerOverride() {
  static base::NoDestructor<scoped_refptr<base::TaskRunner>>
      system_dns_resolution_task_runner(nullptr);
  return *system_dns_resolution_task_runner;
}

// Posts a synchronous callback to a thread pool task runner created with
// GetSystemDnsResolutionTaskTraits(). This task runner can be overridden by
// assigning to GetSystemDnsResolutionTaskRunnerOverride(). `results_cb` will be
// called later on the current sequence with the results of the DNS resolution.
void PostSystemDnsResolutionTaskAndReply(
    base::OnceCallback<int(AddressList* addrlist, int* os_error)>
        system_dns_resolution_callback,
    SystemDnsResultsCallback results_cb) {
  auto addr_list = std::make_unique<net::AddressList>();
  net::AddressList* addr_list_ptr = addr_list.get();
  auto os_error = std::make_unique<int>();
  int* os_error_ptr = os_error.get();

  // This callback owns |addr_list| and |os_error| and just calls |results_cb|
  // with the results.
  auto call_with_results_cb = base::BindOnce(
      [](SystemDnsResultsCallback results_cb,
         std::unique_ptr<net::AddressList> addr_list,
         std::unique_ptr<int> os_error, int net_error) {
        std::move(results_cb).Run(std::move(*addr_list), *os_error, net_error);
      },
      std::move(results_cb), std::move(addr_list), std::move(os_error));

  scoped_refptr<base::TaskRunner> system_dns_resolution_task_runner =
      GetSystemDnsResolutionTaskRunnerOverride();
  if (!system_dns_resolution_task_runner) {
    // In production this will run on every call, otherwise some tests will
    // leave a stale task runner around after tearing down their task
    // environment. This should not be less performant than the regular
    // base::ThreadPool::PostTask().
    system_dns_resolution_task_runner =
        base::ThreadPool::CreateTaskRunner(GetSystemDnsResolutionTaskTraits());
  }
  system_dns_resolution_task_runner->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(std::move(system_dns_resolution_callback), addr_list_ptr,
                     os_error_ptr),
      std::move(call_with_results_cb));
}

int ResolveOnWorkerThread(scoped_refptr<HostResolverProc> resolver_proc,
                          absl::optional<std::string> hostname,
                          AddressFamily address_family,
                          HostResolverFlags flags,
                          handles::NetworkHandle network,
                          AddressList* addrlist,
                          int* os_error) {
  std::string hostname_str = hostname ? *hostname : GetHostName();
  if (resolver_proc) {
    return resolver_proc->Resolve(hostname_str, address_family, flags, addrlist,
                                  os_error, network);
  } else {
    return SystemHostResolverCall(hostname_str, address_family, flags, addrlist,
                                  os_error, network);
  }
}

// Creates NetLog parameters when the resolve failed.
base::Value NetLogHostResolverSystemTaskFailedParams(uint32_t attempt_number,
                                                     int net_error,
                                                     int os_error) {
  base::Value::Dict dict;
  if (attempt_number)
    dict.Set("attempt_number", base::saturated_cast<int>(attempt_number));

  dict.Set("net_error", net_error);

  if (os_error) {
    dict.Set("os_error", os_error);
#if BUILDFLAG(IS_WIN)
    // Map the error code to a human-readable string.
    LPWSTR error_string = nullptr;
    FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
                  nullptr,  // Use the internal message table.
                  os_error,
                  0,  // Use default language.
                  (LPWSTR)&error_string,
                  0,         // Buffer size.
                  nullptr);  // Arguments (unused).
    dict.Set("os_error_string", base::WideToUTF8(error_string));
    LocalFree(error_string);
#elif BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
    dict.Set("os_error_string", gai_strerror(os_error));
#endif
  }

  return base::Value(std::move(dict));
}

using SystemDnsResolverOverrideCallback =
    base::RepeatingCallback<void(const absl::optional<std::string>& host,
                                 AddressFamily address_family,
                                 HostResolverFlags host_resolver_flags,
                                 SystemDnsResultsCallback results_cb,
                                 handles::NetworkHandle network)>;

SystemDnsResolverOverrideCallback& GetSystemDnsResolverOverride() {
  static base::NoDestructor<SystemDnsResolverOverrideCallback> dns_override;

#if DCHECK_IS_ON()
  if (*dns_override) {
    // This should only be called on the main thread, so DCHECK that it is.
    // However, in unittests this may be called on different task environments
    // in the same process so only bother sequence checking if an override
    // exists.
    static base::NoDestructor<base::SequenceCheckerImpl> sequence_checker;
    base::ScopedValidateSequenceChecker scoped_validated_sequence_checker(
        *sequence_checker);
  }
#endif

  return *dns_override;
}

}  // namespace

void SetSystemDnsResolverOverride(
    SystemDnsResolverOverrideCallback dns_override) {
  // TODO(crbug.com/1312224): for now, only allow this override to be set once.
  DCHECK(!GetSystemDnsResolverOverride());
  GetSystemDnsResolverOverride() = std::move(dns_override);
}

HostResolverSystemTask::Params::Params(
    scoped_refptr<HostResolverProc> resolver_proc,
    size_t in_max_retry_attempts)
    : resolver_proc(std::move(resolver_proc)),
      max_retry_attempts(in_max_retry_attempts),
      unresponsive_delay(kDnsDefaultUnresponsiveDelay) {
  // Maximum of 4 retry attempts for host resolution.
  static const size_t kDefaultMaxRetryAttempts = 4u;
  if (max_retry_attempts == kDefaultRetryAttempts)
    max_retry_attempts = kDefaultMaxRetryAttempts;
}

HostResolverSystemTask::Params::Params(const Params& other) = default;

HostResolverSystemTask::Params::~Params() = default;

// static
std::unique_ptr<HostResolverSystemTask> HostResolverSystemTask::Create(
    std::string hostname,
    AddressFamily address_family,
    HostResolverFlags flags,
    const Params& params,
    const NetLogWithSource& job_net_log,
    handles::NetworkHandle network) {
  return std::make_unique<HostResolverSystemTask>(
      hostname, address_family, flags, params, job_net_log, network);
}

// static
std::unique_ptr<HostResolverSystemTask>
HostResolverSystemTask::CreateForOwnHostname(
    AddressFamily address_family,
    HostResolverFlags flags,
    const Params& params,
    const NetLogWithSource& job_net_log,
    handles::NetworkHandle network) {
  return std::make_unique<HostResolverSystemTask>(
      absl::nullopt, address_family, flags, params, job_net_log, network);
}

HostResolverSystemTask::HostResolverSystemTask(
    absl::optional<std::string> hostname,
    AddressFamily address_family,
    HostResolverFlags flags,
    const Params& params,
    const NetLogWithSource& job_net_log,
    handles::NetworkHandle network)
    : hostname_(std::move(hostname)),
      address_family_(address_family),
      flags_(flags),
      params_(params),
      net_log_(job_net_log),
      network_(network) {
  if (hostname_) {
    // |host| should be a valid domain name. HostResolverManager has checks to
    // fail early if this is not the case.
    DCHECK(dns_names_util::IsValidDnsName(*hostname_))
        << "Invalid hostname: " << *hostname_;
  }
  // If a resolver_proc has not been specified, try to use a default if one is
  // set, as it may be in tests.
  if (!params_.resolver_proc.get())
    params_.resolver_proc = HostResolverProc::GetDefault();
}

// Cancels this HostResolverSystemTask. Any outstanding resolve attempts cannot
// be cancelled, but they will post back to the current thread before checking
// their WeakPtrs to find that this task is cancelled.
HostResolverSystemTask::~HostResolverSystemTask() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // If this is cancellation, log the EndEvent (otherwise this was logged in
  // OnLookupComplete()).
  if (!was_completed())
    net_log_.EndEvent(NetLogEventType::HOST_RESOLVER_SYSTEM_TASK);
}

void HostResolverSystemTask::Start(SystemDnsResultsCallback results_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(results_cb);
  DCHECK(!results_cb_);
  results_cb_ = std::move(results_cb);
  net_log_.BeginEvent(NetLogEventType::HOST_RESOLVER_SYSTEM_TASK);
  StartLookupAttempt();
}

void HostResolverSystemTask::StartLookupAttempt() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!was_completed());
  ++attempt_number_;

  auto lookup_complete_cb =
      base::BindOnce(&HostResolverSystemTask::OnLookupComplete,
                     weak_ptr_factory_.GetWeakPtr(), attempt_number_);

  // If a hook has been installed, call it instead of posting a resolution task
  // to a worker thread.
  if (GetSystemDnsResolverOverride()) {
    GetSystemDnsResolverOverride().Run(hostname_, address_family_, flags_,
                                       std::move(lookup_complete_cb), network_);
  } else {
    base::OnceCallback<int(AddressList * addrlist, int* os_error)> resolve_cb =
        base::BindOnce(&ResolveOnWorkerThread, params_.resolver_proc, hostname_,
                       address_family_, flags_, network_);
    PostSystemDnsResolutionTaskAndReply(std::move(resolve_cb),
                                        std::move(lookup_complete_cb));
  }

  net_log_.AddEventWithIntParams(
      NetLogEventType::HOST_RESOLVER_MANAGER_ATTEMPT_STARTED, "attempt_number",
      attempt_number_);

  // If the results aren't received within a given time, RetryIfNotComplete
  // will start a new attempt if none of the outstanding attempts have
  // completed yet.
  // Use a WeakPtr to avoid keeping the HostResolverSystemTask alive after
  // completion or cancellation.
  if (attempt_number_ <= params_.max_retry_attempts) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&HostResolverSystemTask::StartLookupAttempt,
                       weak_ptr_factory_.GetWeakPtr()),
        params_.unresponsive_delay *
            std::pow(params_.retry_factor, attempt_number_ - 1));
  }
}

// Callback for when DoLookup() completes.
void HostResolverSystemTask::OnLookupComplete(const uint32_t attempt_number,
                                              const AddressList& results,
                                              const int os_error,
                                              int error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!was_completed());

  TRACE_EVENT0(NetTracingCategory(),
               "HostResolverSystemTask::OnLookupComplete");

  // Invalidate WeakPtrs to cancel handling of all outstanding lookup attempts
  // and retries.
  weak_ptr_factory_.InvalidateWeakPtrs();

  // If results are empty, we should return an error.
  bool empty_list_on_ok = (error == OK && results.empty());
  if (empty_list_on_ok)
    error = ERR_NAME_NOT_RESOLVED;

  if (error != OK && NetworkChangeNotifier::IsOffline())
    error = ERR_INTERNET_DISCONNECTED;

  if (error != OK) {
    net_log_.EndEvent(NetLogEventType::HOST_RESOLVER_SYSTEM_TASK, [&] {
      return NetLogHostResolverSystemTaskFailedParams(0, error, os_error);
    });
    net_log_.AddEvent(NetLogEventType::HOST_RESOLVER_MANAGER_ATTEMPT_FINISHED,
                      [&] {
                        return NetLogHostResolverSystemTaskFailedParams(
                            attempt_number, error, os_error);
                      });
  } else {
    net_log_.EndEvent(NetLogEventType::HOST_RESOLVER_SYSTEM_TASK,
                      [&] { return results.NetLogParams(); });
    net_log_.AddEventWithIntParams(
        NetLogEventType::HOST_RESOLVER_MANAGER_ATTEMPT_FINISHED,
        "attempt_number", attempt_number);
  }

  std::move(results_cb_).Run(results, os_error, error);
  // Running |results_cb_| can delete |this|.
}

void EnsureSystemHostResolverCallReady() {
#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_APPLE) && !BUILDFLAG(IS_OPENBSD) && \
    !BUILDFLAG(IS_ANDROID)
  EnsureDnsReloaderInit();
#elif BUILDFLAG(IS_WIN)
  EnsureWinsockInit();
#endif
}

namespace {

int AddressFamilyToAF(AddressFamily address_family) {
  switch (address_family) {
    case ADDRESS_FAMILY_IPV4:
      return AF_INET;
    case ADDRESS_FAMILY_IPV6:
      return AF_INET6;
    case ADDRESS_FAMILY_UNSPECIFIED:
      return AF_UNSPEC;
  }
}

}  // namespace

int SystemHostResolverCall(const std::string& host,
                           AddressFamily address_family,
                           HostResolverFlags host_resolver_flags,
                           AddressList* addrlist,
                           int* os_error_opt,
                           handles::NetworkHandle network) {
  struct addrinfo hints = {0};
  hints.ai_family = AddressFamilyToAF(address_family);

#if BUILDFLAG(IS_WIN)
  // DO NOT USE AI_ADDRCONFIG ON WINDOWS.
  //
  // The following comment in <winsock2.h> is the best documentation I found
  // on AI_ADDRCONFIG for Windows:
  //   Flags used in "hints" argument to getaddrinfo()
  //       - AI_ADDRCONFIG is supported starting with Vista
  //       - default is AI_ADDRCONFIG ON whether the flag is set or not
  //         because the performance penalty in not having ADDRCONFIG in
  //         the multi-protocol stack environment is severe;
  //         this defaulting may be disabled by specifying the AI_ALL flag,
  //         in that case AI_ADDRCONFIG must be EXPLICITLY specified to
  //         enable ADDRCONFIG behavior
  //
  // Not only is AI_ADDRCONFIG unnecessary, but it can be harmful.  If the
  // computer is not connected to a network, AI_ADDRCONFIG causes getaddrinfo
  // to fail with WSANO_DATA (11004) for "localhost", probably because of the
  // following note on AI_ADDRCONFIG in the MSDN getaddrinfo page:
  //   The IPv4 or IPv6 loopback address is not considered a valid global
  //   address.
  // See http://crbug.com/5234.
  //
  // OpenBSD does not support it, either.
  hints.ai_flags = 0;
#else
  hints.ai_flags = AI_ADDRCONFIG;
#endif

  // On Linux AI_ADDRCONFIG doesn't consider loopback addresses, even if only
  // loopback addresses are configured. So don't use it when there are only
  // loopback addresses.
  if (host_resolver_flags & HOST_RESOLVER_LOOPBACK_ONLY)
    hints.ai_flags &= ~AI_ADDRCONFIG;

  if (host_resolver_flags & HOST_RESOLVER_CANONNAME)
    hints.ai_flags |= AI_CANONNAME;

#if BUILDFLAG(IS_WIN)
  // See crbug.com/1176970. Flag not documented (other than the declaration
  // comment in ws2def.h) but confirmed by Microsoft to work for this purpose
  // and be safe.
  if (host_resolver_flags & HOST_RESOLVER_AVOID_MULTICAST)
    hints.ai_flags |= AI_DNS_ONLY;
#endif  // BUILDFLAG(IS_WIN)

  // Restrict result set to only this socket type to avoid duplicates.
  hints.ai_socktype = SOCK_STREAM;

  // This function can block for a long time. Use ScopedBlockingCall to increase
  // the current thread pool's capacity and thus avoid reducing CPU usage by the
  // current process during that time.
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::WILL_BLOCK);

#if BUILDFLAG(IS_POSIX) && \
    !(BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_OPENBSD) || BUILDFLAG(IS_ANDROID))
  DnsReloaderMaybeReload();
#endif
  auto [ai, err, os_error] = AddressInfo::Get(host, hints, nullptr, network);
  bool should_retry = false;
  // If the lookup was restricted (either by address family, or address
  // detection), and the results where all localhost of a single family,
  // maybe we should retry.  There were several bugs related to these
  // issues, for example http://crbug.com/42058 and http://crbug.com/49024
  if ((hints.ai_family != AF_UNSPEC || hints.ai_flags & AI_ADDRCONFIG) && ai &&
      ai->IsAllLocalhostOfOneFamily()) {
    if (host_resolver_flags & HOST_RESOLVER_DEFAULT_FAMILY_SET_DUE_TO_NO_IPV6) {
      hints.ai_family = AF_UNSPEC;
      should_retry = true;
    }
    if (hints.ai_flags & AI_ADDRCONFIG) {
      hints.ai_flags &= ~AI_ADDRCONFIG;
      should_retry = true;
    }
  }
  if (should_retry) {
    std::tie(ai, err, os_error) =
        AddressInfo::Get(host, hints, nullptr, network);
  }

  if (os_error_opt)
    *os_error_opt = os_error;

  if (!ai)
    return err;

  *addrlist = ai->CreateAddressList();
  return OK;
}

void SetSystemDnsResolutionTaskRunnerForTesting(  // IN-TEST
    scoped_refptr<base::TaskRunner> task_runner) {
  GetSystemDnsResolutionTaskRunnerOverride() = task_runner;
}

}  // namespace net
