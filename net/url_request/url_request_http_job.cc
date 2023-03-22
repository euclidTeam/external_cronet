// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/url_request/url_request_http_job.h"

#include <algorithm>
#include <iterator>
#include <memory>
#include <utility>
#include <vector>

#include "base/base_switches.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/check_op.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/containers/adapters.h"
#include "base/file_version_info.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/safe_conversions.h"
#include "base/rand_util.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "base/trace_event/base_tracing.h"
#include "base/types/optional_util.h"
#include "base/values.h"
#include "build/build_config.h"
#include "net/base/features.h"
#include "net/base/host_port_pair.h"
#include "net/base/http_user_agent_settings.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/base/network_anonymization_key.h"
#include "net/base/network_delegate.h"
#include "net/base/network_isolation_key.h"
#include "net/base/privacy_mode.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/base/trace_constants.h"
#include "net/base/url_util.h"
#include "net/cert/cert_status_flags.h"
#include "net/cert/ct_policy_status.h"
#include "net/cert/known_roots.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_access_delegate.h"
#include "net/cookies/cookie_constants.h"
#include "net/cookies/cookie_store.h"
#include "net/cookies/cookie_util.h"
#include "net/cookies/parsed_cookie.h"
#include "net/filter/brotli_source_stream.h"
#include "net/filter/filter_source_stream.h"
#include "net/filter/gzip_source_stream.h"
#include "net/filter/source_stream.h"
#include "net/first_party_sets/first_party_set_metadata.h"
#include "net/first_party_sets/same_party_context.h"
#include "net/http/http_content_disposition.h"
#include "net/http/http_log_util.h"
#include "net/http/http_network_session.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_response_info.h"
#include "net/http/http_status_code.h"
#include "net/http/http_transaction.h"
#include "net/http/http_transaction_factory.h"
#include "net/http/http_util.h"
#include "net/http/transport_security_state.h"
#include "net/log/net_log.h"
#include "net/log/net_log_event_type.h"
#include "net/log/net_log_values.h"
#include "net/log/net_log_with_source.h"
#include "net/nqe/network_quality_estimator.h"
#include "net/proxy_resolution/proxy_info.h"
#include "net/proxy_resolution/proxy_resolution_service.h"
#include "net/proxy_resolution/proxy_retry_info.h"
#include "net/ssl/ssl_cert_request_info.h"
#include "net/ssl/ssl_config_service.h"
#include "net/ssl/ssl_connection_status_flags.h"
#include "net/url_request/redirect_util.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_error_job.h"
#include "net/url_request/url_request_job_factory.h"
#include "net/url_request/url_request_redirect_job.h"
#include "net/url_request/url_request_throttler_manager.h"
#include "net/url_request/websocket_handshake_userdata_key.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/url_constants.h"

#if BUILDFLAG(IS_ANDROID)
#include "net/android/network_library.h"
#endif

namespace {

base::Value CookieInclusionStatusNetLogParams(
    const std::string& operation,
    const std::string& cookie_name,
    const std::string& cookie_domain,
    const std::string& cookie_path,
    const net::CookieInclusionStatus& status,
    net::NetLogCaptureMode capture_mode) {
  base::Value::Dict dict;
  dict.Set("operation", operation);
  dict.Set("status", status.GetDebugString());
  if (net::NetLogCaptureIncludesSensitive(capture_mode)) {
    if (!cookie_name.empty())
      dict.Set("name", cookie_name);
    if (!cookie_domain.empty())
      dict.Set("domain", cookie_domain);
    if (!cookie_path.empty())
      dict.Set("path", cookie_path);
  }
  return base::Value(std::move(dict));
}

// Records details about the most-specific trust anchor in |spki_hashes|,
// which is expected to be ordered with the leaf cert first and the root cert
// last. This complements the per-verification histogram
// Net.Certificate.TrustAnchor.Verify
void LogTrustAnchor(const net::HashValueVector& spki_hashes) {
  // Don't record metrics if there are no hashes; this is true if the HTTP
  // load did not come from an active network connection, such as the disk
  // cache or a synthesized response.
  if (spki_hashes.empty())
    return;

  int32_t id = 0;
  for (const auto& hash : spki_hashes) {
    id = net::GetNetTrustAnchorHistogramIdForSPKI(hash);
    if (id != 0)
      break;
  }
  base::UmaHistogramSparse("Net.Certificate.TrustAnchor.Request", id);
}

net::CookieOptions CreateCookieOptions(
    net::CookieOptions::SameSiteCookieContext same_site_context,
    const net::SamePartyContext& same_party_context,
    const net::IsolationInfo& isolation_info,
    bool is_in_nontrivial_first_party_set) {
  net::CookieOptions options;
  options.set_return_excluded_cookies();
  options.set_include_httponly();
  options.set_same_site_cookie_context(same_site_context);
  options.set_same_party_context(same_party_context);
  if (isolation_info.party_context().has_value()) {
    // Count the top-frame site since it's not in the party_context.
    options.set_full_party_context_size(isolation_info.party_context()->size() +
                                        1);
  }
  options.set_is_in_nontrivial_first_party_set(
      is_in_nontrivial_first_party_set);
  return options;
}

bool IsTLS13OverTCP(const net::HttpResponseInfo& response_info) {
  // Although IETF QUIC also uses TLS 1.3, our QUIC connections report
  // SSL_CONNECTION_VERSION_QUIC.
  return net::SSLConnectionStatusToVersion(
             response_info.ssl_info.connection_status) ==
         net::SSL_CONNECTION_VERSION_TLS1_3;
}

GURL UpgradeSchemeToCryptographic(const GURL& insecure_url) {
  DCHECK(!insecure_url.SchemeIsCryptographic());
  DCHECK(insecure_url.SchemeIs(url::kHttpScheme) ||
         insecure_url.SchemeIs(url::kWsScheme));

  GURL::Replacements replacements;
  replacements.SetSchemeStr(insecure_url.SchemeIs(url::kHttpScheme)
                                ? url::kHttpsScheme
                                : url::kWssScheme);

  GURL secure_url = insecure_url.ReplaceComponents(replacements);
  DCHECK(secure_url.SchemeIsCryptographic());

  return secure_url;
}

}  // namespace

