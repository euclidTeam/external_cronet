// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/host_resolver_manager_service_endpoint_request_impl.h"

#include "base/memory/safe_ref.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/types/optional_util.h"
#include "net/base/net_errors.h"
#include "net/dns/dns_alias_utility.h"
#include "net/dns/dns_task_results_manager.h"
#include "net/dns/host_resolver.h"
#include "net/dns/host_resolver_manager.h"
#include "url/scheme_host_port.h"

namespace net {

HostResolverManager::ServiceEndpointRequestImpl::FinalizedResult::
    FinalizedResult(std::vector<ServiceEndpoint> endpoints,
                    std::set<std::string> dns_aliases)
    : endpoints(std::move(endpoints)), dns_aliases(std::move(dns_aliases)) {}

HostResolverManager::ServiceEndpointRequestImpl::FinalizedResult::
    ~FinalizedResult() = default;

HostResolverManager::ServiceEndpointRequestImpl::FinalizedResult::
    FinalizedResult(FinalizedResult&&) = default;
HostResolverManager::ServiceEndpointRequestImpl::FinalizedResult&
HostResolverManager::ServiceEndpointRequestImpl::FinalizedResult::operator=(
    FinalizedResult&&) = default;

HostResolverManager::ServiceEndpointRequestImpl::ServiceEndpointRequestImpl(
    url::SchemeHostPort scheme_host_port,
    NetworkAnonymizationKey network_anonymization_key,
    NetLogWithSource net_log,
    ResolveHostParameters parameters,
    base::WeakPtr<ResolveContext> resolve_context,
    base::WeakPtr<HostResolverManager> manager,
    const base::TickClock* tick_clock)
    : host_(std::move(scheme_host_port)),
      network_anonymization_key_(
          NetworkAnonymizationKey::IsPartitioningEnabled()
              ? std::move(network_anonymization_key)
              : NetworkAnonymizationKey()),
      net_log_(std::move(net_log)),
      parameters_(std::move(parameters)),
      resolve_context_(std::move(resolve_context)),
      manager_(std::move(manager)),
      tick_clock_(tick_clock),
      priority_(parameters_.initial_priority) {}

HostResolverManager::ServiceEndpointRequestImpl::~ServiceEndpointRequestImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!job_.has_value()) {
    return;
  }

  LogCancelRequest();

  // Clear the delegate to avoid calling delegate's callback after destruction.
  // The following CancelServiceEndpointRequest() could result in calling
  // OnJobCancelled() synchronously.
  delegate_ = nullptr;

  job_.value()->CancelServiceEndpointRequest(this);
}

int HostResolverManager::ServiceEndpointRequestImpl::Start(Delegate* delegate) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!delegate_);
  CHECK(manager_);

  if (!resolve_context_) {
    return ERR_CONTEXT_SHUT_DOWN;
  }

  delegate_ = delegate;

  JobKey job_key(host_, resolve_context_.get());
  IPAddress ip_address;
  manager_->InitializeJobKeyAndIPAddress(
      network_anonymization_key_, parameters_, net_log_, job_key, ip_address);

  // Try to resolve locally first.
  std::optional<HostCache::EntryStaleness> stale_info;
  std::deque<TaskType> tasks;
  HostCache::Entry results = manager_->ResolveLocally(
      /*only_ipv6_reachable=*/false, job_key, ip_address,
      parameters_.cache_usage, parameters_.secure_dns_policy,
      parameters_.source, net_log_, host_cache(), &tasks, &stale_info);
  if (results.error() != ERR_DNS_CACHE_MISS ||
      parameters_.source == HostResolverSource::LOCAL_ONLY || tasks.empty()) {
    SetFinalizedResultFromLegacyResults(results);
    return results.error();
  }

  manager_->CreateAndStartJobForServiceEndpointRequest(std::move(job_key),
                                                       std::move(tasks), this);
  return ERR_IO_PENDING;
}

const std::vector<ServiceEndpoint>&
HostResolverManager::ServiceEndpointRequestImpl::GetEndpointResults() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (finalized_result_.has_value()) {
    return finalized_result_->endpoints;
  }

  if (job_) {
    CHECK(job_.value()->dns_task_results_manager());
    return job_.value()->dns_task_results_manager()->GetCurrentEndpoints();
  }

  NOTREACHED_NORETURN();
}

const std::set<std::string>&
HostResolverManager::ServiceEndpointRequestImpl::GetDnsAliasResults() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (finalized_result_.has_value()) {
    return finalized_result_->dns_aliases;
  }

  if (job_) {
    CHECK(job_.value()->dns_task_results_manager());
    // TODO(crbug.com/41493696): Call dns_alias_utility::FixUpDnsAliases().
    return job_.value()->dns_task_results_manager()->GetAliases();
  }

  NOTREACHED_NORETURN();
}

