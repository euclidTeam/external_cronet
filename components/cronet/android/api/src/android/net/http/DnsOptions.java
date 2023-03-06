// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package android.net.http;

import android.annotation.IntDef;
import android.annotation.SuppressLint;
import android.os.Build.VERSION_CODES;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.RequiresApi;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.time.Duration;

/**
 * A class configuring the host resolution functionality. Note that while we refer to {@code
 * DNS} as the most common mechanism being used for brevity, settings apply to other means of
 * resolving hostnames like hosts file resolution.
 *
 * <p>Hostnames can be resolved in two ways - either by using the system resolver (using {@code
 * getaddrinfo} provided by system libraries), or by using a custom resolver which is tailored
 * for the HTTP networking stack.
 *
 * <p>The built-in stack provides several advantages over using the global system resolver:
 *
 * <ul>
 *   <li>It has been tailored to the needs of the HTTP networking stack, particularly speed and
 *       stability.
 *   <li>{@code getaddrinfo} is a blocking call which requires dedicating worker threads and makes
 *       cancellation impossible (we need to abandon the thread until the call completes)
 *   <li>The {@code getaddrinfo} interface gives no insight into the root cause of failures
 *   <li>{@code struct addrinfo} provides no TTL (Time To Live) of the returned addresses. This
 *       restricts flexibility of handling caching (e.g. allowing the use of stale DNS records) and
 *       requires us to either rely on OS DNS caches, or be extremely conservative with the TTL.
 * </ul>
 *
 * <p>Most configuration in this class is only applicable if the built-in DNS resolver is used.
 */
// SuppressLint to be consistent with other cronet code
@SuppressLint("UserHandleName")
public final class DnsOptions {
    @Nullable
    private final Boolean mUseHttpStackDnsResolver;
    @Nullable
    private final Boolean mPersistHostCache;
    @Nullable
    private final Boolean mEnableStaleDns;
    @Nullable
    private final Duration mPersistHostCachePeriod;

    @Nullable
    private final Boolean mPreestablishConnectionsToStaleDnsResults;
    @Nullable
    private final StaleDnsOptions mStaleDnsOptions;

    DnsOptions(Builder builder) {
        this.mEnableStaleDns = builder.mEnableStaleDns;
        this.mStaleDnsOptions = builder.mStaleDnsOptions;
        this.mPersistHostCachePeriod = builder.mPersistHostCachePeriod;
        this.mPreestablishConnectionsToStaleDnsResults =
                builder.mPreestablishConnectionsToStaleDnsResults;
        this.mUseHttpStackDnsResolver = builder.mUseHttpStackDnsResolver;
        this.mPersistHostCache = builder.mPersistHostCache;
    }

    /**
     * Option is enabled.
     */
    public static final int DNS_OPTION_ENABLED = 0;

    /**
     * Option is disabled.
     */
    public static final int DNS_OPTION_DISABLED = 1;

    /**
     * Option is unspecified.
     */
    public static final int DNS_OPTION_UNSPECIFIED = 2;

    /** @hide */
    @Retention(RetentionPolicy.SOURCE)
    @IntDef(flag = false, prefix = "DNS_OPTION_", value = {
            DNS_OPTION_ENABLED,
            DNS_OPTION_DISABLED,
            DNS_OPTION_UNSPECIFIED,
    })
    public @interface DnsOptionEnabled {}

    private static @DnsOptionEnabled int getDnsOptionEnabled(Boolean enabled) {
        if (enabled == null) {
            return DNS_OPTION_UNSPECIFIED;
        } else if (enabled) {
            return DNS_OPTION_ENABLED;
        } else {
            return DNS_OPTION_DISABLED;
        }
    }

    /**
     * See {@link Builder#setUseHttpStackDnsResolverEnabled}
     */
    public @DnsOptionEnabled int isUseHttpStackDnsResolverEnabled() {
        return getDnsOptionEnabled(mUseHttpStackDnsResolver);
    }

    /**
     * See {@link Builder#setHostCachePersistent(boolean)}
     */
    public @DnsOptionEnabled int isHostCachePersistent() {
        return getDnsOptionEnabled(mPersistHostCache);
    }

    /**
     * See {@link Builder#setStaleDnsEnabled}
     */
    @Experimental
    public @DnsOptionEnabled int isStaleDnsEnabled() {
        return getDnsOptionEnabled(mEnableStaleDns);
    }

    /**
     * See {@link Builder#setPersistHostCachePeriod}
     */
    @Nullable
    public Duration getPersistHostCachePeriod() {
        return mPersistHostCachePeriod;
    }

    /**
     * See {@link Builder#setPreestablishConnectionsToStaleDnsResultsEnabled}
     */
    @Experimental
    public @DnsOptionEnabled int isPreestablishConnectionsToStaleDnsResultsEnabled() {
        return getDnsOptionEnabled(mPreestablishConnectionsToStaleDnsResults);
    }