namespace net {

std::unique_ptr<URLRequestJob> URLRequestHttpJob::Create(URLRequest* request) {
  const GURL& url = request->url();

  // URLRequestContext must have been initialized.
  DCHECK(request->context()->http_transaction_factory());
  DCHECK(url.SchemeIsHTTPOrHTTPS() || url.SchemeIsWSOrWSS());

  // Check for reasons not to return a URLRequestHttpJob. These don't apply to
  // https and wss requests.
  if (!url.SchemeIsCryptographic()) {
    // Check for HSTS upgrade.
    TransportSecurityState* hsts =
        request->context()->transport_security_state();
    if (hsts && hsts->ShouldUpgradeToSSL(url.host(), request->net_log())) {
      return std::make_unique<URLRequestRedirectJob>(
          request, UpgradeSchemeToCryptographic(url),
          // Use status code 307 to preserve the method, so POST requests work.
          RedirectUtil::ResponseCode::REDIRECT_307_TEMPORARY_REDIRECT, "HSTS");
    }

#if BUILDFLAG(IS_ANDROID)
    // Check whether the app allows cleartext traffic to this host, and return
    // ERR_CLEARTEXT_NOT_PERMITTED if not.
    if (request->context()->check_cleartext_permitted() &&
        !android::IsCleartextPermitted(url.host())) {
      return std::make_unique<URLRequestErrorJob>(request,
                                                  ERR_CLEARTEXT_NOT_PERMITTED);
    }
#endif
  }

  return base::WrapUnique<URLRequestJob>(new URLRequestHttpJob(
      request, request->context()->http_user_agent_settings()));
}

URLRequestHttpJob::URLRequestHttpJob(
    URLRequest* request,
    const HttpUserAgentSettings* http_user_agent_settings)
    : URLRequestJob(request),
      http_user_agent_settings_(http_user_agent_settings) {
  URLRequestThrottlerManager* manager = request->context()->throttler_manager();
  if (manager)
    throttling_entry_ = manager->RegisterRequestUrl(request->url());

  ResetTimer();
}

URLRequestHttpJob::~URLRequestHttpJob() {
  CHECK(!awaiting_callback_);

  DoneWithRequest(ABORTED);
}

void URLRequestHttpJob::SetPriority(RequestPriority priority) {
  priority_ = priority;
  if (transaction_)
    transaction_->SetPriority(priority_);
}

void URLRequestHttpJob::Start() {
  DCHECK(!transaction_.get());

  request_info_.url = request_->url();
  request_info_.method = request_->method();

  request_info_.network_isolation_key =
      request_->isolation_info().network_isolation_key();
  request_info_.network_anonymization_key =
      request_->isolation_info().network_anonymization_key();
  request_info_.possibly_top_frame_origin =
      request_->isolation_info().top_frame_origin();
  request_info_.is_subframe_document_resource =
      request_->isolation_info().request_type() ==
      net::IsolationInfo::RequestType::kSubFrame;
  request_info_.load_flags = request_->load_flags();
  request_info_.secure_dns_policy = request_->secure_dns_policy();
  request_info_.traffic_annotation =
      net::MutableNetworkTrafficAnnotationTag(request_->traffic_annotation());
  request_info_.socket_tag = request_->socket_tag();
  request_info_.idempotency = request_->GetIdempotency();
  request_info_.pervasive_payloads_index_for_logging =
      request_->pervasive_payloads_index_for_logging();
  request_info_.checksum = request_->expected_response_checksum();
#if BUILDFLAG(ENABLE_REPORTING)
  request_info_.reporting_upload_depth = request_->reporting_upload_depth();
#endif

  bool should_add_cookie_header = ShouldAddCookieHeader();
  UMA_HISTOGRAM_BOOLEAN("Net.HttpJob.CanIncludeCookies",
                        should_add_cookie_header);

  if (!should_add_cookie_header) {
    OnGotFirstPartySetMetadata(FirstPartySetMetadata());
    return;
  }
  absl::optional<FirstPartySetMetadata> metadata =
      cookie_util::ComputeFirstPartySetMetadataMaybeAsync(
          SchemefulSite(request()->url()), request()->isolation_info(),
          request()->context()->cookie_store()->cookie_access_delegate(),
          request()->force_ignore_top_frame_party_for_cookies(),
          base::BindOnce(&URLRequestHttpJob::OnGotFirstPartySetMetadata,
                         weak_factory_.GetWeakPtr()));

  if (metadata.has_value())
    OnGotFirstPartySetMetadata(std::move(metadata.value()));
}

void URLRequestHttpJob::OnGotFirstPartySetMetadata(
    FirstPartySetMetadata first_party_set_metadata) {
  first_party_set_metadata_ = std::move(first_party_set_metadata);

  if (!request()->network_delegate()) {
    OnGotFirstPartySetCacheFilterMatchInfo(
        net::FirstPartySetsCacheFilter::MatchInfo());
    return;
  }
  absl::optional<FirstPartySetsCacheFilter::MatchInfo> match_info =
      request()
          ->network_delegate()
          ->GetFirstPartySetsCacheFilterMatchInfoMaybeAsync(
              SchemefulSite(request()->url()),
              base::BindOnce(
                  &URLRequestHttpJob::OnGotFirstPartySetCacheFilterMatchInfo,
                  weak_factory_.GetWeakPtr()));

  if (match_info.has_value())
    OnGotFirstPartySetCacheFilterMatchInfo(std::move(match_info.value()));
}

void URLRequestHttpJob::OnGotFirstPartySetCacheFilterMatchInfo(
    FirstPartySetsCacheFilter::MatchInfo match_info) {
  request_info_.fps_cache_filter = match_info.clear_at_run_id;
  request_info_.browser_run_id = match_info.browser_run_id;

  // Privacy mode could still be disabled in SetCookieHeaderAndStart if we are
  // going to send previously saved cookies.
  request_info_.privacy_mode = DeterminePrivacyMode();
  request()->net_log().AddEventWithStringParams(
      NetLogEventType::COMPUTED_PRIVACY_MODE, "privacy_mode",
      PrivacyModeToDebugString(request_info_.privacy_mode));

  // Strip Referer from request_info_.extra_headers to prevent, e.g., plugins
  // from overriding headers that are controlled using other means. Otherwise a
  // plugin could set a referrer although sending the referrer is inhibited.
  request_info_.extra_headers.RemoveHeader(HttpRequestHeaders::kReferer);

  // URLRequest::SetReferrer ensures that we do not send username and password
  // fields in the referrer.
  GURL referrer(request_->referrer());

  // Our consumer should have made sure that this is a safe referrer (e.g. via
  // URLRequestJob::ComputeReferrerForPolicy).
  if (referrer.is_valid()) {
    std::string referer_value = referrer.spec();
    request_info_.extra_headers.SetHeader(HttpRequestHeaders::kReferer,
                                          referer_value);
  }

  request_info_.extra_headers.SetHeaderIfMissing(
      HttpRequestHeaders::kUserAgent,
      http_user_agent_settings_ ?
          http_user_agent_settings_->GetUserAgent() : std::string());

  AddExtraHeaders();

  if (ShouldAddCookieHeader()) {
    // We shouldn't overwrite this if we've already computed the key.
    DCHECK(!cookie_partition_key_.has_value());

    cookie_partition_key_ = CookiePartitionKey::FromNetworkIsolationKey(
        request_->isolation_info().network_isolation_key());
    AddCookieHeaderAndStart();
  } else {
    StartTransaction();
  }
}

void URLRequestHttpJob::Kill() {
  weak_factory_.InvalidateWeakPtrs();
  if (transaction_)
    DestroyTransaction();
  URLRequestJob::Kill();
}

ConnectionAttempts URLRequestHttpJob::GetConnectionAttempts() const {
  if (transaction_)
    return transaction_->GetConnectionAttempts();
  return {};
}

void URLRequestHttpJob::CloseConnectionOnDestruction() {
  DCHECK(transaction_);
  transaction_->CloseConnectionOnDestruction();
}

int URLRequestHttpJob::NotifyConnectedCallback(
    const TransportInfo& info,
    CompletionOnceCallback callback) {
  return URLRequestJob::NotifyConnected(info, std::move(callback));
}

PrivacyMode URLRequestHttpJob::DeterminePrivacyMode() const {
  if (!request()->allow_credentials()) {
    // |allow_credentials_| implies LOAD_DO_NOT_SAVE_COOKIES.
    DCHECK(request_->load_flags() & LOAD_DO_NOT_SAVE_COOKIES);

    // TODO(https://crbug.com/775438): Client certs should always be
    // affirmatively omitted for these requests.
    return request()->send_client_certs()
               ? PRIVACY_MODE_ENABLED
               : PRIVACY_MODE_ENABLED_WITHOUT_CLIENT_CERTS;
  }

  // Otherwise, check with the delegate if present, or base it off of
  // |URLRequest::DefaultCanUseCookies()| if not.
  // TODO(mmenke): Looks like |URLRequest::DefaultCanUseCookies()| is not too
  // useful, with the network service - remove it.
  NetworkDelegate::PrivacySetting privacy_setting =
      URLRequest::DefaultCanUseCookies()
          ? NetworkDelegate::PrivacySetting::kStateAllowed
          : NetworkDelegate::PrivacySetting::kStateDisallowed;
  if (request_->network_delegate()) {
    privacy_setting = request()->network_delegate()->ForcePrivacyMode(
        request_->url(), request_->site_for_cookies(),
        request_->isolation_info().top_frame_origin(),
        first_party_set_metadata_.context().context_type());
  }
  switch (privacy_setting) {
    case NetworkDelegate::PrivacySetting::kStateAllowed:
      return PRIVACY_MODE_DISABLED;
    case NetworkDelegate::PrivacySetting::kPartitionedStateAllowedOnly:
      return PRIVACY_MODE_ENABLED_PARTITIONED_STATE_ALLOWED;
    case NetworkDelegate::PrivacySetting::kStateDisallowed:
      return PRIVACY_MODE_ENABLED;
  }
  NOTREACHED();
  return PRIVACY_MODE_ENABLED;
}

void URLRequestHttpJob::NotifyHeadersComplete() {
  DCHECK(!response_info_);
  DCHECK_EQ(0, num_cookie_lines_left_);
  DCHECK(request_->maybe_stored_cookies().empty());

  if (override_response_info_) {
    DCHECK(!transaction_);
    response_info_ = override_response_info_.get();
  } else {
    response_info_ = transaction_->GetResponseInfo();
  }

  if (!response_info_->was_cached && throttling_entry_.get())
    throttling_entry_->UpdateWithResponse(GetResponseCode());

  ProcessStrictTransportSecurityHeader();

  // Clear |set_cookie_access_result_list_| after any processing in case
  // SaveCookiesAndNotifyHeadersComplete is called again.
  request_->set_maybe_stored_cookies(std::move(set_cookie_access_result_list_));

  // The HTTP transaction may be restarted several times for the purposes
  // of sending authorization information. Each time it restarts, we get
  // notified of the headers completion so that we can update the cookie store.
  if (transaction_ && transaction_->IsReadyToRestartForAuth()) {
    // TODO(battre): This breaks the webrequest API for
    // URLRequestTestHTTP.BasicAuthWithCookies
    // where OnBeforeStartTransaction -> OnStartTransaction ->
    // OnBeforeStartTransaction occurs.
    RestartTransactionWithAuth(AuthCredentials());
    return;
  }

  URLRequestJob::NotifyHeadersComplete();
}

void URLRequestHttpJob::DestroyTransaction() {
  DCHECK(transaction_.get());

  DoneWithRequest(ABORTED);

  total_received_bytes_from_previous_transactions_ +=
      transaction_->GetTotalReceivedBytes();
  total_sent_bytes_from_previous_transactions_ +=
      transaction_->GetTotalSentBytes();
  response_info_ = nullptr;
  transaction_.reset();
  override_response_headers_ = nullptr;
  receive_headers_end_ = base::TimeTicks();
}

void URLRequestHttpJob::StartTransaction() {
  DCHECK(!override_response_info_);

  NetworkDelegate* network_delegate = request()->network_delegate();
  if (network_delegate) {
    OnCallToDelegate(
        NetLogEventType::NETWORK_DELEGATE_BEFORE_START_TRANSACTION);
    int rv = network_delegate->NotifyBeforeStartTransaction(
        request_, request_info_.extra_headers,
        base::BindOnce(&URLRequestHttpJob::NotifyBeforeStartTransactionCallback,
                       weak_factory_.GetWeakPtr()));
    // If an extension blocks the request, we rely on the callback to
    // MaybeStartTransactionInternal().
    if (rv == ERR_IO_PENDING)
      return;
    MaybeStartTransactionInternal(rv);
    return;
  }
  StartTransactionInternal();
}

void URLRequestHttpJob::NotifyBeforeStartTransactionCallback(
    int result,
    const absl::optional<HttpRequestHeaders>& headers) {
  // The request should not have been cancelled or have already completed.
  DCHECK(!is_done());

  if (headers)
    request_info_.extra_headers = headers.value();
  MaybeStartTransactionInternal(result);
}

void URLRequestHttpJob::MaybeStartTransactionInternal(int result) {
  OnCallToDelegateComplete();
  if (result == OK) {
    StartTransactionInternal();
  } else {
    request_->net_log().AddEventWithStringParams(NetLogEventType::CANCELLED,
                                                 "source", "delegate");
    // Don't call back synchronously to the delegate.
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&URLRequestHttpJob::NotifyStartError,
                                  weak_factory_.GetWeakPtr(), result));
  }
}