bool HostResolverManager::ServiceEndpointRequestImpl::EndpointsCryptoReady() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (finalized_result_.has_value()) {
    return true;
  }

  if (job_) {
    CHECK(job_.value()->dns_task_results_manager());
    return job_.value()->dns_task_results_manager()->IsMetadataReady();
  }

  NOTREACHED_NORETURN();
}

void HostResolverManager::ServiceEndpointRequestImpl::AssignJob(
    base::SafeRef<Job> job) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!job_.has_value());
  job_ = job;
}

void HostResolverManager::ServiceEndpointRequestImpl::OnJobCompleted(
    const HostCache::Entry& results,
    bool obtained_securely) {
  CHECK(job_);
  CHECK(delegate_);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  job_.reset();
  SetFinalizedResultFromLegacyResults(results);

  const bool is_secure_network_error =
      obtained_securely && results.error() != OK;
  error_info_ = ResolveErrorInfo(results.error(), is_secure_network_error);
  delegate_->OnServiceEndpointRequestFinished(results.error());
  // Do not add code below. `this` may be deleted at this point.
}

void HostResolverManager::ServiceEndpointRequestImpl::OnJobCancelled() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(job_);

  job_.reset();

  // The owner of `this` has already destroyed `this`.
  if (!delegate_) {
    return;
  }

  LogCancelRequest();

  finalized_result_ = FinalizedResult(/*endpoints=*/{}, /*dns_aliases=*/{});
  error_info_ = ResolveErrorInfo(ERR_DNS_REQUEST_CANCELLED,
                                 /*is_secure_network_error=*/false);
  delegate_->OnServiceEndpointRequestFinished(ERR_DNS_REQUEST_CANCELLED);
  // Do not add code below. `this` may be deleted at this point.
}

void HostResolverManager::ServiceEndpointRequestImpl::
    OnServiceEndpointsChanged() {
  // This method is called asynchronously via a posted task. `job_` could
  // be completed or cancelled before executing the task.
  if (finalized_result_.has_value()) {
    return;
  }

  CHECK(job_);
  CHECK(job_.value()->dns_task_results_manager());
  CHECK(delegate_);
  delegate_->OnServiceEndpointsUpdated();
  // Do not add code below. `this` may be deleted at this point.
}

base::WeakPtr<HostResolverManager::ServiceEndpointRequestImpl>
HostResolverManager::ServiceEndpointRequestImpl::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void HostResolverManager::ServiceEndpointRequestImpl::
    SetFinalizedResultFromLegacyResults(const HostCache::Entry& results) {
  CHECK(!finalized_result_);
  if (results.error() == OK && !parameters_.is_speculative) {
    std::vector<IPEndPoint> ipv4_endpoints;
    std::vector<IPEndPoint> ipv6_endpoints;
    for (const auto& ip_endpoint : results.ip_endpoints()) {
      std::vector<IPEndPoint>& ip_endpoints =
          ip_endpoint.address().IsIPv6() ? ipv6_endpoints : ipv4_endpoints;
      if (ip_endpoint.port() == 0) {
        ip_endpoints.emplace_back(ip_endpoint.address(), host_.GetPort());
      } else {
        ip_endpoints.emplace_back(ip_endpoint);
      }
    }

    // See HostCache::Entry::GetEndpoints.
    std::vector<ServiceEndpoint> endpoints;
    if (!ipv4_endpoints.empty() || !ipv6_endpoints.empty()) {
      for (const auto& metadata : results.GetMetadatas()) {
        if (!base::Contains(results.canonical_names(), metadata.target_name)) {
          continue;
        }

        ServiceEndpoint endpoint;
        endpoint.ipv4_endpoints = ipv4_endpoints;
        endpoint.ipv6_endpoints = ipv6_endpoints;
        endpoint.metadata = metadata;
        endpoints.emplace_back(std::move(endpoint));
      }

      // Append Non-SVCB endpoints at the end for fallback.
      // TODO(crbug.com/41493696): Revisit how to handle non-SVCB endpoints once
      // the connection layer starts using this API. Adding non-SVCB endpoints
      // here might be inconsistent with intermediate results generated by
      // DnsTaskResultsManager, which doesn't append non-SVCB endpoints.
      ServiceEndpoint non_alternative_endpoint;
      non_alternative_endpoint.ipv4_endpoints = ipv4_endpoints;
      non_alternative_endpoint.ipv6_endpoints = ipv6_endpoints;
      endpoints.emplace_back(std::move(non_alternative_endpoint));
    }

    finalized_result_ =
        FinalizedResult(std::move(endpoints),
                        dns_alias_utility::FixUpDnsAliases(results.aliases()));
  } else {
    finalized_result_ = FinalizedResult(/*endpoints=*/{}, /*dns_aliases=*/{});
  }
}

void HostResolverManager::ServiceEndpointRequestImpl::LogCancelRequest() {
  net_log_.AddEvent(NetLogEventType::CANCELLED);
  net_log_.EndEvent(NetLogEventType::HOST_RESOLVER_MANAGER_REQUEST);
}

}  // namespace net