    /**
     * See {@link Builder#setStaleDnsOptions}
     */
    @Experimental
    @Nullable
    public StaleDnsOptions getStaleDnsOptions() {
        return mStaleDnsOptions;
    }

    /**
     * Returns a new builder for {@link DnsOptions}.
     *
     * @hide
     */
    @NonNull
    public static Builder builder() {
        return new Builder();
    }

    /**
     * A class configuring the stale DNS functionality.
     *
     * <p>DNS resolution is one of the steps on the critical path to making a URL request, but it
     * can be slow for various reasons (underlying network latency, buffer bloat, packet loss,
     * etc.). Depending on the use case, it might be worthwhile for an app developer to trade off
     * guaranteed DNS record freshness for better availability of DNS records.
     *
     * <p>Stale results can include both:
     *
     * <ul>
     *   <li>results returned from the current network's DNS server, but past their time-to-live,
     *       and
     *   <li>results returned from a different network's DNS server, whether expired or not.
     * </ul>
     *
     * <p>For detailed explanation of the configuration options see javadoc on
     * {@link StaleDnsOptions.Builder} methods.
     */
    // SuppressLint to be consistent with other cronet code
    @Experimental @SuppressLint("UserHandleName")
    public static class StaleDnsOptions {
        @Nullable
        public Duration getFreshLookupTimeout() {
            return mFreshLookupTimeout;
        }

        @Nullable
        public Duration getMaxExpiredDelay() {
            return mMaxExpiredDelay;
        }

        public @DnsOptionEnabled int isCrossNetworkUsageAllowed() {
            return getDnsOptionEnabled(mAllowCrossNetworkUsage);
        }
        public @DnsOptionEnabled int isUseStaleOnNameNotResolvedEnabled() {
            return getDnsOptionEnabled(mUseStaleOnNameNotResolved);
        }

        /**
         * @hide
         */
        @NonNull
        public static Builder builder() {
            return new Builder();
        }

        @Nullable
        private final Duration mFreshLookupTimeout;
        @Nullable
        private final Duration mMaxExpiredDelay;
        @Nullable
        private final Boolean mAllowCrossNetworkUsage;
        @Nullable
        private final Boolean mUseStaleOnNameNotResolved;

        StaleDnsOptions(@NonNull Builder builder) {
            this.mFreshLookupTimeout = builder.mFreshLookupTimeout;
            this.mMaxExpiredDelay = builder.mMaxExpiredDelay;
            this.mAllowCrossNetworkUsage = builder.mAllowCrossNetworkUsage;
            this.mUseStaleOnNameNotResolved = builder.mUseStaleOnNameNotResolved;
        }

        /**
         * Builder for {@link StaleDnsOptions}.
         */
        public static final class Builder {
            private Duration mFreshLookupTimeout;
            private Duration mMaxExpiredDelay;
            private Boolean mAllowCrossNetworkUsage;
            private Boolean mUseStaleOnNameNotResolved;

            public Builder() {}

            /**
             * Sets how long (in milliseconds) to wait for a DNS request to return before using a
             * stale result instead. If set to zero, returns stale results instantly but continues
             * the DNS request in the background to update the cache.
             *
             * @return the builder for chaining
             */
            @NonNull
            public Builder setFreshLookupTimeout(@NonNull Duration freshLookupTimeout) {
                this.mFreshLookupTimeout = freshLookupTimeout;
                return this;
            }

            /**
             * Sets how long (in milliseconds) past expiration to consider using expired results.
             * Setting the value to zero means expired records can be used indefinitely.
             *
             * @return the builder for chaining
             */
            @NonNull
            public Builder setMaxExpiredDelay(@NonNull Duration maxExpiredDelay) {
                this.mMaxExpiredDelay = maxExpiredDelay;
                return this;
            }

            /**
             * Sets whether to return results originating from other networks or not. Normally,
             * the HTTP stack clears the DNS cache entirely when switching connections, e.g. between
             * two Wi-Fi networks or from Wi-Fi to 4G.
             *
             * @return the builder for chaining
             */
            @NonNull
            public Builder setCrossNetworkUsageAllowed(boolean allowCrossNetworkUsage) {
                this.mAllowCrossNetworkUsage = allowCrossNetworkUsage;
                return this;
            }

            /**
             * Sets whether to allow use of stale DNS results when network resolver fails to resolve
             * the hostname.
             *
             * <p>Depending on the use case, if the DNS resolver quickly sees a fresh failure, it
             * may be desirable to use the failure as it is technically the fresher result, and we
             * had such a fresh result quickly; or, prefer having any result (even if stale) to use
             * over dealing with a DNS failure.
             *
             * @return the builder for chaining
             */
            @NonNull
            public Builder setUseStaleOnNameNotResolvedEnabled(boolean useStaleOnNameNotResolved) {
                this.mUseStaleOnNameNotResolved = useStaleOnNameNotResolved;
                return this;
            }