void URLRequestHttpJob::StartTransactionInternal() {
  DCHECK(!override_response_headers_);

  // NOTE: This method assumes that request_info_ is already setup properly.

  // If we already have a transaction, then we should restart the transaction
  // with auth provided by auth_credentials_.

  int rv;

  // Notify NetworkQualityEstimator.
  NetworkQualityEstimator* network_quality_estimator =
      request()->context()->network_quality_estimator();
  if (network_quality_estimator)
    network_quality_estimator->NotifyStartTransaction(*request_);

  if (transaction_.get()) {
    rv = transaction_->RestartWithAuth(
        auth_credentials_, base::BindOnce(&URLRequestHttpJob::OnStartCompleted,
                                          base::Unretained(this)));
    auth_credentials_ = AuthCredentials();
  } else {
    DCHECK(request_->context()->http_transaction_factory());

    rv = request_->context()->http_transaction_factory()->CreateTransaction(
        priority_, &transaction_);

    if (rv == OK && request_info_.url.SchemeIsWSOrWSS()) {
      base::SupportsUserData::Data* data =
          request_->GetUserData(kWebSocketHandshakeUserDataKey);
      if (data) {
        transaction_->SetWebSocketHandshakeStreamCreateHelper(
            static_cast<WebSocketHandshakeStreamBase::CreateHelper*>(data));
      } else {
        rv = ERR_DISALLOWED_URL_SCHEME;
      }
    }

    if (rv == OK && request_info_.method == "CONNECT") {
      // CONNECT has different kinds of targets than other methods (RFC 9110,
      // section 9.3.6), which are incompatible with URLRequest.
      rv = ERR_METHOD_NOT_SUPPORTED;
    }

    if (rv == OK) {
      transaction_->SetConnectedCallback(base::BindRepeating(
          &URLRequestHttpJob::NotifyConnectedCallback, base::Unretained(this)));
      transaction_->SetRequestHeadersCallback(request_headers_callback_);
      transaction_->SetEarlyResponseHeadersCallback(
          early_response_headers_callback_);
      transaction_->SetResponseHeadersCallback(response_headers_callback_);

      if (!throttling_entry_.get() ||
          !throttling_entry_->ShouldRejectRequest(*request_)) {
        rv = transaction_->Start(
            &request_info_,
            base::BindOnce(&URLRequestHttpJob::OnStartCompleted,
                           base::Unretained(this)),
            request_->net_log());
        start_time_ = base::TimeTicks::Now();
      } else {
        // Special error code for the exponential back-off module.
        rv = ERR_TEMPORARILY_THROTTLED;
      }
    }
  }

  if (rv == ERR_IO_PENDING)
    return;

  // The transaction started synchronously, but we need to notify the
  // URLRequest delegate via the message loop.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&URLRequestHttpJob::OnStartCompleted,
                                weak_factory_.GetWeakPtr(), rv));
}

void URLRequestHttpJob::AddExtraHeaders() {
  request_info_.extra_headers.SetAcceptEncodingIfMissing(
      request()->url(), request()->accepted_stream_types(),
      request()->context()->enable_brotli());

  if (http_user_agent_settings_) {
    // Only add default Accept-Language if the request didn't have it
    // specified.
    std::string accept_language =
        http_user_agent_settings_->GetAcceptLanguage();
    if (!accept_language.empty()) {
      request_info_.extra_headers.SetHeaderIfMissing(
          HttpRequestHeaders::kAcceptLanguage,
          accept_language);
    }
  }
}

void URLRequestHttpJob::AddCookieHeaderAndStart() {
  CookieStore* cookie_store = request_->context()->cookie_store();
  DCHECK(cookie_store);
  DCHECK(ShouldAddCookieHeader());
  bool force_ignore_site_for_cookies =
      request_->force_ignore_site_for_cookies();
  if (cookie_store->cookie_access_delegate() &&
      cookie_store->cookie_access_delegate()->ShouldIgnoreSameSiteRestrictions(
          request_->url(), request_->site_for_cookies())) {
    force_ignore_site_for_cookies = true;
  }
  bool is_main_frame_navigation =
      IsolationInfo::RequestType::kMainFrame ==
          request_->isolation_info().request_type() ||
      request_->force_main_frame_for_same_site_cookies();
  CookieOptions::SameSiteCookieContext same_site_context =
      net::cookie_util::ComputeSameSiteContextForRequest(
          request_->method(), request_->url_chain(),
          request_->site_for_cookies(), request_->initiator(),
          is_main_frame_navigation, force_ignore_site_for_cookies);

  bool is_in_nontrivial_first_party_set =
      first_party_set_metadata_.frame_entry().has_value();
  CookieOptions options = CreateCookieOptions(
      same_site_context, first_party_set_metadata_.context(),
      request_->isolation_info(), is_in_nontrivial_first_party_set);

  cookie_store->GetCookieListWithOptionsAsync(
      request_->url(), options,
      CookiePartitionKeyCollection::FromOptional(cookie_partition_key_.value()),
      base::BindOnce(&URLRequestHttpJob::SetCookieHeaderAndStart,
                     weak_factory_.GetWeakPtr(), options));
}

namespace {

bool ShouldBlockAllCookies(const PrivacyMode& privacy_mode) {
  return privacy_mode == PRIVACY_MODE_ENABLED ||
         privacy_mode == PRIVACY_MODE_ENABLED_WITHOUT_CLIENT_CERTS;
}

bool ShouldBlockUnpartitionedCookiesOnly(const PrivacyMode& privacy_mode) {
  return privacy_mode == PRIVACY_MODE_ENABLED_PARTITIONED_STATE_ALLOWED;
}

}  // namespace

void URLRequestHttpJob::SetCookieHeaderAndStart(
    const CookieOptions& options,
    const CookieAccessResultList& cookies_with_access_result_list,
    const CookieAccessResultList& excluded_list) {
  DCHECK(request_->maybe_sent_cookies().empty());

  CookieAccessResultList maybe_included_cookies =
      cookies_with_access_result_list;
  CookieAccessResultList excluded_cookies = excluded_list;

  if (ShouldBlockAllCookies(request_info_.privacy_mode)) {
    // If cookies are blocked (without our needing to consult the delegate),
    // we move them to `excluded_cookies` and ensure that they have the
    // correct exclusion reason.
    excluded_cookies.insert(
        excluded_cookies.end(),
        std::make_move_iterator(maybe_included_cookies.begin()),
        std::make_move_iterator(maybe_included_cookies.end()));
    maybe_included_cookies.clear();
    for (auto& cookie : excluded_cookies) {
      cookie.access_result.status.AddExclusionReason(
          CookieInclusionStatus::EXCLUDE_USER_PREFERENCES);
    }
  }
  if (ShouldBlockUnpartitionedCookiesOnly(request_info_.privacy_mode)) {
    auto partition_it = base::ranges::stable_partition(
        maybe_included_cookies, [](const CookieWithAccessResult& el) {
          return el.cookie.IsPartitioned();
        });
    for (auto it = partition_it; it < maybe_included_cookies.end(); ++it) {
      it->access_result.status.AddExclusionReason(
          CookieInclusionStatus::EXCLUDE_USER_PREFERENCES);
      if (first_party_set_metadata_.AreSitesInSameFirstPartySet()) {
        it->access_result.status.AddExclusionReason(
            CookieInclusionStatus::
                EXCLUDE_THIRD_PARTY_BLOCKED_WITHIN_FIRST_PARTY_SET);
      }
    }
    excluded_cookies.insert(
        excluded_cookies.end(), std::make_move_iterator(partition_it),
        std::make_move_iterator(maybe_included_cookies.end()));
    maybe_included_cookies.erase(partition_it, maybe_included_cookies.end());
  }
  if (request_info_.privacy_mode == PRIVACY_MODE_DISABLED ||
      !maybe_included_cookies.empty()) {
    AnnotateAndMoveUserBlockedCookies(maybe_included_cookies, excluded_cookies);
    if (!maybe_included_cookies.empty()) {
      std::string cookie_line =
          CanonicalCookie::BuildCookieLine(maybe_included_cookies);
      request_info_.extra_headers.SetHeader(HttpRequestHeaders::kCookie,
                                            cookie_line);

      size_t n_partitioned_cookies = 0;

      // TODO(crbug.com/1031664): Reduce the number of times the cookie list
      // is iterated over. Get metrics for every cookie which is included.
      for (const auto& c : maybe_included_cookies) {
        bool request_is_secure = request_->url().SchemeIsCryptographic();
        net::CookieSourceScheme cookie_scheme = c.cookie.SourceScheme();
        CookieRequestScheme cookie_request_schemes;

        switch (cookie_scheme) {
          case net::CookieSourceScheme::kSecure:
            cookie_request_schemes =
                request_is_secure
                    ? CookieRequestScheme::kSecureSetSecureRequest
                    : CookieRequestScheme::kSecureSetNonsecureRequest;
            break;

          case net::CookieSourceScheme::kNonSecure:
            cookie_request_schemes =
                request_is_secure
                    ? CookieRequestScheme::kNonsecureSetSecureRequest
                    : CookieRequestScheme::kNonsecureSetNonsecureRequest;
            break;

          case net::CookieSourceScheme::kUnset:
            cookie_request_schemes = CookieRequestScheme::kUnsetCookieScheme;
            break;
        }

        UMA_HISTOGRAM_ENUMERATION("Cookie.CookieSchemeRequestScheme",
                                  cookie_request_schemes);
        if (c.cookie.IsPartitioned()) {
          ++n_partitioned_cookies;
        }
      }

      if (IsPartitionedCookiesEnabled()) {
        base::UmaHistogramCounts100("Cookie.PartitionedCookiesInRequest",
                                    n_partitioned_cookies);
      }
    }
  }

  CookieAccessResultList maybe_sent_cookies = std::move(excluded_cookies);
  maybe_sent_cookies.insert(
      maybe_sent_cookies.end(),
      std::make_move_iterator(maybe_included_cookies.begin()),
      std::make_move_iterator(maybe_included_cookies.end()));
  maybe_included_cookies.clear();

  if (request_->net_log().IsCapturing()) {
    for (const auto& cookie_with_access_result : maybe_sent_cookies) {
      request_->net_log().AddEvent(
          NetLogEventType::COOKIE_INCLUSION_STATUS,
          [&](NetLogCaptureMode capture_mode) {
            return CookieInclusionStatusNetLogParams(
                "send", cookie_with_access_result.cookie.Name(),
                cookie_with_access_result.cookie.Domain(),
                cookie_with_access_result.cookie.Path(),
                cookie_with_access_result.access_result.status, capture_mode);
          });
    }
  }

  request_->set_maybe_sent_cookies(std::move(maybe_sent_cookies));

  StartTransaction();
}

void URLRequestHttpJob::AnnotateAndMoveUserBlockedCookies(
    CookieAccessResultList& maybe_included_cookies,
    CookieAccessResultList& excluded_cookies) const {
  DCHECK(request_info_.privacy_mode == PrivacyMode::PRIVACY_MODE_DISABLED ||
         (request_info_.privacy_mode ==
              PrivacyMode::PRIVACY_MODE_ENABLED_PARTITIONED_STATE_ALLOWED &&
          base::ranges::all_of(maybe_included_cookies,
                               [](const CookieWithAccessResult& el) {
                                 return el.cookie.IsPartitioned();
                               })))
      << request_info_.privacy_mode;

  bool can_get_cookies = URLRequest::DefaultCanUseCookies();
  if (request()->network_delegate()) {
    can_get_cookies =
        request()->network_delegate()->AnnotateAndMoveUserBlockedCookies(
            *request(), first_party_set_metadata_, maybe_included_cookies,
            excluded_cookies);
  }

  if (!can_get_cookies) {
    request()->net_log().AddEvent(
        NetLogEventType::COOKIE_GET_BLOCKED_BY_NETWORK_DELEGATE);
  }
}