            /**
             * Creates and returns the final {@link StaleDnsOptions} instance, based on the values
             * in this builder.
             */
            @NonNull
            public StaleDnsOptions build() {
                return new StaleDnsOptions(this);
            }
        }
    }

    /**
     * Builder for {@link DnsOptions}.
     */
    public static final class Builder {
        @Nullable
        private Boolean mUseHttpStackDnsResolver;
        @Nullable
        private Boolean mEnableStaleDns;
        @Nullable
        private StaleDnsOptions mStaleDnsOptions;
        @Nullable
        private Boolean mPersistHostCache;
        @Nullable
        private Duration mPersistHostCachePeriod;
        @Nullable
        private Boolean mPreestablishConnectionsToStaleDnsResults;

        public Builder() {}

        /**
         * Enables the use of the HTTP-stack-specific DNS resolver.
         *
         * <p>Setting this to {@code true} is necessary for other functionality
         * of {@link DnsOptions} to work, unless specified otherwise. See the {@link DnsOptions}
         * documentation for more details.
         */
        @NonNull
        public Builder setUseHttpStackDnsResolverEnabled(boolean enable) {
            this.mUseHttpStackDnsResolver = enable;
            return this;
        }

        /**
         * Sets whether to use stale DNS results at all.
         *
         * @return the builder for chaining
         */
        @Experimental
        @NonNull
        public Builder setStaleDnsEnabled(boolean enable) {
            this.mEnableStaleDns = enable;
            return this;
        }

        /**
         * Sets detailed configuration for stale DNS.
         *
         * Only relevant if {@link #setStaleDnsEnabled(boolean)} is set.
         *
         * @return this builder for chaining.
         */
        @Experimental
        @NonNull
        public Builder setStaleDnsOptions(@NonNull StaleDnsOptions staleDnsOptions) {
            this.mStaleDnsOptions = staleDnsOptions;
            return this;
        }

        /**
         * @see #setStaleDnsOptions(StaleDnsOptions)
         *
         * {@hide}
         */
        @Experimental
        @NonNull
        public Builder setStaleDnsOptions(@NonNull StaleDnsOptions.Builder staleDnsOptionsBuilder) {
            return setStaleDnsOptions(staleDnsOptionsBuilder.build());
        }

        /**
         * Sets whether Cronet should use stale cached DNS records to pre-establish connections.
         *
         * <p>If enabled, Cronet will optimistically pre-establish connections to servers that
         * matched the hostname at some point in the past and were cached but the cache entry
         * expired. Such connections won't be used further until a new DNS lookup confirms the
         * cached record was up to date.
         *
         * <p>To use cached DNS records straight away, use {@link #setStaleDnsEnabled} and {@link
         * StaleDnsOptions} configuration options.
         *
         * <p>This option may not be available for all networking protocols.
         *
         * @return the builder for chaining
         */
        @Experimental
        @NonNull
        public Builder setPreestablishConnectionsToStaleDnsResultsEnabled(boolean enable) {
            this.mPreestablishConnectionsToStaleDnsResults = enable;
            return this;
        }

        /**
         * Sets whether the DNS cache should be persisted to disk.
         *
         * <p>Only relevant if {@link HttpEngine.Builder#setStoragePath(String)} is
         * set.
         *
         * @return the builder for chaining
         */
        @NonNull
        public Builder setHostCachePersistent(boolean persistHostCache) {
            this.mPersistHostCache = persistHostCache;
            return this;
        }

        /**
         * Sets the minimum period between subsequent writes to disk for DNS cache persistence.
         *
         * <p>Only relevant if {@link #setHostCachePersistent(boolean)} is set to true.
         *
         * @return the builder for chaining
         */
        @NonNull
        public Builder setPersistHostCachePeriod(@NonNull Duration persistHostCachePeriod) {
            this.mPersistHostCachePeriod = persistHostCachePeriod;
            return this;
        }

        /**
         * Creates and returns the final {@link DnsOptions} instance, based on the values in this
         * builder.
         */
        @NonNull
        public DnsOptions build() {
            return new DnsOptions(this);
        }
    }

    /**
     * An annotation for APIs which are not considered stable yet.
     *
     * <p>Applications using experimental APIs must acknowledge that they're aware of using APIs
     * that are not considered stable. The APIs might change functionality, break or cease to exist
     * without notice.
     *
     * <p>It's highly recommended to reach out to Cronet maintainers ({@code net-dev@chromium.org})
     * before using one of the APIs annotated as experimental outside of debugging
     * and proof-of-concept code. Be ready to help to help polishing the API, or for a "sorry,
     * really not production ready yet".
     *
     * <p>If you still want to use an experimental API in production, you're doing so at your
     * own risk. You have been warned.
     *
     * {@hide}
     */
    public @interface Experimental {}
}