void URLRequestHttpJob::SaveCookiesAndNotifyHeadersComplete(int result) {
  DCHECK(set_cookie_access_result_list_.empty());
  // TODO(crbug.com/1186863): Turn this CHECK into DCHECK once the investigation
  // is done.
  CHECK_EQ(0, num_cookie_lines_left_);

  // End of the call started in OnStartCompleted.
  OnCallToDelegateComplete();

  if (result != OK) {
    request_->net_log().AddEventWithStringParams(NetLogEventType::CANCELLED,
                                                 "source", "delegate");
    NotifyStartError(result);
    return;
  }

  CookieStore* cookie_store = request_->context()->cookie_store();

  if ((request_info_.load_flags & LOAD_DO_NOT_SAVE_COOKIES) || !cookie_store) {
    NotifyHeadersComplete();
    return;
  }

  base::Time response_date;
  absl::optional<base::Time> server_time = absl::nullopt;
  if (GetResponseHeaders()->GetDateValue(&response_date))
    server_time = absl::make_optional(response_date);

  bool force_ignore_site_for_cookies =
      request_->force_ignore_site_for_cookies();
  if (cookie_store->cookie_access_delegate() &&
      cookie_store->cookie_access_delegate()->ShouldIgnoreSameSiteRestrictions(
          request_->url(), request_->site_for_cookies())) {
    force_ignore_site_for_cookies = true;
  }
  bool is_main_frame_navigation =
      IsolationInfo::RequestType::kMainFrame ==
          request_->isolation_info().request_type() ||
      request_->force_main_frame_for_same_site_cookies();
  CookieOptions::SameSiteCookieContext same_site_context =
      net::cookie_util::ComputeSameSiteContextForResponse(
          request_->url_chain(), request_->site_for_cookies(),
          request_->initiator(), is_main_frame_navigation,
          force_ignore_site_for_cookies);

  bool is_in_nontrivial_first_party_set =
      first_party_set_metadata_.frame_entry().has_value();
  CookieOptions options = CreateCookieOptions(
      same_site_context, first_party_set_metadata_.context(),
      request_->isolation_info(), is_in_nontrivial_first_party_set);

  // Set all cookies, without waiting for them to be set. Any subsequent
  // read will see the combined result of all cookie operation.
  const base::StringPiece name("Set-Cookie");
  std::string cookie_string;
  size_t iter = 0;
  HttpResponseHeaders* headers = GetResponseHeaders();

  // NotifyHeadersComplete needs to be called once and only once after the
  // list has been fully processed, and it can either be called in the
  // callback or after the loop is called, depending on how the last element
  // was handled. |num_cookie_lines_left_| keeps track of how many async
  // callbacks are currently out (starting from 1 to make sure the loop runs
  // all the way through before trying to exit). If there are any callbacks
  // still waiting when the loop ends, then NotifyHeadersComplete will be
  // called when it reaches 0 in the callback itself.
  num_cookie_lines_left_ = 1;
  while (headers->EnumerateHeader(&iter, name, &cookie_string)) {
    CookieInclusionStatus returned_status;

    num_cookie_lines_left_++;

    std::unique_ptr<CanonicalCookie> cookie = net::CanonicalCookie::Create(
        request_->url(), cookie_string, base::Time::Now(), server_time,
        cookie_partition_key_.value(), &returned_status);

    absl::optional<CanonicalCookie> cookie_to_return = absl::nullopt;
    if (returned_status.IsInclude()) {
      DCHECK(cookie);
      // Make a copy of the cookie if we successfully made one.
      cookie_to_return = *cookie;
    }
    if (cookie && !CanSetCookie(*cookie, &options)) {
      returned_status.AddExclusionReason(
          CookieInclusionStatus::EXCLUDE_USER_PREFERENCES);
    }
    if (!returned_status.IsInclude()) {
      OnSetCookieResult(options, cookie_to_return, std::move(cookie_string),
                        CookieAccessResult(returned_status));
      continue;
    }
    CookieAccessResult cookie_access_result(returned_status);
    cookie_store->SetCanonicalCookieAsync(
        std::move(cookie), request_->url(), options,
        base::BindOnce(&URLRequestHttpJob::OnSetCookieResult,
                       weak_factory_.GetWeakPtr(), options, cookie_to_return,
                       cookie_string),
        std::move(cookie_access_result));
  }
  // Removing the 1 that |num_cookie_lines_left| started with, signifing that
  // loop has been exited.
  num_cookie_lines_left_--;

  if (num_cookie_lines_left_ == 0)
    NotifyHeadersComplete();
}

void URLRequestHttpJob::OnSetCookieResult(
    const CookieOptions& options,
    absl::optional<CanonicalCookie> cookie,
    std::string cookie_string,
    CookieAccessResult access_result) {
  if (request_->net_log().IsCapturing()) {
    request_->net_log().AddEvent(NetLogEventType::COOKIE_INCLUSION_STATUS,
                                 [&](NetLogCaptureMode capture_mode) {
                                   return CookieInclusionStatusNetLogParams(
                                       "store",
                                       cookie ? cookie.value().Name() : "",
                                       cookie ? cookie.value().Domain() : "",
                                       cookie ? cookie.value().Path() : "",
                                       access_result.status, capture_mode);
                                 });
  }

  set_cookie_access_result_list_.emplace_back(
      std::move(cookie), std::move(cookie_string), access_result);

  num_cookie_lines_left_--;

  // If all the cookie lines have been handled, |set_cookie_access_result_list_|
  // now reflects the result of all Set-Cookie lines, and the request can be
  // continued.
  if (num_cookie_lines_left_ == 0)
    NotifyHeadersComplete();
}

void URLRequestHttpJob::ProcessStrictTransportSecurityHeader() {
  DCHECK(response_info_);
  TransportSecurityState* security_state =
      request_->context()->transport_security_state();
  const SSLInfo& ssl_info = response_info_->ssl_info;

  // Only accept HSTS headers on HTTPS connections that have no
  // certificate errors.
  if (!ssl_info.is_valid() || IsCertStatusError(ssl_info.cert_status) ||
      !security_state) {
    return;
  }

  // Don't accept HSTS headers when the hostname is an IP address.
  if (request_info_.url.HostIsIPAddress())
    return;

  // http://tools.ietf.org/html/draft-ietf-websec-strict-transport-sec:
  //
  //   If a UA receives more than one STS header field in a HTTP response
  //   message over secure transport, then the UA MUST process only the
  //   first such header field.
  HttpResponseHeaders* headers = GetResponseHeaders();
  std::string value;
  if (headers->EnumerateHeader(nullptr, "Strict-Transport-Security", &value))
    security_state->AddHSTSHeader(request_info_.url.host(), value);
}

void URLRequestHttpJob::OnStartCompleted(int result) {
  TRACE_EVENT0(NetTracingCategory(), "URLRequestHttpJob::OnStartCompleted");
  RecordTimer();

  // If the job is done (due to cancellation), can just ignore this
  // notification.
  if (done_)
    return;

  receive_headers_end_ = base::TimeTicks::Now();

  const URLRequestContext* context = request_->context();

  if (transaction_ && transaction_->GetResponseInfo()) {
    const SSLInfo& ssl_info = transaction_->GetResponseInfo()->ssl_info;
    if (!IsCertificateError(result)) {
      LogTrustAnchor(ssl_info.public_key_hashes);
    }
  }

  if (transaction_ && transaction_->GetResponseInfo()) {
    SetProxyServer(transaction_->GetResponseInfo()->proxy_server);
  }

  if (result == OK) {
    scoped_refptr<HttpResponseHeaders> headers = GetResponseHeaders();

    NetworkDelegate* network_delegate = request()->network_delegate();
    if (network_delegate) {
      // Note that |this| may not be deleted until
      // |URLRequestHttpJob::OnHeadersReceivedCallback()| or
      // |NetworkDelegate::URLRequestDestroyed()| has been called.
      OnCallToDelegate(NetLogEventType::NETWORK_DELEGATE_HEADERS_RECEIVED);
      preserve_fragment_on_redirect_url_ = absl::nullopt;
      IPEndPoint endpoint;
      if (transaction_)
        transaction_->GetRemoteEndpoint(&endpoint);
      // The NetworkDelegate must watch for OnRequestDestroyed and not modify
      // any of the arguments after it's called.
      // TODO(mattm): change the API to remove the out-params and take the
      // results as params of the callback.
      int error = network_delegate->NotifyHeadersReceived(
          request_,
          base::BindOnce(&URLRequestHttpJob::OnHeadersReceivedCallback,
                         weak_factory_.GetWeakPtr()),
          headers.get(), &override_response_headers_, endpoint,
          &preserve_fragment_on_redirect_url_);
      if (error != OK) {
        if (error == ERR_IO_PENDING) {
          awaiting_callback_ = true;
        } else {
          request_->net_log().AddEventWithStringParams(
              NetLogEventType::CANCELLED, "source", "delegate");
          OnCallToDelegateComplete();
          NotifyStartError(error);
        }
        return;
      }
    }

    SaveCookiesAndNotifyHeadersComplete(OK);
  } else if (IsCertificateError(result)) {
    // We encountered an SSL certificate error.
    // Maybe overridable, maybe not. Ask the delegate to decide.
    TransportSecurityState* state = context->transport_security_state();
    NotifySSLCertificateError(
        result, transaction_->GetResponseInfo()->ssl_info,
        state->ShouldSSLErrorsBeFatal(request_info_.url.host()) &&
            result != ERR_CERT_KNOWN_INTERCEPTION_BLOCKED);
  } else if (result == ERR_SSL_CLIENT_AUTH_CERT_NEEDED) {
    NotifyCertificateRequested(
        transaction_->GetResponseInfo()->cert_request_info.get());
  } else if (result == ERR_DNS_NAME_HTTPS_ONLY) {
    // If DNS indicated the name is HTTPS-only, synthesize a redirect to either
    // HTTPS or WSS.
    DCHECK(!request_->url().SchemeIsCryptographic());

    base::Time request_time =
        transaction_ && transaction_->GetResponseInfo()
            ? transaction_->GetResponseInfo()->request_time
            : base::Time::Now();
    DestroyTransaction();
    override_response_info_ = std::make_unique<HttpResponseInfo>();
    override_response_info_->request_time = request_time;

    override_response_info_->headers = RedirectUtil::SynthesizeRedirectHeaders(
        UpgradeSchemeToCryptographic(request_->url()),
        RedirectUtil::ResponseCode::REDIRECT_307_TEMPORARY_REDIRECT, "DNS",
        request_->extra_request_headers());
    NetLogResponseHeaders(
        request_->net_log(),
        NetLogEventType::URL_REQUEST_FAKE_RESPONSE_HEADERS_CREATED,
        override_response_info_->headers.get());

    NotifyHeadersComplete();
  } else {
    // Even on an error, there may be useful information in the response
    // info (e.g. whether there's a cached copy).
    if (transaction_.get())
      response_info_ = transaction_->GetResponseInfo();
    NotifyStartError(result);
  }
}

void URLRequestHttpJob::OnHeadersReceivedCallback(int result) {
  // The request should not have been cancelled or have already completed.
  DCHECK(!is_done());

  awaiting_callback_ = false;

  SaveCookiesAndNotifyHeadersComplete(result);
}

void URLRequestHttpJob::OnReadCompleted(int result) {
  TRACE_EVENT0(NetTracingCategory(), "URLRequestHttpJob::OnReadCompleted");
  read_in_progress_ = false;

  DCHECK_NE(ERR_IO_PENDING, result);

  if (ShouldFixMismatchedContentLength(result))
    result = OK;

  // EOF or error, done with this job.
  if (result <= 0)
    DoneWithRequest(FINISHED);

  ReadRawDataComplete(result);
}

void URLRequestHttpJob::RestartTransactionWithAuth(
    const AuthCredentials& credentials) {
  DCHECK(!override_response_info_);

  auth_credentials_ = credentials;

  // These will be reset in OnStartCompleted.
  response_info_ = nullptr;
  override_response_headers_ = nullptr;  // See https://crbug.com/801237.
  receive_headers_end_ = base::TimeTicks();

  ResetTimer();

  // Update the cookies, since the cookie store may have been updated from the
  // headers in the 401/407. Since cookies were already appended to
  // extra_headers, we need to strip them out before adding them again.
  request_info_.extra_headers.RemoveHeader(HttpRequestHeaders::kCookie);

  // TODO(https://crbug.com/968327/): This is weird, as all other clearing is at
  // the URLRequest layer. Should this call into URLRequest so it can share
  // logic at that layer with SetAuth()?
  request_->set_maybe_sent_cookies({});
  request_->set_maybe_stored_cookies({});

  if (ShouldAddCookieHeader()) {
    // Since `request_->isolation_info()` hasn't changed, we don't need to
    // recompute the cookie partition key.
    AddCookieHeaderAndStart();
  } else {
    StartTransaction();
  }
}

void URLRequestHttpJob::SetUpload(UploadDataStream* upload) {
  DCHECK(!transaction_.get() && !override_response_info_)
      << "cannot change once started";
  request_info_.upload_data_stream = upload;
}

void URLRequestHttpJob::SetExtraRequestHeaders(
    const HttpRequestHeaders& headers) {
  DCHECK(!transaction_.get() && !override_response_info_)
      << "cannot change once started";
  request_info_.extra_headers.CopyFrom(headers);
}

LoadState URLRequestHttpJob::GetLoadState() const {
  return transaction_.get() ?
      transaction_->GetLoadState() : LOAD_STATE_IDLE;
}

bool URLRequestHttpJob::GetMimeType(std::string* mime_type) const {
  DCHECK(transaction_.get() || override_response_info_);

  if (!response_info_)
    return false;

  HttpResponseHeaders* headers = GetResponseHeaders();
  if (!headers)
    return false;
  return headers->GetMimeType(mime_type);
}

bool URLRequestHttpJob::GetCharset(std::string* charset) {
  DCHECK(transaction_.get() || override_response_info_);

  if (!response_info_)
    return false;

  return GetResponseHeaders()->GetCharset(charset);
}

void URLRequestHttpJob::GetResponseInfo(HttpResponseInfo* info) {
  if (override_response_info_) {
    DCHECK(!transaction_.get());
    *info = *override_response_info_;
    return;
  }

  if (response_info_) {
    DCHECK(transaction_.get());

    *info = *response_info_;
    if (override_response_headers_.get())
      info->headers = override_response_headers_;
  }
}

void URLRequestHttpJob::GetLoadTimingInfo(
    LoadTimingInfo* load_timing_info) const {
  // If haven't made it far enough to receive any headers, don't return
  // anything. This makes for more consistent behavior in the case of errors.
  if (!transaction_ || receive_headers_end_.is_null())
    return;
  if (transaction_->GetLoadTimingInfo(load_timing_info))
    load_timing_info->receive_headers_end = receive_headers_end_;
}

bool URLRequestHttpJob::GetTransactionRemoteEndpoint(
    IPEndPoint* endpoint) const {
  if (!transaction_)
    return false;

  return transaction_->GetRemoteEndpoint(endpoint);
}

int URLRequestHttpJob::GetResponseCode() const {
  DCHECK(transaction_.get());

  if (!response_info_)
    return -1;

  return GetResponseHeaders()->response_code();
}

void URLRequestHttpJob::PopulateNetErrorDetails(
    NetErrorDetails* details) const {
  if (!transaction_)
    return;
  return transaction_->PopulateNetErrorDetails(details);
}

std::unique_ptr<SourceStream> URLRequestHttpJob::SetUpSourceStream() {
  DCHECK(transaction_.get());
  if (!response_info_)
    return nullptr;

  std::unique_ptr<SourceStream> upstream = URLRequestJob::SetUpSourceStream();
  HttpResponseHeaders* headers = GetResponseHeaders();
  std::vector<SourceStream::SourceType> types;
  size_t iter = 0;
  for (std::string type;
       headers->EnumerateHeader(&iter, "Content-Encoding", &type);) {
    SourceStream::SourceType source_type =
        FilterSourceStream::ParseEncodingType(type);
    switch (source_type) {
      case SourceStream::TYPE_BROTLI:
      case SourceStream::TYPE_DEFLATE:
      case SourceStream::TYPE_GZIP:
        if (request_->accepted_stream_types() &&
            !request_->accepted_stream_types()->contains(source_type)) {
          // If the source type is disabled, we treat it
          // in the same way as SourceStream::TYPE_UNKNOWN.
          return upstream;
        }
        types.push_back(source_type);
        break;
      case SourceStream::TYPE_NONE:
        // Identity encoding type. Pass through raw response body.
        return upstream;
      case SourceStream::TYPE_UNKNOWN:
        // Unknown encoding type. Pass through raw response body.
        // Request will not be canceled; though
        // it is expected that user will see malformed / garbage response.
        return upstream;
    }
  }

  for (const auto& type : base::Reversed(types)) {
    std::unique_ptr<FilterSourceStream> downstream;
    switch (type) {
      case SourceStream::TYPE_BROTLI:
        downstream = CreateBrotliSourceStream(std::move(upstream));
        break;
      case SourceStream::TYPE_GZIP:
      case SourceStream::TYPE_DEFLATE:
        downstream = GzipSourceStream::Create(std::move(upstream), type);
        break;
      case SourceStream::TYPE_NONE:
      case SourceStream::TYPE_UNKNOWN:
        NOTREACHED();
        return nullptr;
    }
    if (downstream == nullptr)
      return nullptr;
    upstream = std::move(downstream);
  }

  return upstream;
}

bool URLRequestHttpJob::CopyFragmentOnRedirect(const GURL& location) const {
  // Allow modification of reference fragments by default, unless
  // |preserve_fragment_on_redirect_url_| is set and equal to the redirect URL.
  return !preserve_fragment_on_redirect_url_.has_value() ||
         preserve_fragment_on_redirect_url_ != location;
}

bool URLRequestHttpJob::IsSafeRedirect(const GURL& location) {
  // HTTP is always safe.
  // TODO(pauljensen): Remove once crbug.com/146591 is fixed.
  if (location.is_valid() &&
      (location.scheme() == "http" || location.scheme() == "https")) {
    return true;
  }
  // Query URLRequestJobFactory as to whether |location| would be safe to
  // redirect to.
  return request_->context()->job_factory() &&
      request_->context()->job_factory()->IsSafeRedirectTarget(location);
}

bool URLRequestHttpJob::NeedsAuth() {
  int code = GetResponseCode();
  if (code == -1)
    return false;

  // Check if we need either Proxy or WWW Authentication. This could happen
  // because we either provided no auth info, or provided incorrect info.
  switch (code) {
    case 407:
      if (proxy_auth_state_ == AUTH_STATE_CANCELED)
        return false;
      proxy_auth_state_ = AUTH_STATE_NEED_AUTH;
      return true;
    case 401:
      if (server_auth_state_ == AUTH_STATE_CANCELED)
        return false;
      server_auth_state_ = AUTH_STATE_NEED_AUTH;
      return true;
  }
  return false;
}

std::unique_ptr<AuthChallengeInfo> URLRequestHttpJob::GetAuthChallengeInfo() {
  DCHECK(transaction_.get());
  DCHECK(response_info_);

  // sanity checks:
  DCHECK(proxy_auth_state_ == AUTH_STATE_NEED_AUTH ||
         server_auth_state_ == AUTH_STATE_NEED_AUTH);
  DCHECK((GetResponseHeaders()->response_code() == HTTP_UNAUTHORIZED) ||
         (GetResponseHeaders()->response_code() ==
          HTTP_PROXY_AUTHENTICATION_REQUIRED));

  if (!response_info_->auth_challenge.has_value())
    return nullptr;
  return std::make_unique<AuthChallengeInfo>(
      response_info_->auth_challenge.value());
}

void URLRequestHttpJob::SetAuth(const AuthCredentials& credentials) {
  DCHECK(transaction_.get());

  // Proxy gets set first, then WWW.
  if (proxy_auth_state_ == AUTH_STATE_NEED_AUTH) {
    proxy_auth_state_ = AUTH_STATE_HAVE_AUTH;
  } else {
    DCHECK_EQ(server_auth_state_, AUTH_STATE_NEED_AUTH);
    server_auth_state_ = AUTH_STATE_HAVE_AUTH;
  }

  RestartTransactionWithAuth(credentials);
}

void URLRequestHttpJob::CancelAuth() {
  if (proxy_auth_state_ == AUTH_STATE_NEED_AUTH) {
    proxy_auth_state_ = AUTH_STATE_CANCELED;
  } else {
    DCHECK_EQ(server_auth_state_, AUTH_STATE_NEED_AUTH);
    server_auth_state_ = AUTH_STATE_CANCELED;
  }

  // The above lines should ensure this is the case.
  DCHECK(!NeedsAuth());

  // Let the consumer read the HTTP error page. NeedsAuth() should now return
  // false, so NotifyHeadersComplete() should not request auth from the client
  // again.
  //
  // Have to do this via PostTask to avoid re-entrantly calling into the
  // consumer.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&URLRequestHttpJob::NotifyFinalHeadersReceived,
                                weak_factory_.GetWeakPtr()));
}

void URLRequestHttpJob::ContinueWithCertificate(
    scoped_refptr<X509Certificate> client_cert,
    scoped_refptr<SSLPrivateKey> client_private_key) {
  DCHECK(transaction_);

  DCHECK(!response_info_) << "should not have a response yet";
  DCHECK(!override_response_headers_);
  receive_headers_end_ = base::TimeTicks();

  ResetTimer();

  int rv = transaction_->RestartWithCertificate(
      std::move(client_cert), std::move(client_private_key),
      base::BindOnce(&URLRequestHttpJob::OnStartCompleted,
                     base::Unretained(this)));
  if (rv == ERR_IO_PENDING)
    return;

  // The transaction started synchronously, but we need to notify the
  // URLRequest delegate via the message loop.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&URLRequestHttpJob::OnStartCompleted,
                                weak_factory_.GetWeakPtr(), rv));
}

void URLRequestHttpJob::ContinueDespiteLastError() {
  // If the transaction was destroyed, then the job was cancelled.
  if (!transaction_.get())
    return;

  DCHECK(!response_info_) << "should not have a response yet";
  DCHECK(!override_response_headers_);
  receive_headers_end_ = base::TimeTicks();

  ResetTimer();

  int rv = transaction_->RestartIgnoringLastError(base::BindOnce(
      &URLRequestHttpJob::OnStartCompleted, base::Unretained(this)));
  if (rv == ERR_IO_PENDING)
    return;

  // The transaction started synchronously, but we need to notify the
  // URLRequest delegate via the message loop.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&URLRequestHttpJob::OnStartCompleted,
                                weak_factory_.GetWeakPtr(), rv));
}

bool URLRequestHttpJob::ShouldFixMismatchedContentLength(int rv) const {
  // Some servers send the body compressed, but specify the content length as
  // the uncompressed size. Although this violates the HTTP spec we want to
  // support it (as IE and FireFox do), but *only* for an exact match.
  // See http://crbug.com/79694.
  if (rv == ERR_CONTENT_LENGTH_MISMATCH ||
      rv == ERR_INCOMPLETE_CHUNKED_ENCODING) {
    if (request_->response_headers()) {
      int64_t expected_length =
          request_->response_headers()->GetContentLength();
      VLOG(1) << __func__ << "() \"" << request_->url().spec() << "\""
              << " content-length = " << expected_length
              << " pre total = " << prefilter_bytes_read()
              << " post total = " << postfilter_bytes_read();
      if (postfilter_bytes_read() == expected_length) {
        // Clear the error.
        return true;
      }
    }
  }
  return false;
}

int URLRequestHttpJob::ReadRawData(IOBuffer* buf, int buf_size) {
  DCHECK_NE(buf_size, 0);
  DCHECK(!read_in_progress_);

  int rv =
      transaction_->Read(buf, buf_size,
                         base::BindOnce(&URLRequestHttpJob::OnReadCompleted,
                                        base::Unretained(this)));

  if (ShouldFixMismatchedContentLength(rv))
    rv = OK;

  if (rv == 0 || (rv < 0 && rv != ERR_IO_PENDING))
    DoneWithRequest(FINISHED);

  if (rv == ERR_IO_PENDING)
    read_in_progress_ = true;

  return rv;
}

int64_t URLRequestHttpJob::GetTotalReceivedBytes() const {
  int64_t total_received_bytes =
      total_received_bytes_from_previous_transactions_;
  if (transaction_)
    total_received_bytes += transaction_->GetTotalReceivedBytes();
  return total_received_bytes;
}

int64_t URLRequestHttpJob::GetTotalSentBytes() const {
  int64_t total_sent_bytes = total_sent_bytes_from_previous_transactions_;
  if (transaction_)
    total_sent_bytes += transaction_->GetTotalSentBytes();
  return total_sent_bytes;
}

void URLRequestHttpJob::DoneReading() {
  if (transaction_) {
    transaction_->DoneReading();
  }
  DoneWithRequest(FINISHED);
}

void URLRequestHttpJob::DoneReadingRedirectResponse() {
  if (transaction_) {
    DCHECK(!override_response_info_);
    if (transaction_->GetResponseInfo()->headers->IsRedirect(nullptr)) {
      // If the original headers indicate a redirect, go ahead and cache the
      // response, even if the |override_response_headers_| are a redirect to
      // another location.
      transaction_->DoneReading();
    } else {
      // Otherwise, |override_response_headers_| must be non-NULL and contain
      // bogus headers indicating a redirect.
      DCHECK(override_response_headers_.get());
      DCHECK(override_response_headers_->IsRedirect(nullptr));
      transaction_->StopCaching();
    }
  }
  DoneWithRequest(FINISHED);
}

IPEndPoint URLRequestHttpJob::GetResponseRemoteEndpoint() const {
  return response_info_ ? response_info_->remote_endpoint : IPEndPoint();
}

void URLRequestHttpJob::RecordTimer() {
  if (request_creation_time_.is_null()) {
    NOTREACHED()
        << "The same transaction shouldn't start twice without new timing.";
    return;
  }

  base::TimeDelta to_start = base::Time::Now() - request_creation_time_;
  request_creation_time_ = base::Time();

  UMA_HISTOGRAM_MEDIUM_TIMES("Net.HttpTimeToFirstByte", to_start);

  // Record additional metrics for TLS 1.3 servers. This is to help measure the
  // impact of enabling 0-RTT. The effects of 0-RTT will be muted because not
  // all TLS 1.3 servers enable 0-RTT, and only the first round-trip on a
  // connection makes use of 0-RTT. However, 0-RTT can affect how requests are
  // bound to connections and which connections offer resumption. We look at all
  // TLS 1.3 responses for an apples-to-apples comparison.
  //
  // Additionally record metrics for Google hosts. Most Google hosts are known
  // to implement 0-RTT, so this gives more targeted metrics as we initially
  // roll out client support.
  //
  // TODO(https://crbug.com/641225): Remove these metrics after launching 0-RTT.
  if (transaction_ && transaction_->GetResponseInfo() &&
      IsTLS13OverTCP(*transaction_->GetResponseInfo())) {
    base::UmaHistogramMediumTimes("Net.HttpTimeToFirstByte.TLS13", to_start);
    if (HasGoogleHost(request()->url())) {
      base::UmaHistogramMediumTimes("Net.HttpTimeToFirstByte.TLS13.Google",
                                    to_start);
    }
  }
}

void URLRequestHttpJob::ResetTimer() {
  if (!request_creation_time_.is_null()) {
    NOTREACHED()
        << "The timer was reset before it was recorded.";
    return;
  }
  request_creation_time_ = base::Time::Now();
}

void URLRequestHttpJob::SetRequestHeadersCallback(
    RequestHeadersCallback callback) {
  DCHECK(!transaction_);
  DCHECK(!request_headers_callback_);
  request_headers_callback_ = std::move(callback);
}

void URLRequestHttpJob::SetEarlyResponseHeadersCallback(
    ResponseHeadersCallback callback) {
  DCHECK(!transaction_);
  DCHECK(!early_response_headers_callback_);
  early_response_headers_callback_ = std::move(callback);
}

void URLRequestHttpJob::SetResponseHeadersCallback(
    ResponseHeadersCallback callback) {
  DCHECK(!transaction_);
  DCHECK(!response_headers_callback_);
  response_headers_callback_ = std::move(callback);
}

void URLRequestHttpJob::RecordCompletionHistograms(CompletionCause reason) {
  if (start_time_.is_null())
    return;

  base::TimeDelta total_time = base::TimeTicks::Now() - start_time_;
  UMA_HISTOGRAM_TIMES("Net.HttpJob.TotalTime", total_time);

  if (reason == FINISHED) {
    UmaHistogramTimes(
        base::StringPrintf("Net.HttpJob.TotalTimeSuccess.Priority%d",
                           request()->priority()),
        total_time);
    UMA_HISTOGRAM_TIMES("Net.HttpJob.TotalTimeSuccess", total_time);
  } else {
    UMA_HISTOGRAM_TIMES("Net.HttpJob.TotalTimeCancel", total_time);
  }

  if (response_info_) {
    // QUIC (by default) supports https scheme only, thus track https URLs only
    // for QUIC.
    bool is_https_google = request() && request()->url().SchemeIs("https") &&
                           HasGoogleHost(request()->url());
    bool used_quic = response_info_->DidUseQuic();
    if (is_https_google) {
      if (used_quic) {
        UMA_HISTOGRAM_MEDIUM_TIMES("Net.HttpJob.TotalTime.Secure.Quic",
                                   total_time);
      }
    }

    // Record metrics for TLS 1.3 to measure the impact of 0-RTT. See comment in
    // RecordTimer().
    //
    // TODO(https://crbug.com/641225): Remove these metrics after launching
    // 0-RTT.
    if (IsTLS13OverTCP(*response_info_)) {
      base::UmaHistogramTimes("Net.HttpJob.TotalTime.TLS13", total_time);
      if (is_https_google) {
        base::UmaHistogramTimes("Net.HttpJob.TotalTime.TLS13.Google",
                                total_time);
      }
    }

    UMA_HISTOGRAM_CUSTOM_COUNTS("Net.HttpJob.PrefilterBytesRead",
                                prefilter_bytes_read(), 1, 50000000, 50);
    if (response_info_->was_cached) {
      UMA_HISTOGRAM_TIMES("Net.HttpJob.TotalTimeCached", total_time);
      UMA_HISTOGRAM_CUSTOM_COUNTS("Net.HttpJob.PrefilterBytesRead.Cache",
                                  prefilter_bytes_read(), 1, 50000000, 50);

      if (response_info_->unused_since_prefetch)
        UMA_HISTOGRAM_COUNTS_1M("Net.Prefetch.HitBytes",
                                prefilter_bytes_read());
    } else {
      UMA_HISTOGRAM_TIMES("Net.HttpJob.TotalTimeNotCached", total_time);
      UMA_HISTOGRAM_CUSTOM_COUNTS("Net.HttpJob.PrefilterBytesRead.Net",
                                  prefilter_bytes_read(), 1, 50000000, 50);

      if (request_info_.load_flags & LOAD_PREFETCH) {
        UMA_HISTOGRAM_COUNTS_1M("Net.Prefetch.PrefilterBytesReadFromNetwork",
                                prefilter_bytes_read());
      }
      if (is_https_google) {
        if (used_quic) {
          UMA_HISTOGRAM_MEDIUM_TIMES(
              "Net.HttpJob.TotalTimeNotCached.Secure.Quic", total_time);
        } else {
          UMA_HISTOGRAM_MEDIUM_TIMES(
              "Net.HttpJob.TotalTimeNotCached.Secure.NotQuic", total_time);
        }
      }
    }
  }

  start_time_ = base::TimeTicks();
}

void URLRequestHttpJob::DoneWithRequest(CompletionCause reason) {
  if (done_)
    return;
  done_ = true;

  // Notify NetworkQualityEstimator.
  NetworkQualityEstimator* network_quality_estimator =
      request()->context()->network_quality_estimator();
  if (network_quality_estimator) {
    network_quality_estimator->NotifyRequestCompleted(*request());
  }

  RecordCompletionHistograms(reason);
  request()->set_received_response_content_length(prefilter_bytes_read());
}

HttpResponseHeaders* URLRequestHttpJob::GetResponseHeaders() const {
  if (override_response_info_) {
    DCHECK(!transaction_.get());
    return override_response_info_->headers.get();
  }

  DCHECK(transaction_.get());
  DCHECK(transaction_->GetResponseInfo());

  return override_response_headers_.get() ?
             override_response_headers_.get() :
             transaction_->GetResponseInfo()->headers.get();
}

void URLRequestHttpJob::NotifyURLRequestDestroyed() {
  awaiting_callback_ = false;

  // Notify NetworkQualityEstimator.
  NetworkQualityEstimator* network_quality_estimator =
      request()->context()->network_quality_estimator();
  if (network_quality_estimator)
    network_quality_estimator->NotifyURLRequestDestroyed(*request());
}

bool URLRequestHttpJob::ShouldAddCookieHeader() const {
  // Read cookies whenever allow_credentials() is true, even if the PrivacyMode
  // is being overridden by NetworkDelegate and will eventually block them, as
  // blocked cookies still need to be logged in that case.
  return request_->context()->cookie_store() && request_->allow_credentials();
}

bool URLRequestHttpJob::IsPartitionedCookiesEnabled() const {
  // Only valid to call this after we've computed the key.
  DCHECK(cookie_partition_key_.has_value());
  return cookie_partition_key_.value().has_value();
}

}  // namespace net
